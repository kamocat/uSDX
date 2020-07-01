volatile int8_t encoder_val = 0;
volatile int8_t encoder_step = 0;
static uint8_t last_state;
ISR(PCINT2_vect){  // Interrupt on rotary encoder turn
  //noInterrupts();
  switch(last_state = (last_state << 4) | (digitalRead(ROT_B) << 1) | digitalRead(ROT_A)){ //transition  (see: https://www.allaboutcircuits.com/projects/how-to-use-a-rotary-encoder-in-a-mcu-based-project/  )
//#define ENCODER_ENHANCED_RESOLUTION  1
#ifdef ENCODER_ENHANCED_RESOLUTION // Option: enhance encoder from 24 to 96 steps/revolution, see: appendix 1, https://www.sdr-kits.net/documents/PA0KLT_Manual.pdf
    case 0x31: case 0x10: case 0x02: case 0x23: encoder_val++; break;
    case 0x32: case 0x20: case 0x01: case 0x13: encoder_val--; break;
#else
    case 0x31: case 0x10: case 0x02: case 0x23: if(encoder_step < 0) encoder_step = 0; encoder_step++; if(encoder_step >  3){ encoder_step = 0; encoder_val++; } break;
    case 0x32: case 0x20: case 0x01: case 0x13: if(encoder_step > 0) encoder_step = 0; encoder_step--; if(encoder_step < -3){ encoder_step = 0; encoder_val--; } break;  
#endif
  }
  //interrupts();
}
void encoder_setup()
{
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  PCMSK2 |= (1 << PCINT22) | (1 << PCINT23); // interrupt-enable for ROT_A, ROT_B pin changes; see https://github.com/EnviroDIY/Arduino-SDI-12/wiki/2b.-Overview-of-Interrupts
  PCICR |= (1 << PCIE2); 
  last_state = (digitalRead(ROT_B) << 1) | digitalRead(ROT_A);
  interrupts();
}
/*
class Encoder {
public:
  volatile int8_t val = 0;
  volatile int8_t step = 0;
  uint8_t last_state;
  Encoder(){
    pinMode(ROT_A, INPUT_PULLUP);
    pinMode(ROT_B, INPUT_PULLUP);
    PCMSK2 |= (1 << PCINT22) | (1 << PCINT23); // interrupt-enable for ROT_A, ROT_B pin changes; see https://github.com/EnviroDIY/Arduino-SDI-12/wiki/2b.-Overview-of-Interrupts
    PCICR |= (1 << PCIE2);
    last_state = (digitalRead(ROT_B) << 1) | digitalRead(ROT_A);
    sei();    
  }
  void event(){
    switch(last_state = (last_state << 4) | (digitalRead(ROT_B) << 1) | digitalRead(ROT_A)){ //transition  (see: https://www.allaboutcircuits.com/projects/how-to-use-a-rotary-encoder-in-a-mcu-based-project/  )
      case 0x31: case 0x10: case 0x02: case 0x23: if(step < 0) step = 0; step++; if(step >  3){ step = 0; val++; } break;
      case 0x32: case 0x20: case 0x01: case 0x13: if(step > 0) step = 0; step--; if(step < -3){ step = 0; val--; } break;  
    }
  }
};
Encoder enc;
ISR(PCINT2_vect){  // Interrupt on rotary encoder turn
  enc.event();
}*/
