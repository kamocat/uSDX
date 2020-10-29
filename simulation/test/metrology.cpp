/* Contains functions for measuring phase, amplitude, and frequency */

#include "metrology.h"
#include <iostream>


/* Estimates the peak using a quadratic model.
 * Assumes evenly-spaced points (y1 at x=-1, y2 at x=0, y3 at x=1)
 * Assumes peak is between y1 and y3
 * Returns the estimated location of the peak.
 */
float estimate_peak(float y1, float y2, float y3){
  float a, b;
  a = (y3 + y1 - 2*y2) * 0.5;
  b = (y1 - y3) * 0.5;
  float x = 0.5 * b/a;
  #ifdef PEAK
    float peak = a*(x*x) + b*x + y2;
    std::cout<<"a: "<<a<<" b: "<<b<<" peak: "<<peak<<std::endl;
  #endif
  return x;
}

/* Finds peaks and zero-crossings.
   Returns the number of nodes found.
 */
int find_nodes( std::vector<struct point> &nodes, std::vector<float> &y){
  const short sign = 0b01;
  const short slope = 0b10;
  short state = 0;
  float prev = 0;
  float cur = 0;
  int i = -1;
  for(auto next = y.begin(); next != y.end(); ++next ){
    struct point p;
    state <<= 2;
    state |= cur<0.0 ? sign : 0;
    state |= *next>cur ? slope : 0;
    short change = ((state>>2) ^ state) & 0xF;
    if( change & sign){ // sign changed
      // Interpolate zero crossing
      p.x = cur/(cur-prev);
      p.x += i;
      // determine quadrant
      p.y = state & sign ? 3 : 1;
      nodes.push_back(p);
    }
    if( change & slope){ // Inflection point
      p.x = estimate_peak(prev, cur, *next);
      p.x += i;
      // determine quadrant
      p.y = state & slope ? 4 : 2;
      nodes.push_back(p);
    }
    ++i;
    prev = cur;
    cur = *next;
  }
  return nodes.size();
}
