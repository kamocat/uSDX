/*
 * hilbert.c
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */

#include "hilbert.h"

int32_t hilbert16(int16_t * src, uint8_t i){
  i -= 7; // Move to the center of the history
  const uint8_t mask = 16-1;
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
  return sum;
}

int32_t hmatch16(int16_t * src, uint8_t i){
  return src[(i-6)&15] - src[(i-8)&15];
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

int32_t diff32(int16_t * src, uint8_t i){
  i -= 16; // Move to the center of the history
  const uint8_t mask = 32-1;
  /* Differentiation coeffecients with a hamming window
   * Values are -1, 1/2, -1/3, 1/4, etc
   * Stored in 15.16 fixed-point
   */
  const int16_t coef[] = {-4055,1967,-1245,868,-631,466,-344,252,-181,127,-86,57,-38,26,-22};
  int32_t sum = 0;
  for(uint8_t j = 0; j<15; ++i){
    sum  += (src[(i-1-j)&mask]-src[(i+1+j)&mask]) * (int32_t)coef[j];
  }
  return sum;
}
