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

/** Starts radio reception and demodulation
 *
 */
void rxStart(uint8_t mode);

/* Pauses radio reception and demodulation
 *
 */
void rxStop(void);

/** Handles the radio reception
 *
 */
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

/** Radio RX thread
 *
 * This thread handles the data received from the ADC callback.
 * It calls the specific radio protocol being run (CW, SSB, etc)
 */
THD_FUNCTION(radio_rx, arg);

/** ADC streaming callback.
 *
 * Performs initial low-pass filter and downsamples to 5kHz
 * Passes data to the Radio RX thread
 */
static void adccallback(ADCDriver *adcp);

/** ADC init
 *
 * Configures the ADC for radio reception
 */
void adc_rx_init(void);

void speaker_init(void);

void speaker_update(int16_t data, int len);

#endif /* RX_H_ */
