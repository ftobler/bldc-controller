/*
 * sine.cpp
 *
 *  Created on: May 12, 2024
 *      Author: ftobler
 */


#include "sine.h"
#include "math.h"

int16_t sintab[N];


void sine_init() {
	for (int i = 0; i < N; i++) {
		float a = (float)i / N * 2.0f * M_PI;
		sintab[i] = sinf(a) * (SINMAX - 1);
	}
}


float sine(float w) {
	int32_t wi = w * N / M_PI / 2.0f;
	int16_t result = sintab[wi & (N - 1)];
	return (float)result / SINMAX;
}
