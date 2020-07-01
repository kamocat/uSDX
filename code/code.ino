//  QCX-SSB.ino - https://github.com/threeme3/QCX-SSB
//
//  Copyright 2019, 2020   Guido PE1NNZ <pe1nnz@amsat.org>
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#define VERSION   "1.02g"

#define QCX     1         // If you DO NOT have a QCX then comment-out (add two-slashes // in the beginning of this line)
//#define F_SAMP_TX 4402
#define F_SAMP_TX 4810        //4810 // ADC sample-rate; is best a multiple of _UA and fits exactly in OCR0A = ((F_CPU / 64) / F_SAMP_TX) - 1 , should not exceed CPU utilization
#define _UA  (F_SAMP_TX)      //360  // unit angle; integer representation of one full circle turn or 2pi radials or 360 degrees, should be a integer divider of F_SAMP_TX and maximized to have higest precision
//#define MAX_DP  (_UA/1)  //(_UA/2) // the occupied SSB bandwidth can be further reduced by restricting the maximum phase change (set MAX_DP to _UA/2).
//#define CONSTANT_AMP  1 // enable this in case there is no circuitry for controlling envelope (key shaping circuit)
//#define CARRIER_COMPLETELY_OFF_ON_LOW  1    // disable oscillator on no-envelope transitions, to prevent potential unwanted biasing/leakage through PA circuit
#define MULTI_ADC  1  // multiple ADC conversions for more sensitive (+12dB) microphone input
#define DEBUG  1   // enable testing and diagnostics features
#ifdef DEBUG
  static uint32_t sr = 0;
  static uint32_t cpu_load = 0;
  volatile uint16_t param_a = 0;  // registers for debugging, testing and experimental purposes
  volatile int16_t param_b = 0;
  volatile int16_t param_c = 0;
#endif

#undef F_CPU
#define F_CPU 20007000   // myqcx1:20008440, myqcx2:20006000   // Actual crystal frequency of 20MHz XTAL1, note that this declaration is just informative and does not correct the timing in Arduino functions like delay(); hence a 1.25 factor needs to be added for correction.
//#define F_CPU F_XTAL   // in case ATMEGA328P clock is the same as SI5351 clock (ATMEGA clock tapped from SI crystal)



#include <avr/sleep.h>
#include <avr/wdt.h>
#include "math.h"
  
// Hardware Details
#include "hal/pinout.h"
#include "hal/lcd.h"
#include "hal/i2c.h"
#include "hal/oled.h"
#include "hal/si5351.h"
#include "hal/encoder.h"
#include "hal/adc.h"
#include "hal/timer.h"

// Radio Protocols
#include "radio/common.h"
#include "filter.h"
#include "radio/ssb.h"
#include "radio/tx.h"
#include "radio/cw.h"
#include "radio/am.h"
#include "radio/dsb.h"
#include "radio/fm.h"
#include "radio/cat.h"
#include "radio/rx.h"

//FUSES = { .low = 0xFF, .high = 0xD6, .extended = 0xFD };   // Fuse settings should be these at programming.

ISR(TIMER2_COMPA_vect)  // Timer2 COMPA interrupt
{
  func_ptr();
#ifdef DEBUG
  numSamples++;
#endif
}

void dummy()
{
}

enum dsp_cap_t { ANALOG, DSP, SDR };
uint8_t dsp_cap = 0;
uint8_t ssb_cap = 0;

uint16_t analogSampleMic()
{
  uint16_t adc;
  noInterrupts();
  if(dsp_cap == SDR) digitalWrite(RX, LOW);  // disable RF input, only for SDR mod
  //si5351.SendRegister(SI_CLK_OE, 0b11111111); // CLK2_EN=0, CLK1_EN,CLK0_EN=0
  ADMUX = admux[2];  // set MUX for next conversion
  ADCSRA |= (1 << ADSC);    // start next ADC conversion
  for(;!(ADCSRA & (1 << ADIF)););  // wait until ADC conversion is completed
  if(dsp_cap == SDR) digitalWrite(RX, HIGH);  // enable RF input, only for SDR mod
  //si5351.SendRegister(SI_CLK_OE, 0b11111100); // CLK2_EN=0, CLK1_EN,CLK0_EN=1
  adc = ADC;
  interrupts();
  return adc;
}

volatile bool change = true;
volatile int32_t freq = 7074000;

int8_t smode = 1;

float dbm_max;
float smeter(float ref = 5)  //= 10*log(8000/2400)=5  ref to 2.4kHz BW.  plus some other calibration factor
{
  if(smode == 0){ // none, no s-meter
    return 0;
  }
  float rms = _absavg256 / 256.0; //sqrt(256.0);
  //if(dsp_cap == SDR) rms = (float)rms * 1.1 * (float)(1 << att2) / (1024.0 * (float)R * 4.0 * 100.0 * 40.0);   // 2 rx gain stages: rmsV = ADC value * AREF / [ADC DR * processing gain * receiver gain * audio gain]
  if(dsp_cap == SDR) rms = (float)rms * 1.1 * (float)(1 << att2) / (1024.0 * (float)R * 4.0 * 820.0 * 3.0/*??*/);   // 1 rx gain stage: rmsV = ADC value * AREF / [ADC DR * processing gain * receiver gain * audio gain]
  else               rms = (float)rms * 5.0 * (float)(1 << att2) / (1024.0 * (float)R * 2.0 * 100.0 * 120.0 / 1.750);
  float dbm = (10.0 * log10((rms * rms) / 50.0) + 30.0) - ref; //from rmsV to dBm at 50R
  dbm_max = max(dbm_max, dbm);
  static uint8_t cnt;
  cnt++;
  if((cnt % 8) == 0){
    if(smode == 1){ // dBm meter
      lcd.noCursor(); lcd.setCursor(9, 0); lcd.print((int16_t)dbm_max); lcd.print(F("dBm   "));
    }
    if(smode == 2){ // S-meter
      uint8_t s = (dbm_max < -63) ? ((dbm_max - -127) / 6) : (uint8_t)(dbm_max - -63 + 10) % 10;  // dBm to S
      lcd.noCursor(); lcd.setCursor(14, 0); if(s < 10){ lcd.print('S'); } lcd.print(s); 
    }
    dbm_max = -174.0 + 34.0;
  }
  if(smode == 3){ // S-bar
    int8_t s = (dbm < -63) ? ((dbm - -127) / 6) : (uint8_t)(dbm - -63 + 10) % 10;  // dBm to S
    lcd.noCursor(); lcd.setCursor(12, 0); 
    char tmp[5];
    for(uint8_t i = 0; i != 4; i++){ tmp[i] = max(2, min(5, s + 1)); s = s - 3; } tmp[4] = 0;
    lcd.print(tmp);
  }
  return dbm;
}

void start_rx()
{
  _init = 1;
  rx_state = 0;
  func_ptr = sdr_rx;  //enable RX DSP/SDR
  adc_start(2, true, F_ADC_CONV); admux[2] = ADMUX;
  if(dsp_cap == SDR){
    adc_start(0, !(att == 1)/*true*/, F_ADC_CONV); admux[0] = ADMUX;
    adc_start(1, !(att == 1)/*true*/, F_ADC_CONV); admux[1] = ADMUX;
  } else { // ANALOG, DSP
    adc_start(0, false, F_ADC_CONV); admux[0] = ADMUX; admux[1] = ADMUX;
  }
  timer1_start(F_SAMP_PWM);
  timer2_start(F_SAMP_RX);  
  TCCR1A &= ~(1 << COM1B1); digitalWrite(KEY_OUT, LOW); // disable KEY_OUT PWM
}

void switch_rxtx(uint8_t tx_enable){
  tx = tx_enable;
  TIMSK2 &= ~(1 << OCIE2A);  // disable timer compare interrupt
  //delay(1);
  noInterrupts();
  if(tx_enable){
    switch(mode){
      case USB:
      case LSB: func_ptr = dsp_tx; break;
      case CW:  func_ptr = dsp_tx_cw; break;
      case AM:  func_ptr = dsp_tx_am; break;
      case FM:  func_ptr = dsp_tx_fm; break;
    }
  } else func_ptr = sdr_rx;
  if((!dsp_cap) && (!tx_enable) && vox)  func_ptr = dummy; //hack: for SSB mode, disable dsp_rx during vox mode enabled as it slows down the vox loop too much!
  interrupts();
  if(tx_enable) ADMUX = admux[2];
  else _init = 1;
  rx_state = 0;
  if(tx_enable){
      digitalWrite(RX, LOW);  // TX (disable RX)
#ifdef NTX
      digitalWrite(NTX, LOW);  // TX (enable TX)
#endif
      lcd.setCursor(15, 1); lcd.print("T");
      si5351.SendRegister(SI_CLK_OE, 0b11111011); // CLK2_EN=1, CLK1_EN,CLK0_EN=0
      //if(!mox) TCCR1A &= ~(1 << COM1A1); // disable SIDETONE, prevent interference during TX
      OCR1AL = 0; // make sure SIDETONE is set to 0%
      TCCR1A |= (1 << COM1B1);  // enable KEY_OUT PWM
  } else {
      //TCCR1A |= (1 << COM1A1);  // enable SIDETONE
      TCCR1A &= ~(1 << COM1B1); digitalWrite(KEY_OUT, LOW); // disable KEY_OUT PWM, prevents interference during RX
      OCR1BL = 0; // make sure PWM (KEY_OUT) is set to 0%
      digitalWrite(RX, !(att == 2)); // RX (enable RX when attenuator not on)
#ifdef NTX
      digitalWrite(NTX, HIGH);  // RX (disable TX)
#endif
      si5351.SendRegister(SI_CLK_OE, 0b11111100); // CLK2_EN=0, CLK1_EN,CLK0_EN=1
      lcd.setCursor(15, 1); lcd.print((vox) ? "V" : "R");
  }
  OCR2A = (((float)F_CPU / (float)64) / (float)((tx_enable) ? F_SAMP_TX : F_SAMP_RX) + 0.5) - 1;
  TIMSK2 |= (1 << OCIE2A);  // enable timer compare interrupt TIMER2_COMPA_vect
}

#ifdef QCX
#define CAL_IQ 1
#ifdef CAL_IQ
int16_t cal_iq_dummy = 0;
// RX I/Q calibration procedure: terminate with 50 ohm, enable CW filter, adjust R27, R24, R17 subsequently to its minimum side-band rejection value in dB
void calibrate_iq()
{
  smode = 1;
  lcd.setCursor(0, 0); lcd.print(blanks); lcd.print(blanks);
  digitalWrite(SIG_OUT, true); // loopback on
  si5351.freq(freq, 0, 90);  // RX in USB
  si5351.SendRegister(SI_CLK_OE, 0b11111000); // CLK2_EN=0, CLK1_EN,CLK0_EN=1
  float dbc;
  si5351.freq_calc_fast(+700); si5351.SendPLLBRegisterBulk(); delay(100);
  dbc = smeter();
  si5351.freq_calc_fast(-700); si5351.SendPLLBRegisterBulk(); delay(100);
  lcd.setCursor(0, 1); lcd.print("I-Q bal. 700Hz"); lcd.print(blanks);
  for(; !digitalRead(BUTTONS);){ wdt_reset(); smeter(dbc); } for(; digitalRead(BUTTONS);) wdt_reset();
  si5351.freq_calc_fast(+600); si5351.SendPLLBRegisterBulk(); delay(100);
  dbc = smeter();
  si5351.freq_calc_fast(-600); si5351.SendPLLBRegisterBulk(); delay(100);
  lcd.setCursor(0, 1); lcd.print("Phase Lo 600Hz"); lcd.print(blanks);
  for(; !digitalRead(BUTTONS);){ wdt_reset(); smeter(dbc); } for(; digitalRead(BUTTONS);) wdt_reset();
  si5351.freq_calc_fast(+800); si5351.SendPLLBRegisterBulk(); delay(100);
  dbc = smeter();
  si5351.freq_calc_fast(-800); si5351.SendPLLBRegisterBulk(); delay(100);
  lcd.setCursor(0, 1); lcd.print("Phase Hi 800Hz"); lcd.print(blanks);
  for(; !digitalRead(BUTTONS);){ wdt_reset(); smeter(dbc); } for(; digitalRead(BUTTONS);) wdt_reset();

  lcd.setCursor(9, 0); lcd.print(blanks);  // cleanup dbmeter
  digitalWrite(SIG_OUT, false); // loopback off
  si5351.SendRegister(SI_CLK_OE, 0b11111100); // CLK2_EN=0, CLK1_EN,CLK0_EN=1
  change = true;  //restore original frequency setting
}
#endif
#endif //QCX


#include "ui.h" // Need to seperate this out

void setup()
{
  digitalWrite(KEY_OUT, LOW);  // for safety: to prevent exploding PA MOSFETs, in case there was something still biasing them.

  uint8_t mcusr = MCUSR;
  MCUSR = 0;
  //wdt_disable();
  wdt_enable(WDTO_4S);  // Enable watchdog
#ifdef DEBUG
  // Benchmark dsp_tx() ISR (this needs to be done in beginning of setup() otherwise when VERSION containts 5 chars, mis-alignment impact performance by a few percent)
  rx_state = 0;
  uint32_t t0, t1;
  func_ptr = dsp_tx;
  t0 = micros();
  TIMER2_COMPA_vect();
  //func_ptr();
  t1 = micros();
  float load_tx = (t1 - t0) * F_SAMP_TX * 100.0 / 1000000.0;
  // benchmark sdr_rx() ISR
  func_ptr = sdr_rx;
  rx_state = 8;
  float load_rx[8];
  float load_rx_avg = 0;
  uint16_t i;
  for(i = 0; i != 8; i++){
    t0 = micros();
    TIMER2_COMPA_vect();
    //func_ptr();
    t1 = micros();
    load_rx[i] = (t1 - t0) * F_SAMP_RX * 100.0 / 1000000.0;
    load_rx_avg += load_rx[i];
  }
  load_rx_avg /= 8;

  //adc_stop();  // recover general ADC settings so that analogRead is working again
#endif
  ADMUX = (1 << REFS0);  // restore reference voltage AREF (5V)

  // disable external interrupts
  PCICR = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;

  encoder_setup();

  initPins();

  delay(100);           // at least 40ms after power rises above 2.7V before sending commands
  lcd.begin(16, 2);  // Init LCD
  for(i = 0; i != N_FONTS; i++){  // Init fonts
    pgm_cache_item(fonts[i], 8);
    lcd.createChar(0x01 + i, /*fonts[i]*/_item);
  }

  //Init si5351
  si5351.powerDown();  // Disable all (used) outputs

#ifdef QCX
  // Test if QCX has DSP/SDR capability: SIDETONE output disconnected from AUDIO2
  ssb_cap = 0; dsp_cap = 0;
  si5351.SendRegister(SI_CLK_OE, 0b11111111); // Mute QSD: CLK2_EN=CLK1_EN,CLK0_EN=0  
  digitalWrite(RX, HIGH);  // generate pulse on SIDETONE and test if it can be seen on AUDIO2
  delay(1); // settle
  digitalWrite(SIDETONE, LOW);
  int16_t v1 = analogRead(AUDIO2);
  digitalWrite(SIDETONE, HIGH);
  int16_t v2 = analogRead(AUDIO2);
  digitalWrite(SIDETONE, LOW);
  dsp_cap = !(abs(v2 - v1) > (0.05 * 1024.0 / 5.0));  // DSP capability?
  if(dsp_cap){  // Test if QCX has SDR capability: AUDIO2 is disconnected from AUDIO1  (only in case of DSP capability)
    delay(400); wdt_reset(); // settle:  the following test only works well 400ms after startup
    v1 = analogRead(AUDIO1);
    digitalWrite(AUDIO2, HIGH);   // generate pulse on AUDIO2 and test if it can be seen on AUDIO1
    pinMode(AUDIO2, OUTPUT);
    delay(1);
    digitalWrite(AUDIO2, LOW); 
    delay(1);
    digitalWrite(AUDIO2, HIGH);
    v2 = analogRead(AUDIO1);
    pinMode(AUDIO2, INPUT);
    if(!(abs(v2 - v1) > (0.125 * 1024.0 / 5.0))) dsp_cap = SDR;  // SDR capacility?
  }
  // Test if QCX has SSB capability: DAH is connected to DVM
  delay(1); // settle
  pinMode(DAH, OUTPUT);
  digitalWrite(DAH, LOW);
  v1 = analogRead(DVM);
  digitalWrite(DAH, HIGH);
  v2 = analogRead(DVM);
  digitalWrite(DAH, LOW);
  pinMode(DAH, INPUT_PULLUP);
  ssb_cap = (abs(v2 - v1) > (0.05 * 1024.0 / 5.0));  // SSB capability?

  //ssb_cap = 0; dsp_cap = 0;  // force standard QCX capability
  //ssb_cap = 1; dsp_cap = 0;  // force SSB and standard QCX-RX capability
  //ssb_cap = 1; dsp_cap = 1;  // force SSB and DSP capability
  //ssb_cap = 1; dsp_cap = 2;  // force SSB and SDR capability
#else
  ssb_cap = 1; dsp_cap = 2;  // force SSB and SDR capability
#endif

  show_banner();
  lcd.setCursor(7, 0); lcd.print(F(" R")); lcd.print(F(VERSION)); lcd_blanks();

#ifdef DEBUG
  /*if((mcusr & WDRF) && (!(mcusr & EXTRF)) && (!(mcusr & BORF))){
    lcd.setCursor(0, 1); lcd.print(F("!!Watchdog RESET")); lcd_blanks();
    delay(1500); wdt_reset();
  }
  if((mcusr & BORF) && (!(mcusr & WDRF))){
    lcd.setCursor(0, 1); lcd.print(F("!!Brownout RESET")); lcd_blanks();  // Brow-out reset happened, CPU voltage not stable or make sure Brown-Out threshold is set OK (make sure E fuse is set to FD)
    delay(1500); wdt_reset();
  }
  if(mcusr & PORF){
    lcd.setCursor(0, 1); lcd.print(F("!!Power-On RESET")); lcd_blanks();
    delay(1500); wdt_reset();
  }*/
  /*if(mcusr & EXTRF){
  lcd.setCursor(0, 1); lcd.print(F("Power-On")); lcd_blanks();
    delay(1); wdt_reset();
  }*/
  
  // Measure CPU loads
  if(!(load_tx <= 100.0)){
    lcd.setCursor(0, 1); lcd.print(F("!!CPU_tx=")); lcd.print(load_tx); lcd.print(F("%")); lcd_blanks();
    delay(1500); wdt_reset();
  }

  if(!(load_rx_avg <= 100.0)){
    lcd.setCursor(0, 1); lcd.print(F("!!CPU_rx")); lcd.print(F("=")); lcd.print(load_rx_avg); lcd.print(F("%")); lcd_blanks();
    delay(1500); wdt_reset();
    // and specify individual timings for each of the eight alternating processing functions:
    for(i = 1; i != 8; i++){
      if(!(load_rx[i] <= 100.0))
      {
        lcd.setCursor(0, 1); lcd.print(F("!!CPU_rx")); lcd.print(i); lcd.print(F("=")); lcd.print(load_rx[i]); lcd.print(F("%")); lcd_blanks();
        delay(1500); wdt_reset();
      }
    }
  }

  // Measure VDD (+5V); should be ~5V
  si5351.SendRegister(SI_CLK_OE, 0b11111111); // Mute QSD: CLK2_EN=CLK1_EN,CLK0_EN=0
  digitalWrite(KEY_OUT, LOW);
  digitalWrite(RX, LOW);  // mute RX
  delay(100); // settle
  float vdd = 2.0 * (float)analogRead(AUDIO2) * 5.0 / 1024.0;
  digitalWrite(RX, HIGH);
  if(!(vdd > 4.8 && vdd < 5.2)){
    lcd.setCursor(0, 1); lcd.print(F("!!V5.0=")); lcd.print(vdd); lcd.print(F("V")); lcd_blanks();
    delay(1500); wdt_reset();
  }

  // Measure VEE (+3.3V); should be ~3.3V
  float vee = (float)analogRead(SCL) * 5.0 / 1024.0;
  if(!(vee > 3.2 && vee < 3.8)){
    lcd.setCursor(0, 1); lcd.print(F("!!V3.3=")); lcd.print(vee); lcd.print(F("V")); lcd_blanks();
    delay(1500); wdt_reset();
  }

  // Measure AVCC via AREF and using internal 1.1V reference fed to ADC; should be ~5V
  analogRead(6); // setup almost proper ADC readout
  bitSet(ADMUX, 3); // Switch to channel 14 (Vbg=1.1V)
  delay(1); // delay improves accuracy
  bitSet(ADCSRA, ADSC);
  for(; bit_is_set(ADCSRA, ADSC););
  float avcc = 1.1 * 1023.0 / ADC;
  if(!(avcc > 4.6 && avcc < 5.2)){
    lcd.setCursor(0, 1); lcd.print(F("!!Vavcc=")); lcd.print(avcc); lcd.print(F("V")); lcd_blanks();
    delay(1500); wdt_reset();
  }

  // Report no SSB capability
  if(!ssb_cap){
    lcd.setCursor(0, 1); lcd.print(F("No MIC input...")); lcd_blanks();
    delay(300); wdt_reset();
  }

  // Test microphone polarity
  /*if((ssb_cap) && (!digitalRead(DAH))){
    lcd.setCursor(0, 1); lcd.print(F("!!MIC in rev.pol")); lcd_blanks();
    delay(300); wdt_reset();
  }*/

  // Measure DVM bias; should be ~VAREF/2
  float dvm = (float)analogRead(DVM) * 5.0 / 1024.0;
  if((ssb_cap) && !(dvm > 1.8 && dvm < 3.2)){
    lcd.setCursor(0, 1); lcd.print(F("!!Vadc2=")); lcd.print(dvm); lcd.print(F("V")); lcd_blanks();
    delay(1500); wdt_reset();
  }

  // Measure AUDIO1, AUDIO2 bias; should be ~VAREF/2
  if(dsp_cap == SDR){
    float audio1 = (float)analogRead(AUDIO1) * 5.0 / 1024.0;
    if(!(audio1 > 1.8 && audio1 < 3.2)){
      lcd.setCursor(0, 1); lcd.print(F("!!Vadc0=")); lcd.print(dvm); lcd.print(F("V")); lcd_blanks();
      delay(1500); wdt_reset();
    }
    float audio2 = (float)analogRead(AUDIO2) * 5.0 / 1024.0;
    if(!(audio2 > 1.8 && audio2 < 3.2)){
      lcd.setCursor(0, 1); lcd.print(F("!!Vadc1=")); lcd.print(dvm); lcd.print(F("V")); lcd_blanks();
      delay(1500); wdt_reset();
    }
  }
  
  // Measure I2C Bus speed for Bulk Transfers
  si5351.freq(freq, 0, 90);
  wdt_reset();
  t0 = micros();
  for(i = 0; i != 1000; i++) si5351.SendPLLBRegisterBulk();
  t1 = micros();
  uint32_t speed = (1000000 * 8 * 7) / (t1 - t0); // speed in kbit/s
  if(false)
  {
    lcd.setCursor(0, 1); lcd.print(F("i2cspeed=")); lcd.print(speed); lcd.print(F("kbps")); lcd_blanks();
    delay(1500); wdt_reset();
  }

  // Measure I2C Bit-Error Rate (BER); should be error free for a thousand random bulk PLLB writes
  si5351.freq(freq, 0, 90);
  wdt_reset();
  uint16_t i2c_error = 0;  // number of I2C byte transfer errors
  for(i = 0; i != 1000; i++){
    si5351.freq_calc_fast(i);
    //for(int j = 3; j != 8; j++) si5351.pll_regs[j] = rand();
    si5351.SendPLLBRegisterBulk();
    #define SI_SYNTH_PLL_B 34
    for(int j = 3; j != 8; j++) if(si5351.RecvRegister(SI_SYNTH_PLL_B + j) != si5351.pll_regs[j]) i2c_error++;
  }
  if(i2c_error){
    lcd.setCursor(0, 1); lcd.print(F("!!BER_i2c=")); lcd.print(i2c_error); lcd_blanks();
    delay(1500); wdt_reset();
  }
#endif

  drive = 4;  // Init settings
  if(!ssb_cap){ mode = CW; filt = 4; stepsize = STEP_500; }
  if(dsp_cap != SDR) pwm_max = 255; // implies that key-shaping circuit is probably present, so use full-scale
  cw_offset = tones[1];
  if(dsp_cap == DSP) volume = 10;

  // Load parameters from EEPROM, reset to factory defaults when stored values are from a different version
  paramAction(LOAD, VERS);
  if((eeprom_version != get_version_id()) || !digitalRead(DIT) ){  // EEPROM clean: if PTT/onboard-key pressed or version signature in EEPROM does NOT corresponds with this firmware
    eeprom_version = get_version_id();
    //for(int n = 0; n != 1024; n++){ eeprom_write_byte((uint8_t *) n, 0); wdt_reset(); } //clean EEPROM
    //eeprom_write_dword((uint32_t *)EEPROM_OFFSET/3, 0x000000);
    paramAction(SAVE);  // save default parfameter values
    lcd.setCursor(0, 1); lcd.print(F("Reset settings.."));
    delay(500); wdt_reset();
  } else {
    paramAction(LOAD);  // load all parameters
  }
  si5351.iqmsa = 0;  // enforce PLL reset
  change = true;
  prev_bandval = bandval;

  if(!dsp_cap) volume = 0;  // mute volume for unmodified QCX receiver

  for(uint16_t i = 0; i != 256; i++)    // refresh LUT based on pwm_min, pwm_max
    lut[i] = (float)i / ((float)255 / ((float)pwm_max - (float)pwm_min)) + pwm_min;

  show_banner();  // remove release number

  start_rx();

// #define CAT 1
#ifdef CAT
  Serial.begin(7680); // 9600 baud corrected for F_CPU=20M
#endif
}

void print_char(uint8_t in){  // Print char in second line of display and scroll right.
  for(int i = 0; i!= 15; i++) out[i] = out[i+1];
  out[15] = in;
  out[16] = '\0';
  cw_event = true;
}



void loop()
{
#ifdef CAT
  if(Serial.available() > 0){
    uint8_t in = Serial.read();
    print_char(in);
    parse_cat(in);
  }
#endif

#ifndef SIMPLE_RX
  delay(1);
#endif

  if(millis() > sec_event_time){
    sec_event_time = millis() + 1000;  // schedule time next second

//#define LCD_REINIT
#ifdef LCD_REINIT
#ifndef OLED
    lcd.begin();  // fast LCD re-init (in case LCD has been taken out and placed back when power-on)
#endif
#endif
  }

  if(menumode == 0){
    smeter();
    if(!((mode == CW) && cw_event) && (smode)) stepsize_showcursor();  // restore cursor (when there is no CW text and smeter is enabled)
  }

  if(cw_event){
    cw_event = false;
    lcd.setCursor(0, 1); lcd.print(out);
  }

#ifdef ONEBUTTON
  uint8_t inv = 1;
#else
  uint8_t inv = 0;
#endif

#define DAH_AS_KEY  1
#ifdef DAH_AS_KEY
  if(!digitalRead(DIT)  || ((mode == CW) && (!digitalRead(DAH))) ){  // PTT/DIT keys transmitter,  for CW also DAH
#else
  if(!digitalRead(DIT) ){  // PTT/DIT keys transmitter
#endif
    switch_rxtx(1);
#ifdef DAH_AS_KEY
    for(; !digitalRead(DIT)  || ((mode == CW) && (!digitalRead(DAH)));){ // until released
#else
    for(; !digitalRead(DIT) ;){ // until released
#endif
      wdt_reset();
      if(inv ^ digitalRead(BUTTONS)) break;  // break if button is pressed (to prevent potential lock-up)
    }
    switch_rxtx(0);
  }

  enum event_t { BL=0x10, BR=0x20, BE=0x30, SC=0x01, DC=0x02, PL=0x04, PT=0x0C }; // button-left, button-right and button-encoder; single-click, double-click, push-long, push-and-turn
  if(inv ^ digitalRead(BUTTONS)){   // Left-/Right-/Rotary-button (while not already pressed)
    if(!(event & PL)){  // hack: if there was long-push before, then fast forward
      uint16_t v = analogSafeRead(BUTTONS);
      event = SC;
      int32_t t0 = millis();
      for(; inv ^ digitalRead(BUTTONS);){ // until released or long-press
        if((millis() - t0) > 300){ event = PL; break; }
        wdt_reset();
      }
      delay(10); //debounce
      for(; (event != PL) && ((millis() - t0) < 500);){ // until 2nd press or timeout
        if(inv ^ digitalRead(BUTTONS)){ event = DC; break; }
        wdt_reset();
      }
      for(; inv ^ digitalRead(BUTTONS);){ // until released, or encoder is turned while longpress
        if(encoder_val && event == PL){ event = PT; break; }
        wdt_reset();
      }  // Max. voltages at ADC3 for buttons L,R,E: 3.76V;4.55V;5V, thresholds are in center
      event |= (v < (4.2 * 1024.0 / 5.0)) ? BL : (v < (4.8 * 1024.0 / 5.0)) ? BR : BE; // determine which button pressed based on threshold levels
    } else {  // hack: fast forward handling
      event = (event&0xf0) | ((encoder_val) ? PT : PL);  // only alternate bewteen push-long/turn when applicable
    }
    switch(event){
      case BL|PL:  // Called when menu button released
        menumode = 2;
        //calibrate_predistortion();
        //powermeter();
        //test_tx_amp();
        break;
      case BL|PT:
        menumode = 1;
        //if(menu == 0) menu = 1;
        break;
      case BL|SC:
        //calibrate_iq();
        int8_t _menumode;
        if(menumode == 0){ _menumode = 1; if(menu == 0) menu = 1; }  // short left-click while in default screen: enter menu mode
        if(menumode == 1){ _menumode = 2; }                          // short left-click while in menu: enter value selection screen
        if(menumode == 2){ _menumode = 0; show_banner(); change = true; paramAction(SAVE, menu); } // short left-click while in value selection screen: save, and return to default screen
        menumode = _menumode;
        break;
      case BL|DC:
        //powerDown();
        /*lcd.setCursor(0, 1); lcd.print(F("Pause")); lcd_blanks();
        for(; !digitalRead(BUTTONS);){ // while in VOX mode
          wdt_reset();  // until 2nd press
          delay(300);
        }*/
        break;
      case BR|SC:
        if(!menumode){
          int8_t prev_mode = mode;
          encoder_val = 1;
          paramAction(UPDATE, MODE); // Mode param //paramAction(UPDATE, mode, NULL, F("Mode"), mode_label, 0, _N(mode_label), true);
          #define MODE_CHANGE_RESETS  1
          #ifdef MODE_CHANGE_RESETS
          if(mode != CW) stepsize = STEP_1k; else stepsize = STEP_500; // sets suitable stepsize
          #endif
          if(mode > CW) mode = LSB;  // skip all other modes (only LSB, USB, CW)
          #ifdef MODE_CHANGE_RESETS
          if(mode == CW) filt = 4; else filt = 0;  // resets filter (to most BW) on mode change
          #else
          prev_stepsize[prev_mode == CW] = stepsize; stepsize = prev_stepsize[mode == CW]; // backup stepsize setting for previous mode, restore previous stepsize setting for current selected mode; filter settings captured for either CQ or other modes.
          prev_filt[prev_mode == CW] = filt; filt = prev_filt[mode == CW];  // backup filter setting for previous mode, restore previous filter setting for current selected mode; filter settings captured for either CQ or other modes.
          #endif
          paramAction(SAVE, MODE); 
          paramAction(SAVE, FILTER);
          si5351.iqmsa = 0;  // enforce PLL reset
          change = true;
        } else {
          if(menumode == 1){ menumode = 0; show_banner(); change = true; }  // short right-click while in menu: enter value selection screen
          if(menumode == 2){ menumode = 1; change = true; paramAction(SAVE, menu); } // short right-click while in value selection screen: save, and return to menu screen
        }
        break;
      case BR|DC:
        //encoder_val = 1; paramAction(UPDATE, drive, NULL, F("Drive"), NULL, 0, 8, true);
        filt++;
        _init = true;
        if(mode == CW && filt > N_FILT) filt = 4;
        if(mode == CW && filt == 4) stepsize = STEP_500; // reset stepsize for 500Hz filter
        if(mode == CW && (filt == 5 || filt == 6) && stepsize < STEP_100) stepsize = STEP_100; // for CW BW 200, 100      -> step = 100 Hz
        if(mode == CW && filt == 7 && stepsize < STEP_10) stepsize = STEP_10;                  // for CW BW 50 -> step = 10 Hz
        if(mode != CW && filt > 3) filt = 0;
        encoder_val = 0; 
        paramAction(UPDATE, FILTER);
        paramAction(SAVE, FILTER);
        wdt_reset(); delay(1500); wdt_reset();
        change = true; // refresh display
        break;
      case BR|PL:
      {
        #ifdef SIMPLE_RX
        // Experiment: ISR-less sdr_rx():
        smode = 0;
        TIMSK2 &= ~(1 << OCIE2A);  // disable timer compare interrupt
        delay(100);
        lcd.setCursor(15, 1); lcd.print("X");
        static uint8_t x = 0;
        uint32_t next = 0;
        for(;;){
          func_ptr();
        #ifdef DEBUG
          numSamples++;
        #endif
          if(!rx_state){
            x++;
            if(x > 16){
              loop();
              //lcd.setCursor(9, 0); lcd.print((int16_t)100); lcd.print(F("dBm   "));  // delays are taking too long!
              x= 0;
            }
          }
          //for(;micros() < next;);  next = micros() + 16;   // sync every 1000000/62500=16ms (or later if missed)
        } //
        #endif //SIMPLE_RX
               
        //int16_t x = 0;
        lcd.setCursor(15, 1); lcd.print("V");
        for(; !digitalRead(BUTTONS);){ // while in VOX mode
          
          int16_t in = analogSampleMic() - 512;
          static int16_t dc;
          int16_t i, q;
          uint8_t j;
          static int16_t v[16];
          for(j = 0; j != 15; j++) v[j] = v[j + 1];
          dc += (in - dc) / 2;
          v[15] = in - dc;     // DC decoupling
          //dc = in;  // this is actually creating a low-pass filter
          i = v[7];
          q = ((v[0] - v[14]) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 + (v[6] - v[8]) * 15) / 128 + (v[6] - v[8]) / 2; // Hilbert transform, 40dB side-band rejection in 400..1900Hz (@4kSPS) when used in image-rejection scenario; (Hilbert transform require 5 additional bits)
        
          uint16_t _amp = magn(i, q);
          //x = max(x, abs(v[15]) );
          //lcd.setCursor(0, 1); lcd.print(x); lcd_blanks();
          //lcd.setCursor(0, 1); lcd.print(_amp); lcd_blanks();
          if(_amp > vox_thresh){            // workaround for RX noise leakage to AREF  
            for(j = 0; j != 16; j++) v[j] = 0;  // clean-up
            switch_rxtx(1);
            vox = 1; tx = 255; //kick
            delay(1);
            for(; tx && !digitalRead(BUTTONS); ) wdt_reset(); // while in tx triggered by vox
            switch_rxtx(0);
            delay(1);
            vox = 0;
            continue;  // skip the rest for the moment
          }    
          wdt_reset();
        }
      }
        lcd.setCursor(15, 1); lcd.print("R");
        break;
      case BR|PT: break;
      case BE|SC:
        if(!menumode)
          stepsize_change(+1);
        else {
          int8_t _menumode;
          if(menumode == 1){ _menumode = 2; }  // short encoder-click while in menu: enter value selection screen
          if(menumode == 2){ _menumode = 1; change = true; paramAction(SAVE, menu); } // short encoder-click while in value selection screen: save, and return to menu screen
          menumode = _menumode;
        }
        break;
      case BE|DC:
        delay(100);
        bandval++;
        if(bandval >= N_BANDS) bandval = 0;
        stepsize = STEP_1k;
        change = true;
        break;
      case BE|PL: stepsize_change(-1); break;
      case BE|PT:
          for(; digitalRead(BUTTONS);){ // process encoder changes until released
          wdt_reset();
          if(dsp_cap && encoder_val){
            paramAction(UPDATE, VOLUME);
            paramAction(SAVE, VOLUME);
            if(volume < 0) powerDown();  // powerDown when volume < 0
          }
        }
        change = true; // refresh display
        break;
    }
  } else event = 0;  // no button pressed: reset event

  if(menumode == 1){
    menu += encoder_val;   // Navigate through menu of parameters and values
    encoder_val = 0;
    menu = max(1 /* 0 */, min(menu, N_PARAMS));
  }

  bool param_change = (encoder_val != 0);
  if(menumode != 0){  // Show parameter and value
    if(menu != 0){
      paramAction(UPDATE_MENU, menu);  // update param with encoder change and display
    } else {
      menumode = 0; show_banner();  // while scrolling through menu: menu item 0 goes back to main console
      change = true; // refresh freq display (when menu = 0)
    }
    if(menumode == 2){
      if(param_change){
        lcd.setCursor(0, 1); lcd.cursor(); delay(10); // edits menu item value; make cursor visible
        if(menu == MODE){ // post-handling Mode parameter
          delay(100);
          change = true;
          si5351.iqmsa = 0;  // enforce PLL reset
          // make more generic: 
          if(mode != CW) stepsize = STEP_1k; else stepsize = STEP_500;
          if(mode == CW) filt = 4; else filt = 0;
        }
        if(menu == BAND){
          change = true;
        }
        if(menu == ATT){ // post-handling ATT parameter
          if(dsp_cap == SDR){
            adc_start(0, !(att & 0x01), F_ADC_CONV); admux[0] = ADMUX;  // att bit 0 ON: attenuate -13dB by changing ADC AREF (full-scale range) from 1V1 to 5V
            adc_start(1, !(att & 0x01), F_ADC_CONV); admux[1] = ADMUX;
          }
          digitalWrite(RX, !(att & 0x02)); // att bit 1 ON: attenuate -20dB by disabling RX line, switching Q5 (antenna input switch) into 100k resistence
          pinMode(AUDIO1, (att & 0x04) ? OUTPUT : INPUT); // att bit 2 ON: attenuate -40dB by terminating ADC inputs with 10R
          pinMode(AUDIO2, (att & 0x04) ? OUTPUT : INPUT);
        }
        if(menu == SIFXTAL){
          change = true;
        }
        if((menu == PWM_MIN) || (menu == PWM_MAX)){
          for(uint16_t i = 0; i != 256; i++)    // refresh LUT based on pwm_min, pwm_max
            lut[i] = (float)i / ((float)255 / ((float)pwm_max - (float)pwm_min)) + pwm_min;
            //lut[i] = min(pwm_max, (float)106*log(i) + pwm_min);  // compressed microphone output: drive=0, pwm_min=115, pwm_max=220
        }
        if(menu == CWTONE){
          if(dsp_cap){ cw_offset = (cw_tone == 0) ? tones[0] : tones[1]; paramAction(SAVE, CWOFF); }
        }
#ifdef CAL_IQ
        if(menu == CALIB){
          if(dsp_cap != SDR) calibrate_iq(); menu = 0;
        }
#endif
      }
#ifdef DEBUG
      if(menu == SR){          // measure sample-rate
        numSamples = 0;
        delay(500 * 1.25);     // delay 0.5s (in reality because F_CPU=20M instead of 16M, delay() is running 1.25x faster therefore we need to multiply wqith 1.25)
        sr = numSamples * 2;   // samples per second */
      }
      if(menu == CPULOAD){     // measure CPU-load
        uint32_t i = 0;
        uint32_t prev_time = millis();
        for(i = 0; i != 300000; i++) wdt_reset(); // fixed CPU-load 132052*1.25us delay under 0% load condition; is 132052*1.25 * 20M = 3301300 CPU cycles fixed load
        cpu_load = 100 - 132 * 100 / (millis() - prev_time);
      }
      if((menu == PARAM_A) || (menu == PARAM_B) || (menu == PARAM_C)){
        delay(300);
      }
#endif
    }
  }

  if(menumode == 0){
    if(encoder_val){  // process encoder tuning steps
      process_encoder_tuning_step(encoder_val);
      encoder_val = 0;
    }
  }

  if(change){
    change = false;
    if(prev_bandval != bandval){ freq = band[bandval]; prev_bandval = bandval; }
    save_event_time = millis() + 1000;  // schedule time to save freq (no save while tuning, hence no EEPROM wear out)
 
    if(menumode == 0){
      display_vfo(freq);
      stepsize_showcursor();
  
      // The following is a hack for SWR measurement:
      //si5351.alt_clk2(freq + 2400);
      //si5351.SendRegister(SI_CLK_OE, 0b11111000); // CLK2_EN=1, CLK1_EN,CLK0_EN=1
      //digitalWrite(SIG_OUT, HIGH);  // inject CLK2 on antenna input via 120K
    }
    
    noInterrupts();
    if(mode == CW){
      si5351.freq(freq + cw_offset, 90, 0);  // RX in CW-R (=LSB), correct for CW-tone offset
      si5351.freq_calc_fast(-cw_offset); si5351.SendPLLBRegisterBulk(); // TX at freq
    } else
    if(mode == LSB)
      si5351.freq(freq, 90, 0);  // RX in LSB
    else
      si5351.freq(freq, 0, 90);  // RX in USB, ...
    interrupts();
  }
  
  if((save_event_time) && (millis() > save_event_time)){  // save freq when time has reached schedule
    paramAction(SAVE, FREQ);  // save freq changes
    save_event_time = 0;
    //lcd.setCursor(15, 1); lcd.print("S"); delay(100); lcd.setCursor(15, 1); lcd.print("R");
  }
  
  wdt_reset();
}

/* BACKLOG:
code definitions and re-use for comb, integrator, dc decoupling, arctan
in func_ptr for different mode types
refactor main()
agc based on rms256, agc/smeter after filter
noisefree integrator (rx audio out) in lower range
raised cosine tx amp for cw, 4ms tau seems enough: http://fermi.la.asu.edu/w9cf/articles/click/index.html
auto paddle
cw tx message/cw encoder
32 bin fft
dynamic range cw
att extended agc
configurable F_CPU
CW-L mode
VFO-A/B+split+RIT
VOX integration in main loop
K2/TS480 CAT control
faster RX-TX switch to support CW
clock
qcx API demo code
scan
unwanted VOX feedback in DSP mode
move last bit of arrays into flash? https://www.microchip.com/webdoc/AVRLibcReferenceManual/FAQ_1faq_rom_array.html
remove floats
u-law in RX path?: http://dystopiancode.blogspot.com/2012/02/pcm-law-and-u-law-companding-algorithms.html
Arduino library?
1. RX bias offset correction by measurement avg, 2. charge decoupling cap. by resetting to 0V and setting 5V for a certain amount of (charge) time
AGC amplitude sense behind filter, controlling gain before filter
add 1K (500R?) to GND at TTL RF output to keep zero-level below BS170 threshold
additional PWM output for potential BOOST conversion
SWR measurement?
CW decoder amp thrshld restriction and noise reduction (use of certain pause amounts)
squelch gating

Analyse assembly:
/home/guido/Downloads/arduino-1.8.10/hardware/tools/avr/bin/avr-g++ -S -g -Os -w -std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics -Wno-error=narrowing -MMD -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10810 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR -I/home/guido/Downloads/arduino-1.8.10/hardware/arduino/avr/cores/arduino -I/home/guido/Downloads/arduino-1.8.10/hardware/arduino/avr/variants/standard /tmp/arduino_build_483134/sketch/QCX-SSB.ino.cpp -o /tmp/arduino_build_483134/sketch/QCX-SSB.ino.cpp.txt

Rewire/code I/Q clk pins so that a Div/1 and Div/2 scheme is used instead of 0 and 90 degrees phase shift
10,11,13,12   10,11,12,13  (pin)
Q- I+ Q+ I-   Q- I+ Q+ I-
90 deg.shift  div/2@S1(pin2)

50MHz LSB OK, USB NOK
s-meter offset vs DC bal.

AGC DR
auto ATT
class-E
PCB
RIT, VFO-B, undersampling, IF-offset
keyer dash-dot
tiny-click removal, DC offset correction

*/
