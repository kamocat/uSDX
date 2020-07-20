/*
 * si5351.h
 *
 *  Created on: Jul 8, 2020
 *      Author: marshal
 */

#ifndef SI5351_H_
#define SI5351_H_

#include "hal.h"

struct synth{
  float carrier; //< in Hz
  int32_t baseband; //< 15.16 fixed-point, in Hz
  struct { //< Calculated values for setting registers
    uint64_t pll:38;    //< 11.27 fixed-point for PLL multipler
    uint64_t synth:38;  //< 11.27 fixed-point for Synth Divider
    uint8_t divide:2;   //< 4-bit field for extra divider settings
    int8_t shift;      //< Shift value for scaling the baseband frequency
  }reg;
  uint8_t channel:2; //< Supports four channels - increase bit width if more are desired
  uint8_t PLLx:1;  //< 0 is PLLA, 1 is PLLB
  uint8_t phase:7;   //< Phase offset, in increments of 1/(vxco*4) seconds
};

void synthInit(struct synth * cfg, uint8_t channel, uint8_t pll);
void synthStart(struct synth * cfg);
void synthSetCarrier(struct synth * cfg, float carrier);
void synthSetBaseband(struct synth * cfg, int32_t baseband);
void synthWriteParam(uint8_t reg, uint64_t val, uint8_t div);
void synthWriteConfig(struct synth * cfg);
/* Sets the initial phase offset
 *
 * Note that this has limited resolution and range. The maximum delay is 31.75x the PLL clock period,
 * which means you will have a difficult time getting a 90-degree phase shift on the 80m band.
 */
void synthSetPhase(struct synth * cfg, float degrees);
void synthStop(struct synth * cfg);


#endif /* SI5351_H_ */
