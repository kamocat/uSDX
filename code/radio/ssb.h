/*
 * ssb.h
 *
 *  Created on: Jul 9, 2020
 *      Author: marshal
 */
#include <stdint.h>
#include <stddef.h>
#include "rx.h"
#include "../drivers/si5351.h"

/** Single sideband decoder
 *
 */
msg_t ssb_rx(mailbox_t * inbox, enum radio_mode * mode);

/** Singgle sideband encoder
 *
 */
msg_t ssb_tx(mailbox_t * inbox, enum radio_mode * mode, struct synth * txclk);
