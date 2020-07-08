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

void synth_init(struct synth * cfg, uint8_t channel, uint8_t pll);
void synth_en(struct synth * cfg);
void synth_set_carrier(struct synth * cfg, float carrier);
void synth_set_baseband(struct synth * cfg, int32_t baseband);
void synth_write_params(uint8_t reg, uint64_t val, uint8_t div);
void synth_phase(struct synth * cfg, uint8_t phase);


#endif /* SI5351_H_ */
