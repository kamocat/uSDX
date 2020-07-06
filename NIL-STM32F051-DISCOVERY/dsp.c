/*
 * dsp.c
 *
 *  Created on: Jul 6, 2020
 *      Author: marshal
 */
#include "dsp.h"
#include "radio/rx.h"
/*
 * ADC streaming callback.
 *
 * We sampled at 500kHz, but our bandwidth of interest is only
 * 2kHz. (Since it's quadrature sampling, nyquist rate is 1x)
 * We use a second-order IIR filter to prevent aliasing, followed
 * by an accumulate for decimation
 */
static void adccallback(ADCDriver *adcp) {
  adcsample_t * ptr = adcp->samples;
  size_t len = adcp->depth/2; // This also determines downsample ratio
  // These are persistent variables for filter history
  static uint32_t a1 = 0;
  static uint32_t b1 = 0;
  static uint32_t a2 = 0;
  static uint32_t b2 = 0;
  // These are temporary variables to return the downsampled signal
  uint32_t c1 = 0;
  uint32_t c2 = 0;
  /* This determines the corner frequency of our IIR filter.
   * It's actually 1/log2 of the corner
   */
  const uint8_t corner = 7;
  for(unsigned int i=0; i<len; ++i){
    a1 += *ptr++ - (a1>>corner); // 12+7 = 19 bits
    b1 += a1 - (b1>>corner);     // 19+7 = 26 bits
    a2 += *ptr++ - (a2>>corner);
    b2 += a2 - (b2>>corner);
    // If len>64, we need to shift right to prevent overflow.
    const uint8_t shift = 0;
    c1 += b1>>shift;             // 26+6=32 bits
    c2 += b2>>shift;
  }
  union complex c;
  c.x.real = (c1>>16) - 0x8000;
  c.x.imag = (c2>>16) - 0x8000;
  chMBPostTimeout(&new_sample, &c.msg, TIME_IMMEDIATE);
}

static void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {

  (void)adcp;
  (void)err;
}

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 8 samples of 1 channel, SW triggered.
 * Channels:    IN10.
 */
static const ADCConversionGroup qsd_in = {
  TRUE, // continuous
  2,     // 2 channels
  adccallback,
  adcerrorcallback,
  ADC_CFGR1_RES_12BIT,                              /* CFGR1 */
  ADC_TR(0, 0),                                     /* TR */
  ADC_SMPR_SMP_1P5,                                 /* SMPR */
  ADC_CHSELR_CHSEL6 | ADC_CHSELR_CHSEL7             /* CHSELR */
};


/** ADC init
 */
void adc_rx_init(void){
  static char re_init = 0;
  static adcsample_t * samples;
  const int len = 64;
  const int mb_size = 16; //< Mailbox length
  if(!re_init){
    re_init=1;
    // Allocate MxN buffer for continuous ADC
    samples = chCoreAllocFromBase(
        len * qsd_in.num_channels * sizeof(adcsample_t),
        sizeof(adcsample_t), 0);
    // Allocate mailbox for data to radio_rx
    msg_t * msg_buf = chCoreAllocFromBase(mb_size*sizeof(msg_t), sizeof(msg_t), 0);
    // Reset the mailbox
    chMBObjectInit(&new_sample, msg_buf, mb_size);
  }
  // Start the continuous conversion
  adcStart(&ADCD1, NULL);
  adcStartConversion(&ADCD1, &qsd_in, samples, len);

}

