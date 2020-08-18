/*
 * trig.h
 *
 *  Created on: Jul 8, 2020
 *      Author: marshal
 */

#ifndef TRIG_H_
#define TRIG_H_

#include <math.h>
#include <stdint.h>

#define F_SAMP_TX 4810        //4810 // ADC sample-rate; is best a multiple of _UA and fits exactly in OCR0A = ((F_CPU / 64) / F_SAMP_TX) - 1 , should not exceed CPU utilization
#define _UA  (F_SAMP_TX)      //360  // unit angle; integer representation of one full circle turn or 2pi radials or 360 degrees, should be a integer divider of F_SAMP_TX and maximized to have higest precision

// Stdlib includes abs, but I'm not sure we want it. This works for all numerical types.
#define abs(x) (x>=0?x:-x)

int16_t arctan3(int16_t q, int16_t i);

#define magn(i, q) (abs(i) > abs(q) ? abs(i) + abs(q) / 4 : abs(q) + abs(i) / 4) // approximation of: magnitude = sqrt(i*i + q*q); error 0.95dB


#endif /* TRIG_H_ */
