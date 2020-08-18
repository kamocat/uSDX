/*
 * trig.c
 *
 *  Created on: Jul 6, 2020
 *      Author: marshal
 */
#include "trig.h"

inline int16_t arctan3(int16_t q, int16_t i)  // error ~ 0.8 degree
{ // source: [1] http://www-labs.iro.umontreal.ca/~mignotte/IFT2425/Documents/EfficientApproximationArctgFunction.pdf
#define _atan2(z)  (_UA/8 - _UA/22 * z + _UA/22) * z  //derived from (5) [1]
  //#define _atan2(z)  (_UA/8 - _UA/24 * z + _UA/24) * z  //derived from (7) [1]
  int16_t r;
  if(abs(q) > abs(i))
    r = _UA / 4 - _atan2(abs(i) / abs(q));        // arctan(z) = 90-arctan(1/z)
  else
    r = (i == 0) ? 0 : _atan2(abs(q) / abs(i));   // arctan(z)
  r = (i < 0) ? _UA / 2 - r : r;                  // arctan(-z) = -arctan(z)
  return (q < 0) ? -r : r;                        // arctan(-z) = -arctan(z)
}
