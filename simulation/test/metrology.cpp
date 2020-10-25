/* Contains functions for measuring phase, amplitude, and frequency */

#include "metrology.h"


/* Estimates the peak using a quadratic model.
 * Assumes evenly-spaced points (y1 at x=-1, y2 at x=0, y3 at x=1)
 * Assumes peak is between y1 and y3
 * Returns the estimated location of the peak.
 */
float estimate_peak(float y1, float y2, float y3){
  float a, b;
  a = y3 + y1 - y2;
  b = (y1 - y3) / 2;
  return b/a;
}

/* Finds peaks and zero-crossings.
   Returns the number of nodes found.
 */
int find_nodes( vector<float> nodes, vector<float> y){
  const short sign = 0b01;
  const short slope = 0b10;
  short state = 0;
  float prev = 0;
  float cur = 0;
  int i = -1;
  for(auto next = y.begin(); i != y.end(); ++i ){
    state <<= 2;
    state |= cur<0.0 ? sign : 0;
    state |= *next>cur ? slope : 0;
    if( (state>>2 ^ state) & sign){ // sign changed
      // Interpolate zero crossing
      float x = cur/(cur-prev);
      nodes.push_back(x+i);
    }
    if( (state>>2 ^ state) & slope){ // Inflection point
      float x = estimate_peak(prev, cur, *next);
      nodes.push_back(x+i);
    }
    ++i;
    prev = cur;
    cur = *next;
  }
  return nodes.size();
}
