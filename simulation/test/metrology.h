/* Contains functions for measuring phase, amplitude, and frequency */

#include <vector>

struct point{
  float x;
  float y;
};


/* Estimates the peak using a quadratic model.
 * Assumes evenly-spaced points (y1 at x=-1, y2 at x=0, y3 at x=1)
 * Assumes peak is between y1 and y3
 * Returns the estimated location of the peak.
 */
float estimate_peak(float y1, float y2, float y3);

/* Finds peaks and zero-crossings.
   Returns the number of nodes found.
 */
int find_nodes( std::vector<struct point> &nodes, std::vector<float> &y);
