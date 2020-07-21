/*
 * hilbert.c
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */

#include "hilbert.h"

int32_t hilbert19(int16_t * src){
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
  return sum;
}

int32_t hilbert32(int16_t * src, uint8_t i){
  i -= 16; // Move to the center of the history
  const uint8_t mask = 32-1;
  /* Hilbert coeffecients with a hamming window
   * Values are 2/pi, 2/3pi, 2/7pi, 2/9pi, etc
   * Stored in 15.16 fixed-point
   */
  const int16_t coef[] = {41353, 12829, 6638, 3753, 2087, 1079, 506, 247};
  int32_t sum;
  sum  = (src[(i- 1)&mask]-src[(i+ 1)&mask]) * (int32_t)coef[0];
  sum += (src[(i- 3)&mask]-src[(i+ 3)&mask]) * (int32_t)coef[1];
  sum += (src[(i- 5)&mask]-src[(i+ 5)&mask]) * (int32_t)coef[2];
  sum += (src[(i- 7)&mask]-src[(i+ 7)&mask]) * (int32_t)coef[3];
  sum += (src[(i- 9)&mask]-src[(i+ 9)&mask]) * (int32_t)coef[4];
  sum += (src[(i-11)&mask]-src[(i+11)&mask]) * (int32_t)coef[5];
  sum += (src[(i-13)&mask]-src[(i+13)&mask]) * (int32_t)coef[6];
  sum += (src[(i-15)&mask]-src[(i+15)&mask]) * (int32_t)coef[7];
  return sum;
}
