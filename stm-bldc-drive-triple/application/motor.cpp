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

enum {
	N = 1024,
	N23 = 2 * N / 3,
	SINMAX = 32768,
};

#define get_encoder() (adc_dma_results[dma_index])

static int16_t sintab[N];
extern uint32_t adc_dma_results[4];

void motor_sintab_init() {
	for (int i = 0; i < N; i++) {
		float a = (float)i / N * 2.0f * M_PI;
		sintab[i] = sinf(a) * (SINMAX - 1);
	}
}

Motor::Motor() {
	coef_a = 1.75f;
	coef_b = -280;
}

void Motor::update() {

	if (calibrated == 0) {
		assign_angle(0, 0);
		return;
	}

	int32_t encoder = adc_dma_results[dma_index];
	int32_t error = target - encoder;
	int32_t output = error * 4;

	int32_t w = coef_a * encoder + coef_b;
	if (error > 0) {
		w += N / 4; //add 90deg
	} else {
		w -= N / 4; //subtract 90deg
	}
	if (output < 0) {
		output = -output;
	}

	assign_angle(output, w);

}


void Motor::calibrate() {
	//initial, the motor is not at a hard endstop and can freely move

	constexpr int pwm = 350; // homing pwm
	constexpr int pwm2 = 750; // calibration pwm

	//find the maximum hard endstop point => run motor forwards
	int w = 0; //phase angle omega
	int maximum_sensor = 0;
	int max_cnt = 0;
	while (1) {
		assign_angle(pwm, w);
		scheduler_task_sleep(1);
		int sensor = get_encoder();
		if (maximum_sensor < sensor) {
			maximum_sensor = sensor;
			max_cnt = 0;
		} else {
			max_cnt++;
		}
		if (max_cnt > N+100) {
			break;
		}
		w = w + 8;
	}
	//find the minimum hard endstop point => run motor backwards
	w = 0;
	int minimum_sensor = 4096;
	max_cnt = 0;
	while (1) {
		assign_angle(pwm, w);
		scheduler_task_sleep(1);
		int sensor = get_encoder();
		if (minimum_sensor > sensor) {
			minimum_sensor = sensor;
			max_cnt = 0;
		} else {
			max_cnt++;
		}
		if (max_cnt > N+100) {
			break;
		}
		w = w - 8;
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

	//calculate a and b for y = ax+b where y = angle omega and x = hall encoder
	coef_a = (sum_xy * n - sum_x * sum_y) / (n * sum_x_squared - sum_x * sum_x);
	coef_b = (sum_y * sum_x_squared - sum_x * sum_xy) / (n * sum_x_squared - sum_x * sum_x);

	//calculate new target position
	target = (maximum_sensor + minimum_sensor) / 2;

	//move controlled to this target
	while (1) {
		int32_t encoder = get_encoder();
		int32_t error = target - encoder;
		int32_t output = error * 2;


		int32_t w = coef_a * encoder + coef_b;
		if (error < 20 && error > -20) {
			break;
		}
		if (error > 0) {
			w += N / 4; //add 90deg
		} else {
			w -= N / 4; //subtract 90deg
		}
		if (output < 0) {
			output = -output;
		}
		if (output > pwm) {
			output = pwm;
		}

		assign_angle(output, w);
	}


	assign_angle(0, 0);

	calibrated = 1;

}


void Motor::assign_angle(uint32_t power, int32_t angle) {
	constexpr int pwm_max_half = (PWM_MAX / 2) - 1;

	if (power > pwm_max_half) {
		power = pwm_max_half;
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
