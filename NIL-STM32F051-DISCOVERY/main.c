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
/*
 * Thread 1.
 */
THD_WORKING_AREA(waThread1, 128);
THD_FUNCTION(Thread1, arg) {

  (void)arg;
  /* Audio test: copy audio in to audio out */
  while (true) {
    /* Read MIC analog input, output to SPKR (pwm ch1 on tim3) */
    uint16_t sample;
    adcConvert(&ADCD1, &mic_in, &sample, 1);
    //FIXME: Write to PWM
    chThdSleepMilliseconds(1);
  }
}


static const DACConfig dac1cfg1 = {
  .init         = 2047U,
  .datamode     = DAC_DHRM_12BIT_RIGHT,
  .cr           = 0
};
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

  while (true) {
    chThdSleepMilliseconds(250);
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
