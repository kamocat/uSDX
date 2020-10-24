/* Contains functions for measuring phase, amplitude, and frequency */

#include <vector>

static enum state{
  pos_inc = 0,
  pos_dec,
  neg_inc,
  neg_dec
};

/* Finds peaks and zero-crossings.
   Returns the number of nodes found.
 */
int find_nodes( vector<float> nodes, vector<float> y){
  for(auto i = y.begin(); i != y.end()-1; ++i ){
    
