/*
 * ssb.c
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */
#include "ssb.h"
#include "hilbert.h"
#include "../drivers/speaker.h"
#include "../trig.h"

msg_t ssb_rx(mailbox_t * inbox, enum radio_mode * mode){
  union complex c;
  const int len = 32;
  int16_t real[len];
  int16_t imag[len];
  while((USB==*mode) || (LSB==*mode)){
    for( int i=0; i<len; ++i){
      /** Fetch new data */
      msg_t m = chMBFetchTimeout(inbox, &c.msg, TIME_MS2I(10));
      if(MSG_OK != m)
        return m;
      real[i] = c.real;
      imag[i] = c.imag;
      if(LSB==*mode)
        imag[i]=-imag[i];
      /** Process the received data */
      int16_t out;
      uint8_t j = (i-16)&31; // get to the center of the data
#ifdef HIGH_EMPH // Remove low frequencies
      int32_t h = hilbert32(imag, i);
      out = real[j] - (h>>16);
#else // Keep low frequencies
      int32_t h = hilbert32(real, i);
      out = imag[j] + (h>>16);
#endif
      /** Fill buffer for audio out
       * We could optionally feed more than one sample in at a time*/
      speakerUpdate(&out, 1);
    }
  }
  return MSG_OK;
}

msg_t ssb_tx(mailbox_t * inbox, enum radio_mode * mode, struct synth * txclk){
  union complex c;
  const int len = 32;
  const int sample_rate = 5000;
  int16_t raw[len];
  int16_t phase[len];
  while((USB==*mode) || (LSB==*mode)){
    for( int i=0; i<len; ++i){
      /** Fetch new data */
      msg_t m = chMBFetchTimeout(inbox, &c.msg, TIME_MS2I(10));
      if(MSG_OK != m)
        return m;
      raw[i] = c.real;
      /** Process the received data */
      int16_t imag = hilbert32(raw, i)>>16;
      if(LSB==*mode)
        imag=-imag;
      uint8_t j = (i-16)&31; // Match group delay of Hilbert transform
      int16_t real = raw[j]; //TODO: Match frequency response of Hilbert transform
      phase[i]=arctan3(imag, real);
      int16_t amp=magn(imag, real);
      int32_t freq = diff32(phase,i);
      freq *= sample_rate;
      /** Update the outputs
       * TODO: Fix amplitude based on frequency
       * TODO: Update DAC more frequently than si5351. Use DMA
       */
      synthSetBaseband(txclk, freq);
      dacPutChannelX(&DACD1, 1, amp);
    }
  }
  return MSG_OK;
}
