/*
 * dsp.h
 *
 *  Created on: Jul 6, 2020
 *      Author: marshal
 */

#ifndef DSP_H_
#define DSP_H_

#include "hal.h"
#include "ch.h"

/** ADC streaming callback for radio RX
 *
 * We sampled at 500kHz, but our bandwidth of interest is only
 * 2kHz. (Since it's quadrature sampling, nyquist rate is 1x)
 * We use a second-order IIR filter to prevent aliasing, followed
 * by an accumulate and decimation
 */

/** Datatype for complex numbers
 *
 * Designed to be the same size as msg_t for low overhead
 */
union complex{
  struct {
    int16_t real;
    int16_t imag;
  }x;
  msg_t msg;
};

/** ADC init
 */
void adc_rx_init(void);

#endif /* DSP_H_ */
