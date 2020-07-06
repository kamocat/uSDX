/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "hal.h"
#include "ch.h"
#include "nil_test_root.h"
#include "oslib_test_root.h"

static void adcerrorcallback(ADCDriver *adcp, adcerror_t err) {

  (void)adcp;
  (void)err;
  chnWrite(&SD1, (const uint8_t *)"ADC Error\r\n", 11);
}

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 8 samples of 1 channel, SW triggered.
 * Channels:    IN10.
 */
static const ADCConversionGroup mic_in = {
  FALSE, // single-shot
  1,     // 1 channel
  NULL,  // No callback
  adcerrorcallback,
  ADC_CFGR1_RES_12BIT,                              /* CFGR1 */
  ADC_TR(0, 0),                                     /* TR */
  ADC_SMPR_SMP_1P5,                                 /* SMPR */
  ADC_CHSELR_CHSEL5                                /* CHSELR */
};

static PWMConfig spkr = {
  4096000,                                    /* 4MHz PWM clock frequency.   */
  4096,                                      /* Initial PWM period 1ms.       */
  NULL,
  {
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  0,
  0
};
/*
 * Thread 1.
 */
THD_WORKING_AREA(waThread1, 128);
THD_FUNCTION(Thread1, arg) {

  (void)arg;
  /* Audio test: copy audio in to audio out */
  adcStart(&ADCD1, NULL);
  pwmStart(&PWMD3, &spkr);
  while (true) {
    /* Read MIC analog input, output to SPKR (pwm ch1 on tim3) */
    uint16_t sample;
    adcConvert(&ADCD1, &mic_in, &sample, 1);
    pwmEnableChannel(&PWMD3, 0, sample); // No scaling for now
    chThdSleepMilliseconds(1);
  }
}


static const DACConfig dac1cfg1 = {
  .init         = 2047U,
  .datamode     = DAC_DHRM_12BIT_RIGHT,
  .cr           = 0
};

/** Enables an output 

Doesn't implement all features, just basic
*/
void si5351_clk_en(uint8_t clk, uint8_t pll){
  uint8_t buf[2];
  buf[0]=clk+16;
  buf[1]= pll ? 0x2F : 0x1F;
  const uint8_t si5351 = 0x60;
  i2cMasterTransmitTimeout(&I2CD1, si5351, buf, 2,
                             NULL, 0, TIME_MS2I(100));
}

/** Configures the si5351 Outputs

The desired divider value is given by a + b/c.
*/
void si5351_clk_set(uint8_t clk, uint32_t a, uint32_t b, uint32_t c){
  uint32_t p1, p2, p3;
  //FIXME: Validate the inputs
  p1 = 128*a + (128*b)/c - 512;
  p2 = 128*b + c*((128*b)/c);
  p3 = c;
  uint8_t buf[9];
  uint8_t reg = clk*8 + 42;
  buf[0]=reg;
  buf[1]=p3>>8;
  buf[2]=p3;
  buf[3]=(0x03&(p1>>16)); //FIXME: Implement "divide by" options
  buf[4]=p1>>8;
  buf[5]=p1;
  buf[6]=(0x0F&(p2>>16)) | (0xF0&(p3>>12));
  buf[7]=p2>>8;
  buf[8]=p2;
  const uint8_t si5351 = 0x60;
  i2cMasterTransmitTimeout(&I2CD1, si5351, buf, 9,
                             NULL, 0, TIME_MS2I(100));
}

/** Configures the si5351 PLLs

The desired value is given by a + b/c.
*/
void si5351_pll_set(uint8_t pll, uint32_t a, uint32_t b, uint32_t c){
  uint32_t p1, p2, p3;
  //FIXME: Validate the inputs
  p1 = 128*a + (128*b)/c - 512;
  p2 = 128*b + c*((128*b)/c);
  p3 = c;
  uint8_t reg = pll ? 34 : 26;  // PLLA 26, PLLB is 34
  uint8_t buf[9];
  buf[0]=reg;
  buf[1]=p3>>8;
  buf[2]=p3;
  buf[3]=p3>>16;
  buf[4]=p1>>8;
  buf[5]=p1;
  buf[6]=(0x0F&(p2>>16)) | (0xF0&(p3>>12));
  buf[7]=p2>>8;
  buf[8]=p2;
  const uint8_t si5351 = 0x60;
  i2cMasterTransmitTimeout(&I2CD1, si5351, buf, 9,
                             NULL, 0, TIME_MS2I(100));
}
/*
 * Thread 2.
 */
THD_WORKING_AREA(waThread2, 128);
THD_FUNCTION(Thread2, arg) {

  (void)arg;
  /* Power amplifier test: Demonstrate DAC and si5351 */
  /* Starting DAC1 driver.*/
  dacStart(&DACD1, &dac1cfg1);
  uint16_t val = 0xFFF/3.3; // 1V desired, 3.3V max.
  dacPutChannelX(&DACD1, 1, val); // Output to DAC
  i2cStart(&I2CD1, NULL);

  while (true) {
    chThdSleepMilliseconds(250);
    si5351_clk_en(2, 1);  // PLLB
    si5351_pll_set(2, 31, 1, 2); // Multiply by 31.5
    si5351_clk_set(2, 121,1, 2); // Divide by 121.5
    // Output should be 7MHz
  }
}
/*
 * Thread 3.
 */
THD_WORKING_AREA(waThread3, 256);
THD_FUNCTION(Thread3, arg) {

  (void)arg;

  /*
   * Activates the serial driver 1 using the driver default configuration.
   * PA9 and PA10 are routed to USART1.
   */
  sdStart(&SD1, NULL);
  /* Welcome message.*/
  chnWrite(&SD1, (const uint8_t *)"Hello World!\r\n", 14);

  /* Waiting for encoder turn and activation of the test suite.*/
  while (true) {
    if (palReadLine(LINE_ENC0)) {
      test_execute((BaseSequentialStream *)&SD1, &nil_test_suite);
      test_execute((BaseSequentialStream *)&SD1, &oslib_test_suite);
    }
    chThdSleepMilliseconds(500);
  }
}

/*
 * Threads creation table, one entry per thread.
 */
THD_TABLE_BEGIN
  THD_TABLE_THREAD(0, "blinker1",     waThread1,       Thread1,      NULL)
  THD_TABLE_THREAD(1, "blinker2",     waThread2,       Thread2,      NULL)
  THD_TABLE_THREAD(4, "tester",       waThread3,       Thread3,      NULL)
THD_TABLE_END

/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /* This is now the idle thread loop, you may perform here a low priority
     task but you must never try to sleep or wait in this loop. Note that
     this tasks runs at the lowest priority level so any instruction added
     here will be executed after all other tasks have been started.*/
  while (true) {
  }
}
