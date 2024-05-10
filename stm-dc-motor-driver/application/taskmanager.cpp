/*
 * taskmanager.cpp
 *
 *  Created on: Apr 17, 2024
 *      Author: ftobler
 */

#include "stm32_hal.h"
#include "taskmanager.h"
#include "flortos.h"
#include "application.h"
#include "main.h"

enum {
	STD_STACK_SIZE = 1024
};

static uint8_t stack_idle_task[STD_STACK_SIZE];
static uint8_t stack_application_task[STD_STACK_SIZE];
static uint8_t stack_controller_task[STD_STACK_SIZE];
static uint8_t stack_protocol_task[STD_STACK_SIZE];

static void idle_task_fn();
static void application_task_fn();
static void protocol_task_fn();
static void controller_task_fn();


void taskmanager_init() {
	scheduler_init();
	scheduler_addTask(TASK_ID_IDLE, idle_task_fn, stack_idle_task, STD_STACK_SIZE); //lowest priority
	scheduler_addTask(TASK_ID_PROTOCOL, protocol_task_fn, stack_protocol_task, STD_STACK_SIZE);
	scheduler_addTask(TASK_ID_APPLICATION, application_task_fn, stack_application_task, STD_STACK_SIZE); //highest priority
//	scheduler_addTask(TASK_ID_CONTROLLER, controller_task_fn, stack_controller_task, STD_STACK_SIZE); //highest priority
	scheduler_join();
}


static void idle_task_fn() {
	while (1) {
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, (GPIO_PinState)((uwTick % 500) > 250));
	}
}

static void application_task_fn() {
	application_setup();
	while (1) {
		scheduler_event_wait(EVENT_APPLICATION_TIMER);
		application_loop();
	}
}

static void protocol_task_fn() {
	while (1) {
		scheduler_task_sleep(1000);
	}
}

static void controller_task_fn() {
	while (1) {
		scheduler_task_sleep(2000);
	}
}


