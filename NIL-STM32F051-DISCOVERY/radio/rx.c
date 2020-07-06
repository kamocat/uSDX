/*
 * rx.c
 *
 *  Created on: Jul 6, 2020
 *      Author: marshal
 */

#include "rx.h"
#include "dsp.h"

/** Mailbox for received data */
msg_t new_sample;

THD_WORKING_AREA(waradio_rx, 128);
THD_FUNCTION(radio_rx, arg){
  (void)arg;
  const int len = 256;
  int16_t * data = chCoreAllocFromBase(len*sizeof(int16_t), sizeof(int16_t), 0);
  while(1){
    for( int i=0; i<len;){
      thread_t * thd = chMsgWait();
      struct complex * c = (struct complex *)chMsgGet(thd);
      data[i++] = c->real;
      data[i++] = c->imag;
      chMsgRelease(thd, MSG_OK);
    }
    /** Here would be a good place to do a FFT or other processing */
  }
}
