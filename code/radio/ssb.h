/*
 * ssb.h
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */
#include <stdint.h>
#include <stddef.h>
#include "rx.h"

/** Single sideband decoder
 *
 */
msg_t ssb_rx(mailbox_t * inbox, enum radio_mode * mode);

/** Singgle sideband encoder
 *
 */
void ssb_tx(int16_t * amp, int32_t * freq, int16_t * src, size_t qty);
