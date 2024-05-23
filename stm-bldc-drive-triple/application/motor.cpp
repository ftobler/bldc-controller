/*
 * motor.cpp
 *
 *  Created on: May 11, 2024
 *      Author: ftobler
 */


#include "motor.h"
#include "math.h"
#include "flortos.h"
#include "application.h"
#include "sine.h"


constexpr int pwm_max_half = (PWM_MAX / 2) - 1;


#define get_encoder() (adc_dma_results[dma_index])

extern int16_t sintab[N];
extern uint32_t adc_dma_results[4];

//volatile static int32_t controller_p = 4;
//volatile static int32_t controller_i = 0;
volatile static int32_t controller_i_max = 200000;

//static int absi(int i);



Controller::Controller() {
	target = 0;
	encoder = 0;
	controller_p = 0;
	output = 0;
}
void Controller::update() {
}

Motor::Motor() {
	coef_a = 1.75f;
	coef_b = -280;
	max_pwm = pwm_max_half;
//	controller_p = 4;
//	controller_i = 0;
//	for (uint32_t i = 0; i < HISTORY_LEN; i++) {
//		encoder_history[i] = 0;
//	}
	differentiator = 0;
	controller_d = 0;
}

__attribute__((optimize("Ofast"))) void Motor::control() {
	int32_t _raw = adc_dma_results[dma_index];
	encoder_raw = _raw;
	encoder = _raw - offset;
	int32_t error = target - encoder;

	differentiator = differentiator * 0.99f + (last_encoder - encoder);
	last_encoder = encoder;

	output = error * controller_p + differentiator * controller_d;
}

__attribute__((optimize("Ofast"))) void Motor::update() {

//	if (calibrated == 0) {
//		assign_angle(0, 0);
//		return;
//	}

//	int32_t encoder_raw = adc_dma_results[dma_index];
//	encoder = encoder_raw - offset;
//	int32_t error = target - encoder;
//
//
//
//	int32_t _last_encoder = encoder_history[encoder_history_pos];
//	encoder_history[encoder_history_pos] = encoder;
//	encoder_history_pos = (encoder_history_pos + 1) % HISTORY_LEN;
//
//	speed_filtered =+ encoder - _last_encoder;
//
//
//	speed = speed_filtered;
//	if (speed > 3) {
//		error -= (speed-3) * speed_nerf;
//	} else if (speed < -3) {
//		error -= (speed+3) * speed_nerf;
//	}
////	error -= speed * speed_nerf;
//	_speed = speed;
//
//
//	int32_t proportional = error * controller_p;
//
//
//	int32_t output = proportional;// + (integrator_tmp * controller_i / 4096 / 4);

//	int32_t w = coef_a * encoder_raw + coef_b;
	int32_t w = ((encoder_raw * 7) / 4) + coef_b; //coef A is 1.75 in theory
	int32_t _input = input;
	if (_input > 0) {
		w += N / 4; //add 90deg
	} else {
		w -= N / 4; //subtract 90deg
	}
	if (_input < 0) {
		_input = -_input;
	}

	assign_angle(_input, w);

}


void Motor::calibrate() {
	//initial, the motor is not at a hard endstop and can freely move

	constexpr int pwm = 350; // homing pwm
	constexpr int pwm2 = 750; // calibration pwm

	//find the maximum hard endstop point => run motor forwards
	int w = 0; //phase angle omega
	int maximum_sensor = 0;
	float tp_sensor = 4096.0f; //tp filtering of the sensor. initialize far away from zero (assumed we are not max now)
	int max_cnt = 0;
	while (1) {
		assign_angle(pwm, w);
		scheduler_task_sleep(1);
		int sensor = get_encoder();
		tp_sensor = sensor * 0.1f + tp_sensor * 0.9f;
		float mean_change = sensor - tp_sensor; //mean change of the sensor between tries.
		if (maximum_sensor < sensor) {
			maximum_sensor = sensor;
		}
		//check if sensor is very near maximum point and
		//sensor is not moving
		if (abs(sensor - maximum_sensor) < 10.0f && mean_change*mean_change < 20.0f) {
			max_cnt++;
			//this condition must apply for a certain duration, then homing is complete.
			if (max_cnt > N/8) {
				break;
			}
			w = w + 1; //slow down
		} else {
			max_cnt = 0;
			w = w + 8; //speed up
		}
	}

	//find the minimum hard endstop point => run motor backwards
	w = 0;
	int minimum_sensor = 4096;
	max_cnt = 0;
	tp_sensor = 0.0f; //tp filtering of the sensor. initialize far away from max (assumed we are max)
	while (1) {
		assign_angle(pwm, w);
		scheduler_task_sleep(1);
		int sensor = get_encoder();
		tp_sensor = sensor * 0.1f + tp_sensor * 0.9f;
		float mean_change = sensor - tp_sensor; //mean change of the sensor between tries.
		if (minimum_sensor > sensor) {
			minimum_sensor = sensor;
		}
		//check if sensor is very near maximum point and
		//sensor is not moving
		if (abs(sensor - minimum_sensor) < 10.0f && mean_change*mean_change < 20.0f) {
			max_cnt++;
			//this condition must apply for a certain duration, then homing is complete.
			if (max_cnt > N/8) {
				break;
			}
			w = w - 1; //slow down
		} else {
			max_cnt = 0;
			w = w - 8; //speed up
		}
	}

	//the two points correspond to the mechanical limit. offset them 35 (~3°) so there is a bit
	//of room to work with. These two numbers represent the min/max calibration points.
	maximum_sensor -= 35;
	minimum_sensor += 35;

	//manually position rotor to minimum sensor position. we should be close already
	w = 0;
	assign_angle(pwm, w);
	scheduler_task_sleep(10);
	max_cnt = 0;
	while (1) {
		int sensor = get_encoder();
		int error = minimum_sensor - sensor;
		if (error == 0) {
			max_cnt++;
		} else if (error > 0) {
			w++;
		} else {
			w--;
		}
		if (max_cnt > 40) {
			break;
		}
		assign_angle(pwm, w);
		scheduler_task_sleep(1);
	}

	//make a regression while driving to max position
	float sum_x = 0.0f;
	float sum_y = 0.0f;
	float sum_x_squared = 0.0f;
	float sum_xy = 0.0f;
	assign_angle(pwm2, w);
	scheduler_task_sleep(10);
	int n = 0;
	while (1) {
		int sensor = get_encoder();
		if (sensor > maximum_sensor) {
			break;
		}

		//accumulate the regression
		float x = sensor;
		float y = w;
		sum_x += x;
		sum_y += y;
		sum_x_squared += x*x;
		sum_xy += x*y;

		w++;
		assign_angle(pwm2, w);
		scheduler_task_sleep(1);
		n++;
	}
	scheduler_task_sleep(10);

	//continiue regression while driving to min position
	while (1) {
		int sensor = get_encoder();
		if (sensor < minimum_sensor) {
			break;
		}

		//accumulate the regression
		float x = sensor;
		float y = w;
		sum_x += x;
		sum_y += y;
		sum_x_squared += x*x;
		sum_xy += x*y;

		w--;
		assign_angle(pwm2, w);
		scheduler_task_sleep(1);
		n++;
	}

	//calculate a and b for y = ax+b where y = angle omega and x = hall encoder
	coef_a = (sum_xy * n - sum_x * sum_y) / (n * sum_x_squared - sum_x * sum_x);
	coef_b = (sum_y * sum_x_squared - sum_x * sum_xy) / (n * sum_x_squared - sum_x * sum_x);

	offset = minimum_sensor;


//	//move controlled to this target
//	while (1) {
//		int32_t encoder = get_encoder();
//		int32_t error = target - encoder + offset;
//		int32_t output = error * 2;
//
//
//		int32_t w = coef_a * encoder + coef_b;
//		if (error < 20 && error > -20) {
//			break;
//		}
//		if (error > 0) {
//			w += N / 4; //add 90deg
//		} else {
//			w -= N / 4; //subtract 90deg
//		}
//		if (output < 0) {
//			output = -output;
//		}
//		if (output > pwm) {
//			output = pwm;
//		}
//
//		assign_angle(output, w);
//		scheduler_task_sleep(1);
//	}


	assign_angle(0, 0);

	calibrated = 1;

}


__attribute__((optimize("Ofast"))) void Motor::assign_angle(uint32_t power, int32_t angle) {
	if (power > pwm_max_half) {
		power = pwm_max_half;
	}
	if (power > max_pwm) {
		power = max_pwm;
	}
	//C % is not a true modulo, but the remainder
	//since N is a power of 2, we use bit masking
	*pwm[0] = pwm_max_half + (int32_t)(sintab[(angle            ) & (N-1)]) * power / SINMAX;
	*pwm[1] = pwm_max_half + (int32_t)(sintab[(angle + N23      ) & (N-1)]) * power / SINMAX;
	*pwm[2] = pwm_max_half + (int32_t)(sintab[(angle + N23 + N23) & (N-1)]) * power / SINMAX;

//	modulo variant:
//	*pwm[0] = 3199 + (int32_t)(sintab[(angle            ) % N]) * power / SINMAX;
//	*pwm[1] = 3199 + (int32_t)(sintab[(angle + N23      ) % N]) * power / SINMAX;
//	*pwm[2] = 3199 + (int32_t)(sintab[(angle + N23 + N23) % N]) * power / SINMAX;
}

void Motor::reverse_field() {
	volatile uint32_t* tmp = pwm[0];
	pwm[0] = pwm[2];
	pwm[2] = tmp;
}


//static int abs(int i) {
//	if (i > 0) {
//		return i;
//	}
//	return -i;
//}
