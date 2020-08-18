#include "test.h"
#include "hal.h"
#include "drivers/speaker.h"
#include <math.h> // for sin()
#include "num2string.h" // for format_int()


const SerialConfig scfg = {
  .speed = 115200,
};

void serial_test(char * msg){
  sdStart(&SD1, &scfg);
  chnWrite(&SD1, (const uint8_t *)msg, strlen2(msg));
}

void speaker_simple_test(float freq){
  PWMConfig spkr = {
    .frequency = 32e6, // Run at full processor speed
    .period = (1<<10), // 11 bits of data to give a 16 kHz update rate
    .callback = NULL,
    .channels = {
     {PWM_OUTPUT_DISABLED, NULL},
     {PWM_OUTPUT_DISABLED, NULL},
     {PWM_OUTPUT_ACTIVE_HIGH, NULL},
     {PWM_OUTPUT_DISABLED, NULL}
    },
    .cr2 = 0,
    .dier = 0,
  };
  pwmStart(&PWMD1, &spkr);
  const float tau = 6.2832;
  float step = freq * tau / 10e3;
  float x = 0;
  while(1){
    x += step;
    if(x > tau)
      x -= tau;
    int16_t val = (1<<9)*(sin(x)+1);
    pwmEnableChannel(&PWMD1, 2, val);
    chThdSleepMicroseconds(100);
  }
}

void speaker_streaming_test(float freq){
  const float tau = 6.2832;
  float step = freq * tau / 10e3;
  float x = 0;
  const int len = 32;
  int16_t audio_buf[len];
  speakerStart(10e3);
  while(1){
    for(int i = 0; i < len; ++i){
      x += step;
      if(x > tau)
        x -= tau;
      audio_buf[i] = (1<<9)*(sin(x)+1);
    }
    speakerUpdate(audio_buf, len);
  }
}

void adc_simple_test(void){
  const ADCConversionGroup mic_in = {
    FALSE, // single-shot
    1,     // 1 channel
    NULL,  // No callback
    NULL,
    ADC_CFGR1_RES_12BIT,                              /* CFGR1 */
    ADC_TR(0, 0),                                     /* TR */
    ADC_SMPR_SMP_1P5,                                 /* SMPR */
    ADC_CHSELR_CHSEL5                                /* CHSELR */
  };
  sdStart(&SD1, &scfg);
  chnWrite(&SD1, (const uint8_t *)"Reading ADC channel 5:\r\n", 24);
  adcStart(&ADCD1, NULL);
  uint16_t sample;
  while(1){
    adcConvert(&ADCD1, &mic_in, &sample, 1);
    char msg[20];
    int digits = format_int(msg, sample);
    memcpy2(msg+digits, "\r\n", 2);
    chnWrite(&SD1, (const uint8_t *)msg, digits+2);
    chThdSleepMilliseconds(100);
  }

}

void encoder_test(void){
  const QEIConfig encoder = {
      .mode       = QEI_COUNT_BOTH,
      .channels   = {{QEI_INPUT_NONINVERTED},{QEI_INPUT_NONINVERTED}},
      .range      = 1000
  };
  sdStart(&SD1, &scfg);
  qeiStart(&QEID3, &encoder);
  qeiEnable(&QEID3);
  while(1){
    int position = qeiGetPositionI(&QEID3);
    char msg[20];
    int digits = format_int(msg, position);
    memcpy2(msg+digits, "\r\n", 2);
    chnWrite(&SD1, (const uint8_t *)msg, digits+2);
    chThdSleepMilliseconds(100);
  }
}
void i2c_simple_test(void){
  const I2CConfig i2c_cfg; // No configuration?
  i2cStart(&I2CD1, &i2c_cfg);
  sdStart(&SD1, &scfg);
  chnWrite(&SD1, (const uint8_t *)"Starting I2C address scan...", 28);
  /* Check all valid i2c addresses */
  for(short i = 0b1000; i <0b1111000; ++i ){
    msg_t result = i2cMasterTransmitTimeout(&I2CD1, i, NULL, 0, NULL, 0, TIME_MS2I(10));
    if(I2C_NO_ERROR == result){ // ACK received
      char buf[32];
      char * tmp = memcpy2(buf, "\r\nACK ", 6);
      tmp += format_hex(tmp, i);
      chnWrite(&SD1, (const uint8_t *)buf, tmp-buf);
    }
  }
  chnWrite(&SD1, (const uint8_t *)"\r\nDone.\r\n", 9);
}
