/*
 * rx.c
 *
 *  Created on: Jul 6, 2020
 *      Author: marshal
 */

#include "rx.h"
#include "dsp.h"

/** Mailbox for received data */
mailbox_t new_sample;

THD_WORKING_AREA(waradio_rx, 128);
THD_FUNCTION(radio_rx, arg){
  (void)arg;
  const int len = 256;
  int16_t * data = chCoreAllocFromBase(len*sizeof(int16_t), sizeof(int16_t), 0);
  while(1){
    union complex c;
    for( int i=0; i<len;){
      chMBFetchTimeout(&new_sample, &c.msg, TIME_INFINITE);
      data[i++] = c.x.real;
      data[i++] = c.x.imag;
    }
    /** Here would be a good place to do a FFT or other processing */
  }
}
