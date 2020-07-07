/*
 * rx.h
 *
 *  Created on: Jul 6, 2020
 *      Author: marshal
 */

#ifndef RX_H_
#define RX_H_

#include "hal.h"
#include "ch.h"

THD_FUNCTION(radio_rx, arg);

/** Datatype for complex numbers
 *
 * Designed to be the same size as msg_t for low overhead
 */
union complex{
  struct {
    int16_t real;
    int16_t imag;
  };
  msg_t msg;
};

/** ADC init
 */
void adc_rx_init(void);

#endif /* RX_H_ */
