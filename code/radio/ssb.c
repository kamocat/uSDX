/*
 * ssb.c
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */
#include "ssb.h"
#include "hilbert.h"
#include "../drivers/speaker.h"

msg_t ssb_rx(mailbox_t * inbox, enum radio_mode * mode){
  union complex c;
  const int len = 32;
  int16_t real[len];
  int16_t imag[len];
  while((USB==*mode) || (LSB==*mode)){
    for( int i=0; i<len;){
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

const int32_t fscale = 1; //FIXME

void ssb_tx(int16_t * amp, int32_t * freq, int16_t * src, size_t qty){
  // Source data is single-channel audio
  int32_t f = freq[0];
  for(size_t i = 0; i < (qty-18); ++i){
    amp[i]=src[i+9];
    // FIXME: Subtraction is not a differentiator
    // Maybe the differentiation can be combined with the hilbert?
    f -= hilbert19(src+i) * fscale;
    freq[i]=f;
  }
}
