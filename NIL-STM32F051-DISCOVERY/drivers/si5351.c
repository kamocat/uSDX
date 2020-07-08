/*
 * si5351.c
 *
 *  Created on: Jul 7, 2020
 *      Author: marshal
 */
#include "si5351.h"
#include "../trig.h"

void synth_en(struct synth * cfg){
  uint8_t buf[2];
  buf[0]=cfg->channel+16;
  buf[1]=cfg->PLLx ? 0x2F : 0x1F;
  const uint8_t si5351 = 0x60;
  i2cMasterTransmitTimeout(&I2CD1, si5351, buf, 2,
                             NULL, 0, TIME_MS2I(100));
}

void synth_write_params(uint8_t reg, uint64_t val, uint8_t div){
  uint8_t buf[9];
  buf[0]=reg*8+26;
  buf[1]=0xFF; // denominator is always 0xFFFFF
  buf[2]=0xFF;
  buf[3]=val>>36 & 0x03;
  buf[3]|=div<<4;
  buf[4]=val>>28;
  buf[5]=val>>20;
  buf[6]=val>>16 | 0xF0;
  buf[7]=val>>8;
  buf[8]=val;

  const uint8_t si5351 = 0x60;
  i2cMasterTransmitTimeout(&I2CD1, si5351, buf, 9, NULL, 0, TIME_MS2I(100));
}

void synth_set_carrier(struct synth * cfg, float carrier){
  // Calculate PLL and Multisynth values
  const double xtal = 27e6;
  double pll = 27; // Good starting value to put vxco at 750MHz
  double div = xtal * pll / carrier;
  double delta = div - (xtal*pll)/(carrier+1); // Linearized change per Hz
  int8_t delta_shift = round(log2(delta)); // round to nearest power of 2
  double delta2 = (0x40000000>>delta_shift)*(1.0/0x40000000U); // (Computing a negative power of 2)
  div *= delta2/delta;
  div = round(div/2)*2+1;   // Minimize chance of rollover
  pll = carrier * div / xtal; // Adjust pll to match

  // If desired frequency is very low, we need to use the additional divider register
  uint8_t div2;
  for(div2=0; div>2048; ++div2){
    div *= 0.5;
    --delta_shift;
  }

  // Store values into config
  const uint32_t fixed_shift = 1<<27;   //For shifting values to fixed-point
  cfg->reg.pll = pll * fixed_shift;
  cfg->reg.synth = div * fixed_shift;
  cfg->reg.shift = delta_shift;
  cfg->reg.divide = div2;

  // Write changes to chip
  synth_write_params(cfg->PLLx, cfg->reg.pll, 0);
  synth_write_params(cfg->channel+2, cfg->reg.synth, cfg->reg.divide);
  synth_en(cfg);
}

void synth_set_baseband(struct synth * cfg, int32_t baseband){
  int32_t diff = baseband - cfg->baseband;
  diff >>= cfg->reg.shift;
  cfg->reg.synth -= diff;
  uint32_t val = cfg->reg.synth;
  uint8_t buf[5];
  /* This could potentially be sped up by dropping the last byte */
  buf[0]=8*(cfg->channel)+46;
  buf[1]=val>>20;
  buf[2]=(val>>16) | 0xF0;
  buf[3]=val>>8;
  buf[4]=val;
  const uint8_t si5351 = 0x60;
  i2cMasterTransmitTimeout(&I2CD1, si5351, buf, 5,NULL, 0, TIME_MS2I(100));
}

void synth_init(struct synth * cfg, uint8_t channel, uint8_t pll){
  cfg->channel = channel;
  cfg->PLLx = pll;
}
