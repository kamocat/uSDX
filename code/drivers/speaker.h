#include "hal.h"
#include <stdint.h>

/// The number of samples in one fifo element. This is used to reduce overhead.
static const size_t spkr_buffer_len = 16;
/// The number of elements in the fifo. This must be at least 3.
static const size_t spkr_fifo_len = 4;

/** Starts the speaker PWM
 *
 * @param sample_rate How quickly you want the samples to be played.
 */
void speakerStart(float sample_rate);
/* Stops the speaker PWM
 *
 */
void speakerStop(void);

/** Copies data into speaker fifo
 *
 * @param data 16-bit signed data to write to speaker. Note that the audio might not be 16-bit resolution
 * @param len The number of samples top copy
 *
 * @returns MSG_OK on sucess, MSG_TIMEOUT if not all data fit
 */
msg_t speakerUpdate(int16_t * data, int len);


/** Copies audio data to PWM
 *
 * This function is called via a timer callback, and should
 * not be called explicitly under normal circumstances.
 */
void speaker_callback(GPTDriver *gpt);

/** Allocates memory for speaker fifo
 *
 * This function must only be called once.
 * It is not automatically called by halInit().
 */
void speakerInit(void);
