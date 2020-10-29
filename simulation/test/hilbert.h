#include <cmath>

class Hilbert{
public:
  Hilbert(const int order);
  Hilbert(const Hilbert &copy);
  float ProcessI(float sample);
  float ProcessQ(float sample);
  ~Hilbert(void);
private:
  float simple_iir(float in, float * coef, float * mem);
  int len;
  float * Icoef;
  float * Qcoef;
  float * Imem;
  float * Qmem;
};
