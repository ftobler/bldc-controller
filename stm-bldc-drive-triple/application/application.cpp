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
#include "autogenerated_modbus.h"
#include "motor.h"
#include "sine.h"
#include "math.h"
#include <stdlib.h>




uint32_t adc_dma_results[4];

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim15;





Motor motors[3];

int32_t vcc_mv = 0;


extern Database_value_t database_value;
constexpr uint32_t app_pwm[3] = {1600,1600,1600};
int32_t app_target[3] = {2500,3000,1700};
//int32_t app_target2[3] = {2000,2000,2200};
//int32_t target_buffer[64][3] = {2000};


constexpr int up = 2400;
constexpr int hold = 1900;
constexpr int down = 1450;


uint8_t balls[5][2] = {0};

static void pick(float x, float y);
static void place(float x, float y);
static void grip();
static void release();
static void wait_for();
static void wait_for_fast();
static void wait_for_time(int ms);
static void wait_for_linear();
static void solve_inverse_kinematic(float cartesian_x, float cartesian_y, float* base, float* arm);
static int32_t angle_to_motor_0(float angle);
static int32_t angle_to_motor_1(float angle);
static void get_grid_coordinates(int grid_x, int grid_y, float* cartesian_x, float* cartesian_y);


void application_setup() {
	Motor* m;
	//configure motor 0 struct
	m = &motors[0];
	m->pwm[0] = &(htim1.Instance->CCR3);
	m->pwm[1] = &(htim1.Instance->CCR2);
	m->pwm[2] = &(htim1.Instance->CCR1);
	m->en_port = ENABLE_M1_GPIO_Port;
	m->en_pin = ENABLE_M1_Pin;
	m->dma_index = 0;
	m->offset = 124;
	m->coef_a = 1.75088286f;
	m->coef_b = -300;
	m->calibrated = 1;
	m->controller_p = 10;//9;
	m->controller_i = 0.005f;
	m->controller_d = 20;
	m->has_control_d = 1;
	m->has_control_i = 1;

	//configure motor 1 struct
	m = &motors[1];
	m->pwm[0] = &(htim3.Instance->CCR4);
	m->pwm[1] = &(htim3.Instance->CCR3);
	m->pwm[2] = &(htim3.Instance->CCR2);
	m->en_port = ENABLE_M2_GPIO_Port;
	m->en_pin = ENABLE_M2_Pin;
	m->dma_index = 1;
	m->reverse_field();
	m->offset = 299;
	m->coef_a = 1.75184751f;
	m->coef_b = -253;
	m->calibrated = 1;
	m->controller_p = 5;//3;
	m->controller_i = 0.005f;
	m->controller_d = 10;
	m->has_control_d = 1;
	m->has_control_i = 1;

	//configure motor 2 struct
	m = &motors[2];
	m->pwm[0] = &(htim3.Instance->CCR1);
	m->pwm[1] = &(htim15.Instance->CCR2);
	m->pwm[2] = &(htim15.Instance->CCR1);
	m->en_port = ENABLE_M3_GPIO_Port;
	m->en_pin = ENABLE_M3_Pin;
	m->dma_index = 2;
	m->reverse_field();
	m->offset = 222;
	m->coef_a = 1.75288868f;
	m->coef_b = -677;
	m->calibrated = 1;
	m->controller_p = 6;
	m->has_control_d = 0;
	m->has_control_i = 0;

	//start the timers, start pwm, one timer in interrupt mode.
	HAL_TIM_Base_Start(&htim1);
	HAL_TIM_Base_Start(&htim3);
	HAL_TIM_Base_Start_IT(&htim15);
	htim15.Instance->ARR = PWM_MAX-1;
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);

	//start ADC DMA to continiously write oversampled values in
	//a buffer. ADC is preconfigured to do this.
	HAL_ADC_Start_DMA(&hadc1, adc_dma_results, 4);

	scheduler_task_sleep(300); //wait for power to be more stable

	//switch PWM on
	for (int i = 0; i < 3; i++) {
		motors[i].en_port->BSRR = motors[i].en_pin;
		motors[i].max_pwm = app_pwm[i];
	}


	motors[0].target = 2000;
	motors[1].target = 2000;
	motors[2].target = 2000;

	balls[0][1] = 0;
	balls[1][1] = 1;
	balls[2][1] = 2;
	balls[3][1] = 3;
	balls[4][1] = 4;

}


int32_t position = 0;
int32_t capture = 0;
static volatile int coordinates[8][2] = {
		{2191,2509},
		{2478,2563},
		{2298,2554},
		{2654,2959},
		{3082,2850},
		{1859,1084},
		{2097,2950},
		{1778,2018},
};

//float px = 0.0f;
//float py = 40.0f;


__attribute__((optimize("Ofast"))) void application_loop() {

	//calculate vcc
	vcc_mv = adc_dma_results[3] * 33000 / 4096;


//	if (motors[0].calibrated) {
//		motors[0].max_pwm = 900;
//	}
//	if (motors[1].calibrated) {
//		motors[1].max_pwm = 900;
//	}
//	if (motors[2].calibrated) {
//		motors[2].max_pwm = 900;
//	}


//	if (motors[0].calibrated && motors[1].calibrated && motors[2].calibrated) {
//		motors[0].target = 1750 + 900.0f * sine(uwTick / 150.0f);
//		motors[1].target = motors[0].target + 450.0f * sine(uwTick / 50.0f);
//		motors[2].target = motors[1].target + 300.0f * sine(uwTick / 30.0f);
//	} else {
//		motors[0].target = 1750;
//		motors[1].target = 1750;
//		motors[2].target = 1750;
//	}


//	pick(coordinates[0][0], coordinates[0][1]);
//	place(coordinates[1][0], coordinates[1][1]);
//	pick(coordinates[2][0], coordinates[2][1]);
//	place(coordinates[3][0], coordinates[3][1]);
//
//	pick(coordinates[1][0], coordinates[1][1]);
//	place(coordinates[2][0], coordinates[2][1]);
//	pick(coordinates[3][0], coordinates[3][1]);
//	place(coordinates[0][0], coordinates[0][1]);
//
//	pick(coordinates[4][0], coordinates[4][1]);
//	place(coordinates[5][0], coordinates[5][1]);
//	pick(coordinates[6][0], coordinates[6][1]);
//	place(coordinates[7][0], coordinates[7][1]);

//	float base, arm;
//	solve_inverse_kinematic(px, py, &base, &arm);
//	app_target[0] = angle_to_motor_0(base);
//	app_target[1] = angle_to_motor_1(arm);
//	wait_for_time(1000);

//	for (int32_t grid_x = 0; grid_x < 9; grid_x++) {
//		for (int32_t grid_y = 0; grid_y < 5; grid_y++) {
//			float base, arm, px, py;
//			get_grid_coordinates(grid_x, grid_y, &px, &py);
//			solve_inverse_kinematic(px, py, &base, &arm);
//			app_target[0] = angle_to_motor_0(base);
//			app_target[1] = angle_to_motor_1(arm);
//			if (isnan(base) == 0 && isnan(arm) == 0) {
//				wait_for_time(300);
//			}
//		}
//	}

	for (int i = 0; i < 5; i++) {
		int rand_x = rand() % 9;
		int rand_y = rand() % 5;
		uint8_t populated = 0;
		for (int j = 0; j < 5; j++) {
			if (balls[j][0] == rand_x && balls[j][1] == rand_y) {
				//already a ball here.
				//skip this one.
				populated = 1;
				break;
			}
		}
		if (populated == 0) {
			//pick, assign and place
			float base, arm, px, py;

			//pick
			get_grid_coordinates(balls[i][0], balls[i][1], &px, &py);
			solve_inverse_kinematic(px, py, &base, &arm);
			app_target[0] = angle_to_motor_0(base);
			app_target[1] = angle_to_motor_1(arm);
			if (isnan(base) == 0 && isnan(arm) == 0) {
				pick(app_target[0], app_target[1]);
			}

			//assign
			balls[i][0] = rand_x;
			balls[i][1] = rand_y;

			//place
			get_grid_coordinates(balls[i][0], balls[i][1], &px, &py);
			solve_inverse_kinematic(px, py, &base, &arm);
			app_target[0] = angle_to_motor_0(base);
			app_target[1] = angle_to_motor_1(arm);
			if (isnan(base) == 0 && isnan(arm) == 0) {
				place(app_target[0], app_target[1]);
			}

		}
	}

}

//volatile float m0_m1_precontrol = -1.6f;
//volatile float m1_m0_precontrol = 0.08f;
volatile float m0_m1_precontrol = 0.5f;
volatile float m1_m0_precontrol = -0.25f;

void motor_control_transfer() {
	float angle_knee_joint = (motors[1].encoder - 1800) / (1024.0f * 3.14159f / 2);
	float cos_value = cose(angle_knee_joint);
	motors[0].input = motors[0].output + motors[1].output * m0_m1_precontrol * cos_value;
	motors[1].input = motors[1].output + motors[0].output * m1_m0_precontrol * cos_value;
	motors[2].input = motors[2].output;
}

static void pick(float x, float y) {
	app_target[0] = x;
	app_target[1] = y;
	wait_for();
	scheduler_task_sleep(100);
	grip();
	scheduler_task_sleep(50);
}

static void place(float x, float y) {
	app_target[0] = x;
	app_target[1] = y;
	wait_for();
	release();
}

static void grip() {
	app_target[2] = down;
	wait_for_fast();
	scheduler_task_sleep(300);
	app_target[2] = hold;
	wait_for_fast();
}
static void release() {
	app_target[2] = up;
	wait_for_fast();
	scheduler_task_sleep(150);
	app_target[2] = hold;
//	wait_for_fast();
}


static void wait_for() {

	int32_t diffmax = 0;
	for (int i = 0; i < 2; i++) {  //omit calculation for z motor here. (it can go fast)
		int32_t diff = motors[i].target - app_target[i];
		if (diff < 0) {
			diff = -diff;
		}
		if (diff > diffmax) {
			diffmax = diff;
		}
	}
	if (diffmax < 50) {
		diffmax = 50;
	}
	if (diffmax > 600) {
		diffmax = 600;
	}


	wait_for_time(diffmax);

	scheduler_task_sleep(300);
}

static void wait_for_fast() {
	wait_for_time(75);
}


static void wait_for_time(int ms) {
	int32_t diff[3];
	for (int i = 0; i < 3; i++) {
		diff[i] = motors[i].target - app_target[i];
	}
	for (int j = 0; j < ms; j++) {
		float t = (float)j / ms;
		t = (cose(t * 3.14159f) + 1.0f) / 2.0f;
		for (int i = 0; i < 3; i++) {
			int position = app_target[i] + diff[i] * t;
			motors[i].target = position;
		}
		scheduler_task_sleep(1);
	}
}

static void wait_for_linear() {
	for (int j = 0; j < 200; j++) {
		int good = 0;
		for (int i = 0; i < 3; i++) {
			if (app_target[i] != motors[i].target) {
				if (app_target[i] > motors[i].target) {
					motors[i].target++;
				} else {
					motors[i].target--;
				}
			} else {
				good += 1;
			}
		}
		if (good >= 3) {
			break;
		}
		scheduler_task_sleep(1);
	}
}


static void solve_inverse_kinematic(float cartesian_x, float cartesian_y, float* base, float* arm) {
	//the robot base is defined as (0/0).
	//the base angle of 0° points in cartesian_y direction
	//the base angle increasing points in cartesian_x direction (clockwise)
	//the arm angle of 0* points back to base (impossible) / points in negative cartesian_y direction.
	//the arm angle increasing rotates the joint clockwise
	//=> fully extended arm, poining in cartesian_y is base=0°, arm=180°

	constexpr float link_length_base = 40.0f;
	constexpr float link_length_swing = 55.13f;

	constexpr float link_length_base_sq = link_length_base * link_length_base;
	constexpr float link_length_swing_sq = link_length_swing * link_length_swing;

	//calculate the length extension of the arm.
	float length = sqrtf(cartesian_x*cartesian_x + cartesian_y*cartesian_y);
	float length_sq = length * length;

	//the arm consists out of 2 triangles. one with base len and the other
	//with swing len as the hypotenuse
	//these triangles are right angle triangles.
	float base_angle_1 = acosf((link_length_base_sq + length_sq - link_length_swing_sq) / (2.0f * link_length_base * length));
	*arm = acosf((link_length_base_sq + link_length_swing_sq - length_sq) / (2.0f * link_length_base * link_length_swing));
	float base_angle_2 = atan2f(cartesian_x, cartesian_y);
	*base = base_angle_1 + base_angle_2;
}

int32_t base_zero_angle = 1890;
int32_t arm_zero_angle = 1700;


static int32_t angle_to_motor_0(float angle) {
	constexpr float pi = M_PI;
	return base_zero_angle + angle / 2.0f / pi * 4096.0f;
}

static int32_t angle_to_motor_1(float angle) {
	constexpr float pi = M_PI;
	return arm_zero_angle - ((angle / 2.0f / pi) - 0.5f) * 4096.0f;
}


static void get_grid_coordinates(int grid_x, int grid_y, float* cartesian_x, float* cartesian_y) {
	*cartesian_x = (grid_x - 4.0f) * 10.0f;
	*cartesian_y = 55.0f + (3.0f - grid_y) * 10.0f;
}


