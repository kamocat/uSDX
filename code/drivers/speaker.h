#include "hal.h"
#include <stdint.h>

const size_t spkr_buffer_len = 64;

void speakerStart(void);
void speakerStop(void);
void speakerUpdate(int16_t data, int len);
