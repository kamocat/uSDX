#include <cmath>

// Order must be even
void hilbert_init(float * Qk, float * Ik, int order);


float simple_iir(float in, float * coef, float * mem, int order);
