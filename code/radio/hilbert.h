#include <stdint.h>
#include <stddef.h>


/** 19-element Hilbert transform
 *
 * Generated with hamming window
 */
int32_t hilbert19(int16_t * src);

/** 31-element Hilbert transform
 *
 * This implementation assumes a 32-element buffer, and wraps accordingly
 * @param src The source data. Make sure I and Q are not interlieved
 * @param i The index of the latest element. Older elements are assumed to have decreasing index.
 * @returns The input signal, delayed by 90 degrees (The imaginary component).
 */
int32_t hilbert32(int16_t * src, uint8_t i);

/** 31-element differentiation
 *
 * This implementation assumes a 32-element buffer, and wraps accordingly
 * @param src The source data. Make sure I and Q are not interlieved
 * @param i The index of the latest element. Older elements are assumed to have decreasing index.
 * @returns the derivative of the input
 */
int32_t diff32(int16_t * src, uint8_t i);
