/*
 * ssb.c
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */
#include "ssb.h"

void ssb_rx(int16_t * dest, int16_t * src, size_t qty){
  // Source data is interlieved In-phase / Quadrature phase
  // Destination is single-channel
  for(int i = 0; i < qty; ++i){
    *dest++ = *src++ + *src++; // Return the sum of I and Q
  }
}

void ssb_tx(int16_t * amp, int32_t * freq, int16_t * src, size_t qty){
  // Source data is single-channel audio
}
