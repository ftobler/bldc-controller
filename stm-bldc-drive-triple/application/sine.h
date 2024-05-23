/*
 * sine.h
 *
 *  Created on: May 12, 2024
 *      Author: ftobler
 */

#ifndef SINE_H_
#define SINE_H_

#include "stdint.h"


enum {
	N = 1024,
	N23 = 2 * N / 3,
	SINMAX = 32768,
};

void sine_init();
float sine(float w);
float cose(float w);



#endif /* SINE_H_ */
