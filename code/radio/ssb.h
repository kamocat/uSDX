/*
 * ssb.h
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */
#include <stdint.h>
#include <stddef.h>

/** Single sideband decoder
 *
 */
void ssb_rx(int16_t * dest, int16_t * src, size_t qty);

/** Singgle sideband encoder
 *
 */
void ssb_tx(int16_t * amp, int32_t * freq, int16_t * src, size_t qty);
