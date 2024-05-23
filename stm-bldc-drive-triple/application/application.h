/*
 * application.h
 *
 *  Created on: Apr 17, 2024
 *      Author: ftobler
 */

#ifndef APPLICATION_H_
#define APPLICATION_H_

#ifdef __cplusplus
extern "C" {
#endif

constexpr int PWM_MAX = 3200; //20kHz

void application_setup();
void application_loop();

void motor_control_transfer();

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_H_ */
