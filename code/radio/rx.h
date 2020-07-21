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

/** List of radio modes.
 *
 * This list will be expanded as more modes are supported
 */
enum radio_mode {
  STOPPED, //< Receiver is stopped
  CW, //< Continuous wave, also known as morse code
  USB,//< Upper Sideband, for frequencies above 10MHz
  LSB,//< Lower sideband, for frequencies below 10MHz
};

/** Starts radio reception and demodulation
 *
 */
void rxStart(enum radio_mode mode, float frequency);

/* Stops radio reception and demodulation
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
void adccallback(ADCDriver *adcp);

/** ADC init
 *
 * Configures the ADC for radio reception
 */
void adc_rx_init(void);

#endif /* RX_H_ */
