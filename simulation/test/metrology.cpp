/* Contains functions for measuring phase, amplitude, and frequency */

#include <vector>

static enum state{
  pos_inc = 0b00,
  pos_dec = 0b01,
  neg_inc = 0b10,
  neg_dec = 0b11
};

/* Finds peaks and zero-crossings.
   Returns the number of nodes found.
 */
int find_nodes( vector<float> nodes, vector<float> y){
  short state = pos_inc;
  for(auto i = y.begin(); i != y.end()-1; ++i ){
    state <<= 2;
    state |= y[i]<0.0 ? neg_inc : pos_inc;
    state |=
