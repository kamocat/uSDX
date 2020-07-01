
int8_t prev_bandval = 2;
int8_t bandval = 2;
#define N_BANDS 10
uint32_t band[N_BANDS] = { /*472000, 1840000,*/ 3573000, 5357000, 7074000, 10136000, 14074000, 18100000, 21074000, 24915000, 28074000, 50313000/*, 70101000, 144125000*/ };

enum step_t { STEP_10M, STEP_1M, STEP_500k, STEP_100k, STEP_10k, STEP_1k, STEP_500, STEP_100, STEP_10, STEP_1 };
int32_t stepsizes[10] = { 10000000, 1000000, 500000, 100000, 10000, 1000, 500, 100, 10, 1 };
volatile int8_t stepsize = STEP_1k;
int8_t prev_stepsize[] = { STEP_1k, STEP_500 }; //default stepsize for resp. SSB, CW

void process_encoder_tuning_step(int8_t steps)
{
  int32_t stepval = stepsizes[stepsize];
  //if(stepsize < STEP_100) freq %= 1000; // when tuned and stepsize > 100Hz then forget fine-tuning details
  freq += steps * stepval;
  //freq = max(1, min(99999999, freq));
  change = true;
}

void stepsize_showcursor()
{
  lcd.setCursor(stepsize+1, 1);  // display stepsize with cursor
  lcd.cursor();
}

void stepsize_change(int8_t val)
{
  stepsize += val;
  if(stepsize < STEP_1M) stepsize = STEP_10;
  if(stepsize > STEP_10) stepsize = STEP_1M;
  if(stepsize == STEP_10k || stepsize == STEP_500k) stepsize += val;
  stepsize_showcursor();
}

void powerDown()
{ // Reduces power from 110mA to 70mA (back-light on) or 30mA (back-light off), remaining current is probably opamp quiescent current
  lcd.setCursor(0, 1); lcd.print(F("Power-off 73 :-)")); lcd_blanks();

  MCUSR = ~(1<<WDRF);  // MSY be done before wdt_disable()
  wdt_disable();   // WDTON Fuse High bit need to be 1 (0xD1), if NOT it will override and set WDE=1; WDIE=0, meaning MCU will reset when watchdog timer is zero, and this seems to happen when wdt_disable() is called
  
  timer2_stop();
  timer1_stop();
  adc_stop();

  si5351.powerDown();

  delay(1500);

  // Disable external interrupts INT0, INT1, Pin Change
  PCICR = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;
  // Disable internal interrupts
  TIMSK0 = 0;
  TIMSK1 = 0;
  TIMSK2 = 0;
  WDTCSR = 0;
  // Enable BUTTON Pin Change interrupt
  *digitalPinToPCMSK(BUTTONS) |= (1<<digitalPinToPCMSKbit(BUTTONS));
  *digitalPinToPCICR(BUTTONS) |= (1<<digitalPinToPCICRbit(BUTTONS));

  // Power-down sub-systems
  PRR = 0xff;

  lcd.noDisplay();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  interrupts();
  sleep_bod_disable();
  //MCUCR |= (1<<BODS) | (1<<BODSE);  // turn bod off by settings BODS, BODSE; note BODS is reset after three clock-cycles, so quickly go to sleep before it is too late
  //MCUCR &= ~(1<<BODSE);  // must be done right before sleep
  sleep_cpu();  // go to sleep mode, wake-up by either INT0, INT1, Pin Change, TWI Addr Match, WDT, BOD
  sleep_disable();

  //void(* reset)(void) = 0; reset();   // soft reset by calling reset vector (does not reset registers to defaults)
  do { wdt_enable(WDTO_15MS); for(;;); } while(0);  // soft reset by trigger watchdog timeout
}

void show_banner(){
  lcd.setCursor(0, 0);
#ifdef QCX
  lcd.print(F("QCX"));
  const char* cap_label[] = { "SSB", "DSP", "SDR" };
  if(ssb_cap || dsp_cap){ lcd.print(F("-")); lcd.print(cap_label[dsp_cap]); }
#else
  lcd.print(F("uSDX"));
#endif
  lcd.print(F("\x01 ")); lcd_blanks();
}

const char* mode_label[5] = { "LSB", "USB", "CW ", "AM ", "FM " };

void display_vfo(uint32_t f){
  lcd.setCursor(0, 1);
  lcd.print('\x06');  // VFO A/B

  uint32_t scale=10e6;  // VFO frequency
  if(f/scale == 0){ lcd.print(' '); scale/=10; }  // Initial space instead of zero
  for(; scale!=1; f%=scale, scale/=10){
    lcd.print(f/scale);
    if(scale == (uint32_t)1e3 || scale == (uint32_t)1e6) lcd.print(',');  // Thousands separator
  }
  
  lcd.print(" "); lcd.print(mode_label[mode]); lcd.print("  ");
  lcd.setCursor(15, 1); lcd.print("R");
}

volatile uint8_t event;
volatile uint8_t menumode = 0;  // 0=not in menu, 1=selects menu item, 2=selects parameter value
volatile int8_t menu = 0;  // current parameter id selected in menu

#define pgm_cache_item(addr, sz) byte _item[sz]; memcpy_P(_item, addr, sz);  // copy array item from PROGMEM to SRAM
#define get_version_id() ((VERSION[0]-'1') * 2048 + ((VERSION[2]-'0')*10 + (VERSION[3]-'0')) * 32 +  ((VERSION[4]) ? (VERSION[4] - 'a' + 1) : 0) * 1)  // converts VERSION string with (fixed) format "9.99z" into uint16_t (max. values shown here, z may be removed) 


uint8_t eeprom_version;
#define EEPROM_OFFSET 0x150  // avoid collision with QCX settings, overwrites text settings though
int eeprom_addr;

// Support functions for parameter and menu handling
enum action_t { UPDATE, UPDATE_MENU, LOAD, SAVE, SKIP };
template<typename T> void paramAction(uint8_t action, T& value, const __FlashStringHelper* menuid, const __FlashStringHelper* label, const char* enumArray[], int32_t _min, int32_t _max, bool continuous){
  switch(action){
    case UPDATE:
    case UPDATE_MENU:
      if(((int32_t)value + encoder_val) < _min) value = (continuous) ? _max : _min; 
      else value += encoder_val;
      encoder_val = 0;
      if(continuous) value = (value % (_max+1));
      value = max(_min, min((int32_t)value, _max));
      if(action == UPDATE_MENU){
        lcd.setCursor(0, 0);
        lcd.print(menuid); lcd.print(' ');
        lcd.print(label); lcd_blanks(); lcd_blanks();
        lcd.setCursor(0, 1); // value on next line
      } else { // UPDATE (not in menu)
        lcd.setCursor(0, 1); lcd.print(label); lcd.print(F(": "));
      }
      if(enumArray == NULL){
        if((_min < 0) && (value >= 0)) lcd.print('+');
        lcd.print(value);
      } else {
        lcd.print(enumArray[value]);
      }
      lcd_blanks(); lcd_blanks();
      //if(action == UPDATE) paramAction(SAVE, value, menuid, label, enumArray, _min, _max, continuous, init_val);
      break;
    case LOAD:
      for(uint8_t* ptr = (uint8_t *) &value, n = sizeof(value); n; --n) *ptr++ = eeprom_read_byte((uint8_t *)eeprom_addr++);
      break;
    case SAVE:
      for(uint8_t* ptr = (uint8_t *) &value, n = sizeof(value); n; --n) eeprom_write_byte((uint8_t *)eeprom_addr++, *ptr++);
      break;
    case SKIP:
      eeprom_addr += sizeof(value);
      break;
  }
}
uint32_t save_event_time = 0;
uint32_t sec_event_time = 0;

static uint8_t pwm_min = 0;    // PWM value for which PA reaches its minimum: 29 when C31 installed;   0 when C31 removed;   0 for biasing BS170 directly
static uint8_t pwm_max = 220;  // PWM value for which PA reaches its maximum: 96 when C31 installed; 255 when C31 removed; 220 for biasing BS170 directly

const char* offon_label[2] = {"OFF", "ON"};
const char* filt_label[N_FILT+1] = { "Full", "4000", "2500", "1700", "500", "200", "100", "50" };
const char* band_label[N_BANDS] = { "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m" };

#define _N(a) sizeof(a)/sizeof(a[0])
#define N_PARAMS 26  // number of (visible) parameters
#define N_ALL_PARAMS (N_PARAMS+2)  // number of parameters

enum params_t {ALL, VOLUME, MODE, FILTER, BAND, STEP, AGC, NR, ATT, ATT2, SMETER, CWDEC, CWTONE, CWOFF, VOX, VOXGAIN, MOX, DRIVE, SIFXTAL, PWM_MIN, PWM_MAX, CALIB, SR, CPULOAD, PARAM_A, PARAM_B, PARAM_C, FREQ, VERS};

void paramAction(uint8_t action, uint8_t id = ALL)  // list of parameters
{
  if((action == SAVE) || (action == LOAD)){
    eeprom_addr = EEPROM_OFFSET;
    for(uint8_t _id = 1; _id < id; _id++) paramAction(SKIP, _id);
  }
  if(id == ALL) for(id = 1; id != N_ALL_PARAMS+1; id++) paramAction(action, id);  // for all parameters
  
  const char* stepsize_label[10] = { "10M", "1M", "0.5M", "100k", "10k", "1k", "0.5k", "100", "10", "1" };
  const char* att_label[] = { "0dB", "-13dB", "-20dB", "-33dB", "-40dB", "-53dB", "-60dB", "-73dB" };
  const char* smode_label[4] = { "OFF", "dBm", "S", "S-bar" };
  const char* cw_tone_label[4] = { "325", "700" };
  switch(id){
    // Visible parameters
    case VOLUME:  paramAction(action, volume, F("1.1"), F("Volume"), NULL, -1, 16, false); break;
    case MODE:    paramAction(action, mode, F("1.2"), F("Mode"), mode_label, 0, _N(mode_label) - 1, true); break;
    case FILTER:  paramAction(action, filt, F("1.3"), F("Filter BW"), filt_label, 0, _N(filt_label) - 1, false); break;
    case BAND:    paramAction(action, bandval, F("1.4"), F("Band"), band_label, 0, _N(band_label) - 1, false); break;
    case STEP:    paramAction(action, stepsize, F("1.5"), F("Tune Rate"), stepsize_label, 0, _N(stepsize_label) - 1, false); break;
    case AGC:     paramAction(action, agc, F("1.6"), F("AGC"), offon_label, 0, 1, false); break;
    case NR:      paramAction(action, nr, F("1.7"), F("NR"), NULL, 0, 8, false); break;
    case ATT:     paramAction(action, att, F("1.8"), F("ATT"), att_label, 0, 7, false); break;
    case ATT2:    paramAction(action, att2, F("1.9"), F("ATT2"), NULL, 0, 16, false); break;
    case SMETER:  paramAction(action, smode, F("1.10"), F("S-meter"), smode_label, 0, _N(smode_label) - 1, false); break;
    case CWDEC:   paramAction(action, cwdec, F("2.1"), F("CW Decoder"), offon_label, 0, 1, false); break;
    case CWTONE:  paramAction(action, cw_tone, F("2.2"), F("CW Tone"), cw_tone_label, 0, 1, false); break;
    case CWOFF:   paramAction(action, cw_offset, F("2.3"), F("CW Offset"), NULL, 300, 2000, false); break;
    case VOX:     paramAction(action, vox, F("3.1"), F("VOX"), offon_label, 0, 1, false); break;
    case VOXGAIN: paramAction(action, vox_thresh, F("3.2"), F("VOX Level"), NULL, 0, 255, false); break;
    case MOX:     paramAction(action, mox, F("3.3"), F("MOX"), NULL, 0, 4, false); break;
    case DRIVE:   paramAction(action, drive, F("3.4"), F("TX Drive"), NULL, 0, 8, false); break;
    case SIFXTAL: paramAction(action, si5351.fxtal, F("8.1"), F("Ref freq"), NULL, 14000000, 28000000, false); break;
    case PWM_MIN: paramAction(action, pwm_min, F("8.2"), F("PA Bias min"), NULL, 0, 255, false); break;
    case PWM_MAX: paramAction(action, pwm_max, F("8.3"), F("PA Bias max"), NULL, 0, 255, false); break;
#ifdef CAL_IQ
    case CALIB:   if(dsp_cap != SDR) paramAction(action, cal_iq_dummy, F("8.4"), F("IQ Test/Cal."), NULL, 0, 0, false); break;
#endif
#ifdef DEBUG
    case SR:      paramAction(action, sr, F("9.1"), F("Sample rate"), NULL, -2147483648, 2147483647, false); break;
    case CPULOAD: paramAction(action, cpu_load, F("9.2"), F("CPU load %"), NULL, -2147483648, 2147483647, false); break;
    case PARAM_A: paramAction(action, param_a, F("9.3"), F("Param A"), NULL, 0, 65535, false); break;
    case PARAM_B: paramAction(action, param_b, F("9.4"), F("Param B"), NULL, -32768, 32767, false); break;
    case PARAM_C: paramAction(action, param_c, F("9.5"), F("Param C"), NULL, -32768, 32767, false); break;
#endif
    // Invisible parameters
    case FREQ:    paramAction(action, freq, NULL, NULL, NULL, 0, 0, false); break;
    case VERS:    paramAction(action, eeprom_version, NULL, NULL, NULL, 0, 0, false); break;
  }
}
