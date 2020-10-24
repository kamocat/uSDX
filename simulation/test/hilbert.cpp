#include "hilbert.h"

void hilbert_init(float * a, float * b, int order){
  const float epi = 535.49; //pow(e,pi)

  // Create coeffecients
  for( int i = 0; i<order; ++i ){
    a[i] = powf(epi, powf(2,-i));
    b[i] = powf(epi, powf(2,-i-1));
    a[i+order] = 1/a[i];
    b[i+order] = 1/b[i];
  }
}

float simple_iir(float in, float * coef, float * mem, int order){
  for( int i = 0; i < order; ++i ){
    in += mem[i] * coef[i+order];
    float out = in - mem[i] * coef[i];
    mem[i] = in;
    in = out;
  }
  return in;
}
