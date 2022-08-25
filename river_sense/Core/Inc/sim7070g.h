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
	SIM_INIT,
	SIM_UE_CHECK,
	SIM_PDN_ACTIVATION,
	SIM_HTTP_POST_BUILD,
	SIM_HTTP_MAKE_POST,
	SIM_ERROR,
	SIM_STANDBY,
	SIM_CONNECT_SERVICE,
	SIM_CONNECT_NBIOT,
	SIM_COMMAND_TEST
}simStateType;








// Prototypes begin
void SIM_Handler(void);
void SIM_serialRX_Handler(uint8_t charReceived);
void SIM_Init(void);
void SIM_UE_Check(void);
void SIM_PDN_Activation(void);
void SIM_HTTP_Post_Build(void);
void SIM_HTTP_Make_Post(void);

void copySubstringFromMatch(char* destination, char* source, char* strToMatch);

// Prototypes end



