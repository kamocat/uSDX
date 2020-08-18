#include "speaker.h"

/** Currently the strategy is to use DMA to transfer data
 * from a buffer to the PWM driver, updating every cycle.
 * However, this is impractical for several reasons:
 * 1, this makes the audio sample rate tied to the PWM top value,
 * meaning we can't pick an arbitrary sample rate
 * 2, a 16-bit PWM signal would have an update rate of 488Hz, effectively
 * generating a 900Hz tone that overpowers our audio. Obviously this is
 * undesirable. An 11-bit PWM signal could have an update rate of 15kHz,
 * meaning it generates a 30kHz tone. But to feed this with DMA, we would
 * have to upsample from 5kHz, taking up RAM with duplicate data.
 *
 * The simpler approach is to run a fast interrupt on a different timer,
 * allowing us to decouple the PWM generation from the audio sample rate.
 * Even with reduced audio output resolution, high input resolution still has
 * advantages like increased dynamic range and noise rejection.
 */

objects_fifo_t spkr_fifo;

void speakerInit(void){
  size_t objsize = spkr_buffer_len*sizeof(int16_t);
  msg_t * msg_buf = chCoreAllocFromBase(spkr_fifo_len*sizeof(msg_t), sizeof(msg_t), 0);
  void * obj_buf = chCoreAllocFromBase(objsize*spkr_fifo_len, 4, 0);
  chFifoObjectInit(&spkr_fifo, objsize, spkr_fifo_len, obj_buf, msg_buf);
}

msg_t speakerUpdate(int16_t * data, int len){
  static int16_t * buf = 0;
  static size_t i = 0;
  for(int n = 0; n < len; ++n){
    if(!buf){
      i = 0;
      buf = (int16_t *)chFifoTakeObjectTimeout(&spkr_fifo, TIME_IMMEDIATE);
      if(!buf)
        return MSG_TIMEOUT;
    }
    buf[i]=data[n];
    ++i;
    if(i>=spkr_buffer_len){
      chFifoSendObject(&spkr_fifo, (void*)buf);
      buf=0;
    }
  }
  return MSG_OK;
}

/** Copies audio data to PWM
 *
 */
void speaker_callback(GPTDriver * gptp){
  static size_t i=0;
  static uint16_t * buf = 0;
  (void)gptp;   // We don't use the gpt driver info
  if(!buf){
    //get next buffer
    msg_t result = chFifoReceiveObjectTimeout(&spkr_fifo, (void**)&buf, TIME_IMMEDIATE);
    //If fifo is empty, return.
    if(MSG_TIMEOUT == result)
      return;
  }
  // Convert signed integer to unsigned with offset
  buf[i] += 0x8000;
  // Scale to 11 bits
  buf[i]>>=(16-11);
  // Update PWM
  pwmEnableChannel(&PWMD3, 0, buf[i]);
  // increment to next sample
  ++i;
  if(i >= spkr_buffer_len){
    //Free this buffer
    chFifoReturnObject(&spkr_fifo, buf);
    buf = NULL;
  }
}

static PWMConfig spkr = {
  .frequency = 32e6, // Run at full processor speed
  .period = (1<<10), // 11 bits of data to give a 16 kHz update rate
  .callback = NULL,
  .channels = {
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  .cr2 = 0,
  .dier = 0,
};

static GPTConfig gpt_config = {
   .callback = speaker_callback,
};

void speakerStart(float sample_rate){
  pwmStart(&PWMD3, &spkr);
  gpt_config.frequency = sample_rate;
  gptStart(&GPTD14, &gpt_config);
  gptStartContinuous(&GPTD14, 1);
}

void speakerStop(void){
  pwmStop(&PWMD3);
  gptStopTimer(&GPTD14);
  gptStop(&GPTD14);
}
