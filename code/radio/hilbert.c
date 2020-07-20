/*
 * hilbert.c
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */

#include "hilbert.h"

int16_t hilbert19(int16_t * src){
  const size_t M = 19/2;
  /* Hilbert coeffecients with a hamming window
   * Values are 2/pi, 2/3pi, 2/7pi, 2/9pi, etc
   * Stored in 15.16 fixed-point
   */
  const int16_t coef[] = {40564, 10709, 3839, 1118, 371};
  int32_t sum;
  sum  = (src[M-1]-src[M+1]) * (int32_t)coef[0];
  sum += (src[M-3]-src[M+3]) * (int32_t)coef[1];
  sum += (src[M-5]-src[M+5]) * (int32_t)coef[2];
  sum += (src[M-7]-src[M+7]) * (int32_t)coef[3];
  sum += (src[M-9]-src[M+9]) * (int32_t)coef[4];
  return sum>>16;
}
