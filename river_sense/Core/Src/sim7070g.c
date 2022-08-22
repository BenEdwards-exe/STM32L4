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
#include <sim7070g.h>

// External variables begin
extern UART_HandleTypeDef huart1;

// External variables end

simStateType simState = SIM_INIT;
uint8_t shouldTransmit = 1;
char ATcommand[80];
uint8_t commandIndex = 0;
volatile uint8_t isStateChanged = 0;
uint8_t serialRX_Buffer[250] = {0};
uint8_t serialRX_BufferIndex = 0;
uint8_t serialRX_Data;
uint8_t clearBuffer = 0;


// PDN Auto-activation variables
#define PDN_LENGTH 8
char pdnCommands[PDN_LENGTH][20] = {
		"AT+CPIN?\r\n",
		"AT+CSQ\r\n",
		"AT+CGATT?\r\n",
		"AT+COPS?\r\n",
		"AT+CGNAPN\r\n",
		"AT+CNACT?\r\n",
		"AT+CNACT=0,0\r\n", // deactivate network before activation; FIX LATER
		"AT+CNACT=0,1\r\n", // activate network
//		"AT+CNACT?\r\n" // check again; maybe remove later
};
uint8_t pdnCommandIndex = 0;
uint8_t pdnFeedback[PDN_LENGTH][150] = {0};



// HTTP Post variables
#define HTTP_POST_LENGTH 10
char httpPostCommands[HTTP_POST_LENGTH][80] = {
		"AT+SHCONF=\"URL\",\"http://api.thingspeak.com\"\r\n", // set up server URL
//		"AT+SHCONF=\"BODYLEN\",1024\r\n", // set HTTP body length for range of max body length
//		"AT+SHCONF=\"HEADERLEN\",350\r\n", // set HTTP head length for max range of head length
//		"AT+SHCONF?\r\n", // Check configuration
		"AT+SHCONN\r\n", // HTTP build
		"AT+SHSTATE?\r\n", // Get HTTP status, 1: connected; 0: disconnected
		"AT+SHCHEAD\r\n", // Clear HTTP header, because HTTP header is appended
		"AT+SHAHEAD=\"Content-Type\",\"application/x-www-form-urlencoded\"\r\n", // magic
		"AT+SHAHEAD=\"Cache-control\",\"no-cache\"\r\n", // magic
		"AT+SHAHEAD=\"Connection\",\"keep-alive\"\r\n", // magic
		"AT+SHAHEAD=\"Accept\",\"*/*\"\r\n", // magic
		"AT+SHREQ=\"/update?api_key=1EC4ZVYTHEJUAAIO&field2=30\",3\r\n", // set request type and append header
//		"AT+SHREAD=0,2", // Read http result (second variable dependent on result length)
		"AT+SHDISC\r\n" // Disconnect http connect
};
uint8_t httpPostCommandIndex = 0;
uint8_t httpPostFeedback[HTTP_POST_LENGTH][150] = {0};



uint8_t allRX_Data[1000] = {0};
uint8_t allRX_Data_Index = 0;

void SIM_Handler(void) {

	switch (simState) {
		case SIM_INIT:
			SIM_Init_Handler();
			break;
		case SIM_UE_CHECK:
			SIM_UE_Check_Handler();
			break;
		case SIM_PDN_ACTIVATION:
			SIM_PDN_Activation();
			break;
		default:
			break;
	}



//	if (simState == SIM_INIT) { // Check communication between SIM and computer
//
//		if (shouldTransmit) {
//			sprintf(ATcommand, "AT\r\n");
//			// Transmit with interrupt
//			HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
//			// Enable receive command with interrupt
//			HAL_UART_Receive_IT(&huart1, &serialRX_Data, 1);
//
//			shouldTransmit = 0;
//
//		} // end if shouldTransmit
//
//	} // end if SIM_INIT
//
//	else if (simState == SIM_UE_CHECK) {
//		// Check UE Info
//		// If not connected to NB-IoT; NB-IoT setup should commence
//
//		if (shouldTransmit) {
//			sprintf(ATcommand, "AT+CPSI?\r\n");
//			// Transmit
//			HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
//			// Stop future transmissions until needed again
//			shouldTransmit = 0;
//
//		}
//
//	}
//
//	else if (simState == SIM_COMMAND_TEST) {
//		if (shouldTransmit) {
//			sprintf(ATcommand, "AT+CFUN=0?\r\n");
//			HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
//			shouldTransmit = 0;
//		} // end if shouldTransmit
//
//	} // end if SIM_COMMAND_TEST
//
//	else if (simState == SIM_PDN_ACTIVATION) {
//		if (shouldTransmit) {
//			sprintf(ATcommand, pdnCommands[pdnCommandIndex++]);
//			HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
//			shouldTransmit = 0;
//		}
//	}
//
//	else if (simState == SIM_HTTP_POST) {
//		if (shouldTransmit) {
//			sprintf(ATcommand, httpPostCommands[httpPostCommandIndex++]);
//			HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
//			shouldTransmit = 0;
//		}
//	}
//
//	else if (simState == SIM_STANDBY) {
//		shouldTransmit = 0;
//	}


	return;
}


void SIM_serialRX_Handler(uint8_t charReceived) {

	serialRX_Buffer[serialRX_BufferIndex++] = charReceived;

	// Remove later; Just for tracking
	allRX_Data[allRX_Data_Index++] = charReceived;



	if ((charReceived == (uint8_t)'\n')) {

		switch (simState) {
			case SIM_INIT:
				SIM_Init_Handler();
				break;
			case SIM_UE_CHECK:
				SIM_UE_Check_Handler();
				break;
			case SIM_PDN_ACTIVATION:
				SIM_PDN_Activation();
				break;
			default:
				break;
		}

	}


//
//	// Check if command was received; is 'OK' returned
//	if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {
//
//		if (simState == SIM_INIT) {
//			simState = SIM_UE_CHECK;
//			clearBuffer = 1;
//			shouldTransmit = 1;
//
//		} else if (simState == SIM_UE_CHECK) {
//
//			if (strstr((char*) serialRX_Buffer, "AT+CPSI?\r\r\n+CPSI: LTE NB-IOT,Online")) {
//				// SIM is connected to NB-IoT
//				simState = SIM_PDN_ACTIVATION;
//				clearBuffer = 1;
//				shouldTransmit = 1;
//			} else {
//				// SIM is NOT connected to NB-IoT
//				// NB-IoT setup should now happen
//				simState = SIM_CONNECT_NBIOT;
//				clearBuffer = 1;
//				shouldTransmit = 1;
//			}
//
//		} else if (simState == SIM_COMMAND_TEST) {
//			simState = SIM_STANDBY;
//			clearBuffer = 1;
//			shouldTransmit = 1;
//
//		} else if (simState == SIM_PDN_ACTIVATION) {
//			strncpy((char*)pdnFeedback[pdnCommandIndex-1], (char*)serialRX_Buffer, sizeof(pdnFeedback[0]));
//			if (pdnCommandIndex + 1 > PDN_LENGTH) { // Command sequence complete
//				simState = SIM_HTTP_POST;
//			}
//			clearBuffer = 1;
//			shouldTransmit = 1;
//
//		} else if (simState == SIM_HTTP_POST) {
//			strncpy((char*)httpPostFeedback[httpPostCommandIndex-1], (char*)serialRX_Buffer, sizeof(httpPostFeedback[0]));
//			if (httpPostCommandIndex + 1 > HTTP_POST_LENGTH) { // Command sequence complete
//				simState = SIM_STANDBY;
////				AT+SHCONN\r\r\nERROR\r\n
//			}
//			clearBuffer = 1;
//			shouldTransmit = 1;
//		}
//
//
//
//
//	} // end if 'OK' was returned
//
//	if (strstr((char*) serialRX_Buffer, "\nERROR\r\n")) { // Error message returned
//		simState = SIM_STANDBY;
//	}




	if (clearBuffer) { // reset index and clear buffer
		serialRX_BufferIndex = 0;
		memset(serialRX_Buffer, 0, sizeof(serialRX_Buffer));
		clearBuffer = 0;
	}


	return;
}


void SIM_Init_Handler(void) {

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


void SIM_UE_Check_Handler(void) {

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

	uint8_t maxCommand = 1;

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
				if (strstr((char*) serialRX_Buffer, "+COPS?\r\r\n+COPS: 1,0,\"VodaCom-SA\",9\r\n\r\nOK\r\n")) {
					isIncrementCommand = 1;
				}

				// TODO: If network information is wrong
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
