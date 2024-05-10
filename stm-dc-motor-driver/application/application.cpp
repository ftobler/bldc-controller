/*
 * application.cpp
 *
 *  Created on: Apr 17, 2024
 *      Author: ftobler
 */


#include "stm32_hal.h"
#include "application.h"
#include "main.h"
#include "flortos.h"

enum {

};

extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim17;

volatile int32_t target_position = 0;
volatile int32_t max_pwm = 6000;
extern int32_t encoder_position;
static int32_t integrator = 0;

uint32_t adc_dma_results[1];


volatile int32_t control_p = 5;
volatile int32_t control_i = 5;
volatile int32_t control_i_max = 8192;



extern int32_t encoder_speed;
extern int32_t encoder_speed_raw;


extern int32_t as5600_angle;


void application_setup() {

	HAL_TIM_Base_Start(&htim17); //counts 10us per tick

	HAL_TIM_Base_Start_IT(&htim3);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

	htim3.Instance->CCR1 = 0;
	htim3.Instance->CCR2 = 0;

	HAL_ADC_Start_DMA(&hadc1, adc_dma_results, 1);

	scheduler_task_sleep(300); //wait for power to be more stable
}

__attribute__((optimize("Ofast"))) void application_loop() {
	int32_t error = target_position - encoder_position;
	if (error < 100 && error > -100) {
		error = 0;
		integrator = integrator * 7 / 8; //decay
	}
	integrator += error;
	if (integrator > control_i_max * 4096) {
		integrator = control_i_max * 4096;
	}
	if (integrator < -control_i_max * 4096) {
		integrator = -control_i_max * 4096;
	}

	int32_t output = error * control_p + integrator * control_i / 4096;

	int32_t pwm = 0;
	if (output > 0) {
		pwm = output;
	} else {
		pwm = -output;
	}

	if (pwm > 100) {
		pwm += 3000;
	}
	if (pwm > max_pwm) {
		pwm = max_pwm;
	}

	if (output > 0) {
		htim3.Instance->CCR1 = 0;
		htim3.Instance->CCR2 = pwm;
	} else {
		htim3.Instance->CCR1 = pwm;
		htim3.Instance->CCR2 = 0;
	}




	//calculate speed
	static int32_t encoder_speed_filter = 0;
	encoder_speed_filter = (encoder_speed_filter * 127 + encoder_speed_raw) / 128;
	encoder_speed = encoder_speed_filter / 256;
	encoder_speed_raw = (encoder_speed_raw * 7) / 8; //decay;
}

