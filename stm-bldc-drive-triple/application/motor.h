/*
 * motor.h
 *
 *  Created on: May 11, 2024
 *      Author: ftobler
 */

#ifndef MOTOR_H_
#define MOTOR_H_

#include "stm32_hal.h"


void motor_sintab_init();

class Motor {
private:
public:
	volatile uint32_t* pwm[3]; //pointer to pwm registers
	volatile GPIO_TypeDef* en_port; //enable port
	uint16_t en_pin; //enable pin
	uint32_t dma_index; //index of the sensor feedback

	int32_t target;
	uint32_t calibrated = 0;
	uint32_t max_pwm;
	int32_t offset = 0; //0 position should be at mechanical zero
	int32_t encoder;


//	float coef_a = 0.0f;
//	float coef_b = 0.0f;
	float coef_a = 0.0f;
	int32_t coef_b = 0;
	Motor();
	void update();
	void calibrate();
	void assign_angle(uint32_t power, int32_t angle);
	void reverse_field();
};

#endif /* MOTOR_H_ */
