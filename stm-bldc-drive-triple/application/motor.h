/*
 * motor.h
 *
 *  Created on: May 11, 2024
 *      Author: ftobler
 */

#ifndef MOTOR_H_
#define MOTOR_H_

#include "stm32_hal.h"

class Motor {
private:
public:
	volatile uint32_t* pwm[3]; //pointer to pwm registers
	volatile GPIO_TypeDef* en_port; //enable port
	uint16_t en_pin; //enable pin
	uint32_t dma_index; //index of the sensor feedback
	uint32_t hall_value;
	Motor();
	void update();
};

#endif /* MOTOR_H_ */
