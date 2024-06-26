/*
 * motor.h
 *
 *  Created on: May 11, 2024
 *      Author: ftobler
 */

#ifndef MOTOR_H_
#define MOTOR_H_

#include "stm32_hal.h"

constexpr uint32_t HISTORY_LEN = 128;


void motor_sintab_init();



class Motor {
public:
	volatile uint32_t* pwm[3]; //pointer to pwm registers
	volatile GPIO_TypeDef* en_port; //enable port
	uint16_t en_pin; //enable pin
	int8_t dma_index; //index of the sensor feedback
	uint8_t has_control_d = 0;
	uint8_t has_control_i = 0;

	uint32_t calibrated = 0;
	uint32_t max_pwm;
	int32_t offset = 0; //0 position should be at mechanical zero

	int32_t target;
	int32_t encoder;
	int32_t encoder_raw;
	int32_t controller_p;
	float controller_d;
	float controller_i;
	int32_t output = 0;
	int32_t input = 0;

	int32_t last_encoder;
	float differentiator;
	int32_t integrator;

	float coef_a = 0.0f;
	int32_t coef_b = 0;
	Motor();
	void control();
	void update();
	void calibrate();
	void assign_angle(uint32_t power, int32_t angle);
	void reverse_field();
};

#endif /* MOTOR_H_ */
