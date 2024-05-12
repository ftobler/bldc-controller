/*
 * taskmanager.h
 *
 *  Created on: Apr 17, 2024
 *      Author: ftobler
 */

#ifndef TASKMANAGER_H_
#define TASKMANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif


enum {
	TASK_ID_IDLE = 0,

	TASK_ID_LED = 1,

	TASK_ID_PROTOCOL = 2,
	EVENT_PROTOCOL_UART_IDLE = 0x01,


	TASK_ID_APPLICATION = 3,
	EVENT_APPLICATION_TIMER = 0x01,

	TASK_ID_MOTOR_0 = 4,
	EVENT_MOTOR_0_START = 0x01,
	EVENT_MOTOR_0_PWM = 0x02,

	TASK_ID_MOTOR_1 = 5,
	EVENT_MOTOR_1_START = 0x01,
	EVENT_MOTOR_1_PWM = 0x02,

	TASK_ID_MOTOR_2 = 6,
	EVENT_MOTOR_2_START = 0x01,
	EVENT_MOTOR_2_PWM = 0x02,

//	TASK_ID_CONTROLLER = 3,
};


void taskmanager_init();

#ifdef __cplusplus
}
#endif


#endif /* TASKMANAGER_H_ */
