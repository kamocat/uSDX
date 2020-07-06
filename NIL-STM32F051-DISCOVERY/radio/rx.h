/*
 * rx.h
 *
 *  Created on: Jul 6, 2020
 *      Author: marshal
 */

#ifndef RX_H_
#define RX_H_

#include "hal.h"
#include "ch.h"

THD_FUNCTION(radio_rx, arg);

/** Mailbox for received data */
extern msg_t new_sample;

#endif /* RX_H_ */
