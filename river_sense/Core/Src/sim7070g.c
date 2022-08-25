/*
 * sim7070g.c
 *
 *  Created on: Aug 20, 2022
 *      Author: benjr
 */

#include "main.h"
#include "stm32l4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sim7070g.h>

// External variables begin
extern UART_HandleTypeDef huart1;
extern volatile uint8_t isLD3_Flicker;
// External variables end

simStateType volatile simState = SIM_INIT;
uint8_t shouldTransmit = 1;
char ATcommand[80];
volatile uint16_t commandIndex = 0;
volatile uint8_t isStateChanged = 0;
uint8_t serialRX_Buffer[250] = {0};
uint8_t serialRX_BufferIndex = 0;
uint8_t serialRX_Data;
uint8_t clearBuffer = 0;

// HTTP variables
char responseSubstring[50] = {0};
uint16_t statusCodeVal = 0;
uint16_t dataLengthVal = 0;
volatile uint8_t SHREAD_Flag = 0;
char SHREAD_Data[100] = {0};


// Buffer to store all data. TODO: remove later
uint8_t allRX_Data[1000] = {0};
uint8_t allRX_Data_Index = 0;


void SIM_Handler(void) {

	switch (simState) {
		case SIM_INIT:
			SIM_Init();
			break;
		case SIM_UE_CHECK:
			SIM_UE_Check();
			break;
		case SIM_PDN_ACTIVATION:
			SIM_PDN_Activation();
			break;
		case SIM_HTTP_POST_BUILD:
			SIM_HTTP_Post_Build();
			break;
		case SIM_HTTP_MAKE_POST:
			SIM_HTTP_Make_Post();
			break;
		default:
			break;
	}


	return;
}


void SIM_serialRX_Handler(uint8_t charReceived) {

	serialRX_Buffer[serialRX_BufferIndex++] = charReceived;

	// Remove later; Just for tracking
	allRX_Data[allRX_Data_Index++] = charReceived;



	if ((charReceived == (uint8_t)'\n')) {
		SIM_Handler();
	}


	if (clearBuffer) { // reset index and clear buffer
		serialRX_BufferIndex = 0;
		memset(serialRX_Buffer, 0, sizeof(serialRX_Buffer));
		clearBuffer = 0;
	}


	return;
}


void SIM_Init(void) {

	// ---------------------------- TX ------------------------------- //
	// 0: AT
	// 1: AT+CPIN?
	// All good state change: SIM_INIT to SIM_UE_CHECK

	uint8_t maxCommand = 1;

	if ((shouldTransmit) && (commandIndex <= maxCommand)) {
		// Next command should be transmitted

		switch (commandIndex) {
			case 0:
				// Check connection between stm23 and sim7070g
				sprintf(ATcommand, "AT\r\n");
				// Transmit with interrupt
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				// Enable UART Receive Interrupt
				HAL_UART_Receive_IT(&huart1, &serialRX_Data, 1);
				break;
			case 1:
				// Check SIM card status
				sprintf(ATcommand, "AT+CPIN?\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t*) ATcommand, strlen(ATcommand));
				break;
			default:
				break;
		}

		shouldTransmit = 0; // no transmission until response has been read


	} // if shouldTransmit
	// --------------------------------------------------------------- //

	// ---------------------------- RX ------------------------------- //
	uint8_t isIncrementCommand = 0;

	switch (commandIndex) {
		case 0:
			// OK should be received
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1; // Increment to go to next command
			}

			break;

		case 1:
			// +CPIN:READY\n\nOK\r\n should be received
			if (strstr((char*) serialRX_Buffer, "+CPIN: READY\r\n\r\nOK\r\n")) {
				isIncrementCommand = 1; // Increment to go to next command
				simState = SIM_UE_CHECK;
				isStateChanged = 1;
			}

			break;
		default:
			break;
	}

	if (isIncrementCommand) {
		++commandIndex; // Next command
		clearBuffer = 1; // Clear buffer to receive next response
		shouldTransmit = 1; // Can transmit next command

		isIncrementCommand = 0;
	}

	// --------------------------------------------------------------- //


	if ((commandIndex > maxCommand) || (isStateChanged)) {
		commandIndex = 0;
		isStateChanged = 0;
	}


	return;
}


void SIM_UE_Check(void) {

	// ---------------------------- TX ------------------------------- //
	// 0: AT+CPSI?

	uint8_t maxCommand = 0;

	if ((shouldTransmit) && (commandIndex <= maxCommand)) {
		// Next command should be transmitted
		switch (commandIndex) {
			case 0:
				// Inquire UE system information
				sprintf(ATcommand, "AT+CPSI?\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;
			default:
				break;
		}
		shouldTransmit = 0;
	}
	// --------------------------------------------------------------- //



	// ---------------------------- RX ------------------------------- //
	// If NB-IoT is online, change from SIM_UE_CHECK to SIM_PDN_ACTIVATION
	// If NB-IoT is NOT online, change from SIM_UE_CHECK to SIM_CONNECT_NBIOT
	uint8_t isIncrementCommand = 0;

	switch (commandIndex) {
			case 0:
				// OK should be received
				// +CPSI: LTE NB-IOT,Online should be received
				if ((strstr((char*) serialRX_Buffer, "\nOK\r\n")) && (strstr((char*) serialRX_Buffer, "+CPSI: LTE NB-IOT,Online"))) {
					isIncrementCommand = 1; // Increment to go to next command
					simState = SIM_PDN_ACTIVATION;
					isStateChanged = 1;
				}

				// TODO: If NB-IoT is not online
//				[16:37:45.613] +CPSI: NO SERVICE,Online
//				[16:37:45.613]
//				[16:37:45.613] OK

				break;
			default:
				break;
		}

	if (isIncrementCommand) {
		++commandIndex; // Next command
		clearBuffer = 1; // Clear buffer to receive next response
		shouldTransmit = 1; // Can transmit next command

		isIncrementCommand = 0;
	}
	// --------------------------------------------------------------- //


	if ((commandIndex > maxCommand) || (isStateChanged)) {
		commandIndex = 0;
		isStateChanged = 0;
	}


	return;
}



void SIM_PDN_Activation(void) {

	// ---------------------------- TX ------------------------------- //
	// 0: AT+CGATT? - Check PS service. 1 indicates PS has attached.
	// 1: AT+COPS? - Network information, operator and network
	// 2: AT+CGNAPN - Query APN delivered by the network
	// 3: AT+CNACT? - Get local IP
	// 4: AT+CNACT=0,1 - Activate 0th PDP

	uint8_t maxCommand = 4;

	if ((shouldTransmit) && (commandIndex <= maxCommand)) {
		// Next command should be transmitted
		switch (commandIndex) {
			case 0:
				// Check PS service
				sprintf(ATcommand, "AT+CGATT?\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 1:
				// Network information
				sprintf(ATcommand, "AT+COPS?\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 2:
				// Network APN
				sprintf(ATcommand, "AT+CGNAPN\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 3:
				// Local IP (to check if need to activate network)
				sprintf(ATcommand, "AT+CNACT?\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 4:
				// Activate network, Activate 0th PDP.
				sprintf(ATcommand, "AT+CNACT=0,1\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;


			default:
				break;
		}
		shouldTransmit = 0;
	}
	// --------------------------------------------------------------- //



	// ---------------------------- RX ------------------------------- //

	uint8_t isIncrementCommand = 0;

	switch (commandIndex) {
			case 0:
				// CGATT: 1\r\n\r\nOK\r\n - Has attached
				if ( strstr((char*) serialRX_Buffer, "CGATT: 1\r\n\r\nOK\r\n") ) {
					isIncrementCommand = 1; // Increment to go to next command
				}

				// TODO: If not attached to PS service
				break;

			case 1:
				// +COPS?\r\r\n+COPS: 1,0,\"VodaCom-SA\",9\r\n\r\nOK\r\n - 9: NB-IoT Network
				if (strstr((char*) serialRX_Buffer, "+COPS?\r\r\n+COPS: 1,0,\"VodaCom-SA\",9\r\n\r\nOK\r\n") ) {
					isIncrementCommand = 1;
				}

				// TODO: If network information is wrong
				break;

			case 2:
				// +CGNAPN: 1,\"internet\"\r\n\r\nOK\r\n
				if ( strstr((char*) serialRX_Buffer, "+CGNAPN: 1,\"internet\"\r\n\r\nOK\r\n") ) {
					isIncrementCommand = 1;
				}

			case 3:
				// +CNACT: 0,0,\"0.0.0.0\" - Not activated
				// \r\nOK\r\n - End of response
				if ( (strstr((char*) serialRX_Buffer, "+CNACT: 0,0,\"0.0.0.0\"")) && (strstr((char*) serialRX_Buffer, "\r\nOK\r\n")) ) {
					// Set to correct command for activation; Clear buffer; Enable transmit next command
					commandIndex = 4;
					clearBuffer = 1;
					shouldTransmit = 1;
				}
				//
				// Network is already activated
				// \r\n+CNACT: 0,1, - Activated
				// \r\nOK\r\n - End of response
				else if ( (strstr((char*) serialRX_Buffer, "\r\n+CNACT: 0,1,")) && (strstr((char*) serialRX_Buffer, "\r\nOK\r\n"))) {
					// Network is already activated
					// Change state; Clear buffer; Enable transmit
					simState = SIM_HTTP_POST_BUILD;
					isStateChanged = 1;
					clearBuffer = 1;
					shouldTransmit = 1;
				}
				break;

			case 4:
				// +APP PDP: 0,ACTIVE\r\n - Activated
				if ( strstr((char*) serialRX_Buffer, "+APP PDP: 0,ACTIVE\r\n") ) {
					// Successful network activation
					// Change state; Clear buffer; Enable transmit
					simState = SIM_HTTP_POST_BUILD;
					isStateChanged = 1;
					clearBuffer = 1;
					shouldTransmit = 1;
				}
				break;

			default:
				break;
		}

	if (isIncrementCommand) {
		++commandIndex; // Next command
		clearBuffer = 1; // Clear buffer to receive next response
		shouldTransmit = 1; // Can transmit next command

		isIncrementCommand = 0;
	}
	// --------------------------------------------------------------- //


	if ((commandIndex > maxCommand) || (isStateChanged)) {
		commandIndex = 0;
		isStateChanged = 0;
	}


	return;
}


void SIM_HTTP_Post_Build(void) {

	// ---------------------------- TX ------------------------------- //
	//
	// 0: AT+SHSTATE? - Get HTTP status
	// 1: AT+SHDISC - Disconnect HTTP (only send if connection exists).
	// 2: AT+SHCONF="URL","http://api.thingspeak.com" - Set up server URL
	// 3: AT+SHCONF="BODYLEN",1024 - Set HTTP body length
	// 4: AT+SHCONF="HEADERLEN",350 - Set HTTP head length
	// 5: AT+SHCONN - HTTP build (retry if unsuccessful)
	// Change from SIM_HTTP_POST_BUILD to HTTP_MAKE_POST


	uint8_t maxCommand = 5;

	if ((shouldTransmit) && (commandIndex <= maxCommand)) {
		// Next command should be transmitted

		switch (commandIndex) {
			case 0:
				// Check if HTTP is connected
				sprintf(ATcommand, "AT+SHSTATE?\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 1:
				// Disconnect HTTP
				sprintf(ATcommand, "AT+SHDISC\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 2:
				// Set up server URL
				sprintf(ATcommand, "AT+SHCONF=\"URL\",\"http://api.thingspeak.com\"\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 3:
				// Set HTTP body length
				sprintf(ATcommand, "AT+SHCONF=\"BODYLEN\",1024\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 4:
				// Set HTTP head length
				sprintf(ATcommand, "AT+SHCONF=\"HEADERLEN\",350\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				break;

			case 5:
				// HTTP connect (retry if unsuccessful)
				sprintf(ATcommand, "AT+SHCONN\r\n");
				HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
				isLD3_Flicker = 0; // TODO: remove later
				break;

			default:
				break;
		}

		shouldTransmit = 0; // no transmission until response has been read


	} // if shouldTransmit
	// --------------------------------------------------------------- //

	// ---------------------------- RX ------------------------------- //
	uint8_t isIncrementCommand = 0;

	switch (commandIndex) {
		case 0:
			// +SHSTATE: 0\r\n\r\nOK\r\n - HTTP disconnect state
			if (strstr((char*) serialRX_Buffer, "+SHSTATE: 0\r\n\r\nOK\r\n")) {
				commandIndex = 2; // AT+SHCONF=... next command
				clearBuffer = 1; // Clear buffer to receive next response
				shouldTransmit = 1; // Can transmit next command
			}
			// +SHSTATE: 1\r\n\r\nOK\r\n - HTTP connect state
			else if (strstr((char*) serialRX_Buffer, "+SHSTATE: 1\r\n\r\nOK\r\n")) {
				commandIndex = 1; // AT+SHDISC next command
				clearBuffer = 1; // Clear buffer to receive next response
				shouldTransmit = 1; // Can transmit next command
			}
			break;

		case 1:
			// OK should be received for successful disconnect
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1;
			}
			break;

		case 2:
			// OK should be received. URL setup
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1;
			}
			break;

		case 3:
			// OK should be received. HTTP body length
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1;
			}
			break;

		case 4:
			// OK should be received. HTTP head length
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1; // Increment to go to next command
			}
			break;

		case 5:
			// Connection successful
			if (strstr((char*) serialRX_Buffer, "AT+SHCONN\r\r\nOK\r\n")) {
				simState = SIM_HTTP_MAKE_POST;
				isStateChanged = 1;
				shouldTransmit = 1;
				clearBuffer = 1;

				isLD3_Flicker = 1; // TODO: remove later
			}
			// Connection unsuccessful
			else if (strstr((char*) serialRX_Buffer, "AT+SHCONN\r\r\nERROR\r\n")) {
				// Resends HTTP build command
				clearBuffer = 1;
				shouldTransmit = 1;
			}
			break;

		default:
			break;
	}

	if (isIncrementCommand) {
		++commandIndex; // Next command
		clearBuffer = 1; // Clear buffer to receive next response
		shouldTransmit = 1; // Can transmit next command

		isIncrementCommand = 0;
	}

	// --------------------------------------------------------------- //


	if ((commandIndex > maxCommand) || (isStateChanged)) {
		commandIndex = 0;
		isStateChanged = 0;
	}


	return;
}

void SIM_HTTP_Make_Post(void) {
	// ---------------------------- TX ------------------------------- //
	//
	// 0: AT+SHSTATE? - Get HTTP status
	// 1: AT+SHCHEAD
	// 2: AT+SHAHEAD="Content-Type","application/x-www-form-urlencoded"
	// 3: AT+SHAHEAD="Cache-control","no-cache"
	// 4: AT+SHAHEAD="Connection","keep-alive"
	// 5: AT+SHAHEAD="Accept","*/*"
	// 6: AT+SHREQ="/update?api_key=1EC4ZVYTHEJUAAIO&field2=5",3
	// 7: AT+SHREAD=0,2 // read http result (second variable dependant on result from SHREQ)
	// 8: AT+SHDISC - Disconnect HTTP connect

	uint8_t maxCommand = 8;

	if ((shouldTransmit) && (commandIndex <= maxCommand)) {
		// Next command should be transmitted

		switch (commandIndex) {
			case 0:
				// Get HTTP header
				sprintf(ATcommand, "AT+SHSTATE?\r\n");
				break;

			case 1:
				// Clear HTTP header
				sprintf(ATcommand, "AT+SHCHEAD\r\n");
				break;

			case 2:
				// Add header content
				sprintf(ATcommand, "AT+SHAHEAD=\"Content-Type\",\"application/x-www-form-urlencoded\"\r\n");
				break;

			case 3:
				// Add header content
				sprintf(ATcommand, "AT+SHAHEAD=\"Cache-control\",\"no-cache\"\r\n");
				break;

			case 4:
				// Add header content
				sprintf(ATcommand, "AT+SHAHEAD=\"Connection\",\"keep-alive\"\r\n");
				break;

			case 5:
				// Add header content
				sprintf(ATcommand, "AT+SHAHEAD=\"Accept\",\"*/*\"\r\n");
				break;

			case 6:
				// Add header content
				sprintf(ATcommand, "AT+SHREQ=\"/update?api_key=1EC4ZVYTHEJUAAIO&field1=60\",3\r\nAT\r\n");
				break;

			case 7: ;
				// Read data after request
				char d[4];
				itoa(dataLengthVal, d, 10);
				sprintf(ATcommand, "AT+SHREAD=0,%s\r\n", d);
				break;

			case 8:
				// Disconnect HTTP
				sprintf(ATcommand, "AT+SHDISC\r\n");
				break;

			default:
				break;
		}

		HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand)); // Transmit AT command
		shouldTransmit = 0; // no transmission until response has been read


	} // if shouldTransmit
	// --------------------------------------------------------------- //

	// ---------------------------- RX ------------------------------- //
	uint8_t isIncrementCommand = 0;

	switch (commandIndex) {
		case 0:
			// +SHSTATE: 0\r\n\r\nOK\r\n - HTTP disconnect state
			if (strstr((char*) serialRX_Buffer, "+SHSTATE: 0\r\n\r\nOK\r\n")) {
				// If disconnected, build HTTP post
				simState = SIM_HTTP_POST_BUILD;
				isStateChanged = 1;
				clearBuffer = 1; // Clear buffer to receive next response
				shouldTransmit = 1; // Can transmit next command
			}
			// +SHSTATE: 1\r\n\r\nOK\r\n - HTTP connect state
			else if (strstr((char*) serialRX_Buffer, "+SHSTATE: 1\r\n\r\nOK\r\n")) {
				commandIndex = 1; // AT+SHDISC next command
				clearBuffer = 1; // Clear buffer to receive next response
				shouldTransmit = 1; // Can transmit next command
			}
			break;

		case 1:
			// OK received if header cleared
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1;
			}
			break;

		case 2:
			// OK received if header content was added
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1;
			}
			break;

		case 3:
			// OK received if header content was added
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1;
			}
			break;

		case 4:
			// OK received if header content was added
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1;
			}
			break;

		case 5:
			// OK received if header content was added
			if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
				isIncrementCommand = 1;
			}
			break;

		case 6:
			if (strstr((char*) serialRX_Buffer, "+SHREQ: \"POST\"")) {
				copySubstringFromMatch(responseSubstring, (char*)serialRX_Buffer, "+SHREQ: \"POST\"");
				// +SHREQ: \"POST\",200,2\r\n
				uint8_t splittedValIndex = 0;
				uint16_t splittedVal[4] = {0};
				char* token;
				if (strstr(responseSubstring, "\r\n")) { // response has finished transmitting
					token = strtok(responseSubstring, ",");
					while (token != NULL  && splittedValIndex<4) {
						splittedVal[++splittedValIndex] = atoi(token);
						token = strtok(NULL, ",");
					}
					statusCodeVal = splittedVal[2];
					dataLengthVal = splittedVal[3];
					isIncrementCommand = 1;
					isLD3_Flicker = 0; // TODO: remove later
				}

			}
			break;

		case 7:
			// should only move on to next command after second time that +SHREAD: is matched
			// done to ensure that the full request data has been sent back
			if (strstr((char*) serialRX_Buffer, "+SHREAD:")) {
				// Set flag
				++SHREAD_Flag;
				if (SHREAD_Flag>4) {
					copySubstringFromMatch(SHREAD_Data, (char*)serialRX_Buffer, "+SHREAD:");
					SHREAD_Flag = 0;
					isIncrementCommand = 1;
				}
			}
			break;

		case 8:
			// OK received. Successful disconnect.
			if (strstr((char*) serialRX_Buffer, "+SHDISC\r\r\nOK\r\n")) {
				isIncrementCommand = 1;
				simState = SIM_STANDBY;
				isStateChanged = 1;
				isLD3_Flicker = 1; // TODO: remove later
			}
			break;

		default:
			break;
	}

	if (isIncrementCommand) {
		++commandIndex; // Next command
		clearBuffer = 1; // Clear buffer to receive next response
		shouldTransmit = 1; // Can transmit next command

		isIncrementCommand = 0;
	}

	// --------------------------------------------------------------- //


	if ((commandIndex > maxCommand) || (isStateChanged)) {
		commandIndex = 0;
		isStateChanged = 0;
	}


	return;
}

// No check is implemented for destination that is smaller than the substring
void copySubstringFromMatch(char* destination, char* source, char* strToMatch) {
	char* firstOccurence = strstr(source, strToMatch);
	if (!firstOccurence) { // string to match not found
		return;
	}

	uint16_t startPos = firstOccurence - source;
	uint16_t substringLength = strlen(source) - startPos;

	strncpy(destination, source+startPos, startPos+substringLength);

	return;
}
