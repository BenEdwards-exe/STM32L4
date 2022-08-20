/*
 * sim7070g.h
 *
 *  Created on: Aug 20, 2022
 *      Author: benjr
 */

#ifndef INC_SIM7070G_H_
#define INC_SIM7070G_H_



#endif /* INC_SIM7070G_H_ */




typedef enum {
	SIM_ERROR,
	SIM_STANDBY,
	SIM_INIT,
	SIM_CONNECT_SERVICE,
	SIM_CONNECT_NBIOT,
	SIM_HTTP_POST,
	SIM_PDN_ACTIVATION,
	SIM_COMMAND_TEST
}simStateType;








// Prototypes begin
void simHandler(void);
void simPDN_Activation(void);
void serialRXHandler(uint8_t charReceived);


// Prototypes end



