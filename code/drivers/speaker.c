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

uint16_t spkr_dma_buf[spkr_buffer_len];

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
  .callback = NULL,
  .channels = {
   {PWM_OUTPUT_ACTIVE_HIGH, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  .cr2 = 0,
  .dier = STM32_TIM_DIER_CC1DE,   // DMA request on channel 1 compare match
};

const uint32_t dma_config = 0
    | STM32_DMA_CR_DIR_M2P      // Memory to peripheral
    | STM32_DMA_CR_CIRC         // Circular buffer
    | STM32_DMA_CR_MINC         // Memory Increment Mode
    | STM32_DMA_CR_PSIZE_WORD   // 16-bit peripheral space
    | STM32_DMA_CR_MSIZE_WORD   // 16-bit buffer width in RAM
    | STM32_DMA_CR_PL(3)        // High priority
    | STM32_DMA_CR_TCIE         // Transfer Complete Interrupt
    | STM32_DMA_CR_HTIE         // Half-transfer Interrupt
    ;

void speakerStart(void){
  static stm32_dma_stream_t * stream =
      dmaStreamAlloc(STM32_PWM_TIM3_DMA_STREAM, 2, speaker_callback, NULL);
  dmaStreamSetPeripheral(stream, &TIM3->DMAR);
  dmaStreamSetMemory0(stream, spkr_dma_buf);
  dmaStreamSetTransactionSize(stream, spkr_buffer_len);
  dmaStreamSetMode(stream, dma_config);
  dmaStreamEnable(stream);
  pwmStart(&PWMD3, &spkr);
}

void speakerStop(void){
  pwmStop(&PWMD3);
  dmaStreamDisable(stream);
}
