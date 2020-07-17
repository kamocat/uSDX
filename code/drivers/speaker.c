#include "speaker.h"


static const DMAConfig spkr_dma = {
  .stream = STM32_ICU1_CH1_DMA_STREAM,
  .channel = STM32_ICU1_CH1_DMA_CHANNEL,
  .dma_priority = STM32_ICU1_CH1_DMA_PRIORITY,
  .irq_priority = STM32_ICU1_CH1_DMA_IRQ_PRIORITY,
  .periph_addr = &TIM3->DMAR,
  .direction = DMA_DIR_P2M,
  .psize = 2,
  .msize = 2,
  .inc_peripheral_addr = false,
  .inc_memory_addr = true,
  .circular = true,
  .error_cb = &error_cb,
  .end_cb = &end_cb,
  .mburst = 8,
  .fifo = 4
};

//DMA channels is STM32_PWM_TIM3_DMA_STREAM

void speakerUpdate(int16_t data, int len);

/** Copies audio data to PWM
 *
 */
void speaker_callback(PWMDriver * pwmp){
//FIXME: Copy data into DMA buffer. Or interrupt every period.
}

static PWMConfig spkr = {
  .frequency = 4096000,                                    /* 4MHz PWM clock frequency.   */
  .period = 4096,                                      /* Initial PWM period 1ms.       */
  .callback = speaker_callback,
  .channels = {
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  .cr2 = 0,
  .dier = STM32_TIM_DIER_CC1DE,   // DMA request on channel 1 compare match
};

void speakerStart(void){
  pwmStart(&PWMD3, &spkr);
}

void speakerStop(void){
  pwmStop(&PWMD3);
}
