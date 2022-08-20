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
uint8_t serialRX_Buffer[250] = {0};
uint8_t serialRX_BufferIndex = 0;
uint8_t serialRX_Data;
uint8_t clearBuffer = 0;

#define PDN_LENGTH 8
char pdnCommands[PDN_LENGTH][20] = {
		"AT+CPIN?\r\n",
		"AT+CSQ\r\n",
		"AT+CGATT?\r\n",
		"AT+COPS?\r\n",
		"AT+CGNAPN\r\n",
		"AT+CNACT=0,0\r\n", // deactivate network before activation; FIX LATER
		"AT+CNACT=0,1\r\n",
		"AT+CNACT?\r\n"
};
uint8_t pdnCommandIndex = 0;
uint8_t pdnFeedback[PDN_LENGTH][150] = {0};


void simHandler(void) {

	if (simState == SIM_INIT) { // Check communication between SIM and computer

		if (shouldTransmit) {
			sprintf(ATcommand, "AT\r\n");
			// Enable transmit command with interrupt
			HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
			// Enable receive command with interrupt
			HAL_UART_Receive_IT(&huart1, &serialRX_Data, 1);

			shouldTransmit = 0;

		} // end if shouldTransmit

	} // end if SIM_INIT

	else if (simState == SIM_COMMAND_TEST) {
		if (shouldTransmit) {
			sprintf(ATcommand, "AT+CPSI?\r\n");
			HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
			shouldTransmit = 0;
		} // end if shouldTransmit
	} // end if SIM_COMMAND_TEST

	else if (simState == SIM_PDN_ACTIVATION) {
		if (shouldTransmit) {
			sprintf(ATcommand, pdnCommands[pdnCommandIndex++]);
			HAL_UART_Transmit_IT(&huart1, (uint8_t *) ATcommand, strlen(ATcommand));
			shouldTransmit = 0;
		}
	}


	return;
}


void serialRXHandler(uint8_t charReceived) {

	serialRX_Buffer[serialRX_BufferIndex++] = charReceived;

	// Check if command was received; is 'OK' returned
	if (strstr((char*) serialRX_Buffer, "\nOK\r\n")) {

		if (simState == SIM_INIT) {
			simState = SIM_PDN_ACTIVATION;
			clearBuffer = 1;
			shouldTransmit = 1;

		} else if (simState == SIM_COMMAND_TEST) {
			simState = SIM_STANDBY;
			clearBuffer = 1;
			shouldTransmit = 1;

		} else if (simState == SIM_PDN_ACTIVATION) {
			strncpy((char*)pdnFeedback[pdnCommandIndex-1], (char*)serialRX_Buffer, sizeof(pdnFeedback[0]));
			if (pdnCommandIndex + 1 > PDN_LENGTH) { // Command sequence complete
				simState = SIM_STANDBY;
			}
			clearBuffer = 1;
			shouldTransmit = 1;
		}




	} // end if 'OK' was returned




	if (clearBuffer) { // reset index and clear buffer
		serialRX_BufferIndex = 0;
		memset(serialRX_Buffer, 0, sizeof(serialRX_Buffer));
		clearBuffer = 0;
	}


	return;
}

void simPDN_Activation(void) {

	return;
}
