//test suite
#include "hilbert.h" 
#include "metrology.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
const float tau = 2 * 3.141592;

void logspace(float * buf, int n, float start, float end){
  start = logf(start);
  end = logf(end);
  float inc = (end-start)/(n-1);
  const float e = 2.71828;
  for( int i = 0; i < n; ++i ){
    buf[i] = pow(e,(start+i*inc));
  }
}

int main( int argc, char ** argv){
#ifdef MAGTEST
  if( argc >= 4 ){
    float y1 = std::stod(argv[1]);
    float y2 = std::stod(argv[2]);
    float y3 = std::stod(argv[3]);
    float x = estimate_peak(y1, y2, y3);
    std::cout<<y1<<", "<<y2<<", "<<y3<<" -> "<<x<<std::endl;
  } else {
    std::cerr<<"Usage: "<<argv[0]<<" y1 y2 y3"<<std::endl;
    return 1;
  }
#endif
#ifdef HILTEST
  int size = 1000;
  int order = 4;
  float * a = new float[order*2];
  float * b = new float[order*2];
  float * Qm = new float[order];
  float * Im = new float[order];
  std::cout << "Initializing hilbert transform...";
  hilbert_init(b, a, order);
  std::cout << "done."<<std::endl<<"Creating chirp...";
  float * x = new float[size];
  logspace(x, size, 5, 200);
  std::cout << "done."<<std::endl<<"Processing filter...";
  std::ofstream log;
  log.open("result.csv");
  log<<"x\tf\tI\tQ\tmag\t"<<std::endl;
  for( int i = 1; i < size; ++i){
    float f = sin(tau*x[i]);
    log<<x[i]<<'\t'<<f<<'\t';
    float I = simple_iir(f,a,Im,order)/360;
    float Q = simple_iir(f,b,Qm,order)/130e3;
    float mag = sqrt(I*I+Q*Q);
    log<<I<<'\t';
    log<<Q<<'\t';
    log<<mag<<'\t';
    log<<std::endl;
  }
  std::cout<<"done."<<std::endl;
  log.close();
  delete[] x;
  delete[] a;
  delete[] b;
  delete[] Qm;
  delete[] Im;
#endif
    return 0;
}
