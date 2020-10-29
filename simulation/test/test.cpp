//test suite
#include "hilbert.h" 
#include "metrology.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
const float tau = 2 * 3.141592;

std::vector <float> logspace(float start, float end, int n){
  start = logf(start);
  end = logf(end);
  float inc = (end-start)/(n-1);
  const float e = 2.71828;
  std::vector <float> buf;
  for( int i = 0; i < n; ++i ){
     float x = pow(e,(start+i*inc));
     buf.push_back(x);
  }
  return buf;
}

std::vector <float> linspace(float start, float end, int n){
  float inc = (end-start)/(n-1);
  std::vector <float> x;
  for( int i = 0; i < n; ++i ){
    x.push_back(start+i*inc);
  }
  return x;
}

void save_points(std::vector <struct point> p, const char * fname){
  std::ofstream log;
  log.open(fname);
  for(auto i = p.begin(); i!=p.end(); ++i){
    log<<i->x<<'\t'<<i->y<<std::endl;
  }
  log.close();
}


int main( int argc, char ** argv){
#ifdef PEAK
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
#ifdef PHASE
  std::vector <float> x = linspace(0, 20, 100);
  std::vector <float> y, I, Q;
  class Hilbert h(8);
  for(auto i = x.begin(); i!=x.end(); ++i){
    float val = sin(*i);
    y.push_back(val);
    I.push_back(h.ProcessI(val));
    Q.push_back(h.ProcessQ(val));
  }
  std::vector <struct point> n, nI, nQ;
  find_nodes(n, y);
  save_points(n, "Y.csv");
  find_nodes(nI, I);
  save_points(nI, "I.csv");
  find_nodes(nQ, Q);
  save_points(nQ, "Q.csv");
  
#endif
#ifdef HILTEST
  int size = 1000;
  int order = 4;
  std::cout << "Initializing hilbert transform...";
  class Hilbert h(order);
  std::cout << "done."<<std::endl<<"Creating chirp...";
  std::vector <float> x = logspace(5, 50, size);
  std::cout << "done."<<std::endl<<"Processing filter...";
  std::ofstream log;
  log.open("result.csv");
  log<<"x\tf\tI\tQ\tmag\t"<<std::endl;
  for( auto i = x.begin(); i != x.end(); ++i){
    float f = sin(*i * tau);
    log<<*i<<'\t'<<f<<'\t';
    float I = h.ProcessI(f)/130e3;
    float Q = h.ProcessQ(f)/360;
    float mag = sqrt(I*I+Q*Q);
    log<<I<<'\t';
    log<<Q<<'\t';
    log<<mag<<'\t';
    log<<std::endl;
  }
  std::cout<<"done."<<std::endl;
  log.close();
#endif
    return 0;
}
