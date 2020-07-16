/*
 * hilbert.c
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */

#include <stdint.h>

int16_t hilbert19(int16_t * src){
  const size_t M = 19/2;
  const int32_t coef[M/2+1];
  int32_t sum;
  sum  = (src[M-1]-src[M+1]) * coef[0];
  sum += (src[M-3]-src[M+3]) * coef[1];
  sum += (src[M-5]-src[M+5]) * coef[2];
  sum += (src[M-7]-src[M+7]) * coef[3];
  sum += (src[M-9]-src[M+9]) * coef[4];
  return sum>>16;
}
