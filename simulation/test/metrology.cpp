/* Contains functions for measuring phase, amplitude, and frequency */

#include <vector>

static enum state{
  inc = 0b00,
  dec = 0b01,
  neg = 0b10,
  pos = 0b01
};

/* Finds peaks and zero-crossings.
   Returns the number of nodes found.
 */
int find_nodes( vector<float> nodes, vector<float> y){
  short state = 0;
  state |= y[0]<0.0 ? neg : pos;
  state |= y[1]>y ? inc : dec;
  for(auto i = y.begin()+1; i != y.end()-2; ++i ){
    state |= y[i]<0.0 ? neg : pos;
    state |= y[i+1]>y ? inc : dec;
    if( (state>>2 ^ state) & dec){ // sign changed
      // Interpolate zero crossing
      float x = y[i]/(y[i]-y[i-1]);
      nodes.push_back(x+i);
    }
    if( (state>>2 ^ state) & neg){ // Inflection point
      // Estimate peak. We know it's beteen i+1 and i-1
    }
  }
  return nodes.length();
}
