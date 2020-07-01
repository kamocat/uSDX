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
#include "hal/pinout.h"
#include "hal/lcd.h"
#include "hal/i2c.h"
#include "hal/oled.h"
#include "hal/si5351.h"
#include "hal/encoder.h"
#include "math.h"
#include "radio/common.h"
#include "radio/ssb.h"
#include "radio/tx.h"
//FUSES = { .low = 0xFF, .high = 0xD6, .extended = 0xFD };   // Fuse settings should be these at programming.





volatile uint16_t acc;
volatile uint32_t cw_offset;
volatile uint8_t cw_tone = 1;
const uint32_t tones[] = {325, 700};

volatile int16_t p_sin = 0;   // initialized with A*sin(0) = 0
volatile int16_t n_cos = 448/2; // initialized with A*cos(t) = A
inline void process_minsky() // Minsky circle sample [source: https://www.cl.cam.ac.uk/~am21/hakmemc.html, ITEM 149]: p_sin+=n_cos*2*PI*f/fs; n_cos-=p_sin*2*PI*f/fs;
{
  uint8_t alpha100 = tones[cw_tone]/*cw_offset*/ * 628 / F_SAMP_TX;  // alpha = f_tone * 6.28 / fs
  p_sin += alpha100 * n_cos / 100;
  n_cos -= alpha100 * p_sin / 100;
}

void dummy()
{
}

void dsp_tx_cw()
{ // jitter dependent things first
  OCR1BL = lut[255];
  
  process_minsky();
  OCR1AL = (p_sin >> (16 - volume)) + 128;
}

void dsp_tx_am()
{ // jitter dependent things first
  ADCSRA |= (1 << ADSC);    // start next ADC conversion (trigger ADC interrupt if ADIE flag is set)
  OCR1BL = amp;                        // submit amplitude to PWM register (actually this is done in advance (about 140us) of phase-change, so that phase-delays in key-shaping circuit filter can settle)
  int16_t adc = ADC - 512; // current ADC sample 10-bits analog input, NOTE: first ADCL, then ADCH
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive-4);
  //static int16_t dc;
  //dc += (in - dc) / 2;
  //in = in - dc;     // DC decoupling
  #define AM_BASE 32
  in=max(0, min(255, (in + AM_BASE)));
  amp=in;// lut[in];
}

uint8_t reg;
void dsp_tx_dsb()
{ // jitter dependent things first
  ADCSRA |= (1 << ADSC);    // start next ADC conversion (trigger ADC interrupt if ADIE flag is set)
  OCR1BL = amp;                        // submit amplitude to PWM register (actually this is done in advance (about 140us) of phase-change, so that phase-delays in key-shaping circuit filter can settle)
  si5351.SendRegister(16+2, reg);             // CLK2 polarity depending on amplitude
  int16_t adc = ADC - 512; // current ADC sample 10-bits analog input, NOTE: first ADCL, then ADCH
  int16_t in = (adc >> MIC_ATTEN);
  in = in << drive;
  reg = (in < 0) ? 0x2C|3|0x10 : 0x2C|3;  //0x2C=PLLB local msynth 3=8mA 0x10=invert
  in=min(255, abs(in));
  amp=in;// lut[in];
}

void dsp_tx_fm()
{ // jitter dependent things first
  ADCSRA |= (1 << ADSC);    // start next ADC conversion (trigger ADC interrupt if ADIE flag is set)
  OCR1BL = lut[255];                   // submit amplitude to PWM register (actually this is done in advance (about 140us) of phase-change, so that phase-delays in key-shaping circuit filter can settle)
  si5351.SendPLLBRegisterBulk();       // submit frequency registers to SI5351 over 731kbit/s I2C (transfer takes 64/731 = 88us, then PLL-loopfilter probably needs 50us to stabalize)
  int16_t adc = ADC - 512; // current ADC sample 10-bits analog input, NOTE: first ADCL, then ADCH
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive);
  int16_t df = in;
  si5351.freq_calc_fast(df);           // calculate SI5351 registers based on frequency shift and carrier frequency
}

volatile int8_t cwdec = 0;

static int32_t signal;
static int16_t avg = 0;
static int16_t maxpk=0;
static int16_t k0=0;
static int16_t k1=0;
static uint8_t sym;
static int16_t ta=0;
const char m2c[] PROGMEM = "**ETIANMSURWDKGOHVF*L*PJBXCYZQ**54S3***2**+***J16=/***H*7*G*8*90************?_****\"**.****@***'**-********;!*)*****,****:****";
static uint8_t nsamp=0;

char cw(int16_t in)
{
  char ch = 0;
  int i;
  signal += abs(in);
  #define OSR 64 // (8*FS/1000)
  if((nsamp % OSR) == 0){   // process every 8 ms
    nsamp=0;
    if(!signal) return ch;
    signal = signal / OSR;  //normalize
    maxpk = signal > maxpk ? signal : maxpk;
    #define RT  4
    if(signal>(maxpk/2) ){  // threshold: 3dB below max signal
      k1++;  // key on
      k0=0;
    } else {
      k0++;  // key off
      if(k0>0 && k1>0){        //symbol space
        if(k1>(ta/100)) ta=RT*ta/100+(100-RT)*k1;                   // go slower
        if(k1>(ta/600) && k1<(ta/300)) ta=(100-RT)*ta/100+RT*k1*3;  // go faster
        if(k1>(ta/600)) sym=(sym<<1)|(k1>(ta/200));        // dit (0) or dash (1)
        k1=0;
      }
      if(k0>=(ta/200) && sym>1){ //letter space
        if(sym<128) ch=/*m2c[sym]*/ pgm_read_byte_near(m2c + sym);
        sym=1;
      }
      if(k0>=(ta/67)){        //word space (67=1.5/100)
        ch = ' ';
        k0=-1000*(ta/100);           //delay word spaces
      }
    }
    avg = avg*99/100 + signal*1/100;
    maxpk = maxpk*99/100 + signal*1/100;
    signal = 0;
  }
  nsamp++;
  return ch;
}

static char out[] = "                ";
volatile bool cw_event = false;

#define F_SAMP_PWM (78125/1)
//#define F_SAMP_RX 78125
#define F_SAMP_RX 62500  //overrun; sample rate of 55500 can be obtained
//#define F_SAMP_RX 52083
//#define F_SAMP_RX 44643
//#define F_SAMP_RX 39062
//#define F_SAMP_RX 34722
//#define F_SAMP_RX 31250
//#define F_SAMP_RX 28409
#define F_ADC_CONV (192307/1)  // finding: tiny-clicks above noise-floor occur with 192kHz ADC conversion-rate and 78kHz PWM output, can be resolved by either lower down PWM or conversation-rate

volatile bool agc = true;
volatile uint8_t nr = 0;
volatile uint8_t att = 0;
volatile uint8_t att2 = 0;
volatile uint8_t _init;

//static uint32_t gain = 1024;
static int16_t gain = 1024;
inline int16_t process_agc(int16_t in)
{
  //int16_t out = ((uint32_t)(gain) >> 20) * in;
  //gain = gain + (1024 - abs(out) + 512);
  int16_t out = (gain >= 1024) ? (gain >> 10) * in : in;
  //if(gain >= 1024) out = (gain >> 10) * in;  // net gain >= 1
  //else if(gain >= 16) out = ((gain >> 4) * in) >> 6;  // net gain < 1
  //else out = (gain * in) >> 10;
  int16_t accum = (1 - abs(out >> 10));
  if((INT16_MAX - gain) > accum) gain = gain + accum;
  if(gain < 1) gain = 1;
  return out;
}

inline int16_t process_nr_old(int16_t ac)
{
  ac = ac >> (6-abs(ac));  // non-linear below amp of 6; to reduce noise (switchoff agc and tune-up volume until noise dissapears, todo:extra volume control needed)
  ac = ac << 3;
  return ac;
}

#define EA(y, x, one_over_alpha)  (y) = (y) + ((x) - (y)) / (one_over_alpha); // exponental averaging [Lyons 13.33.1]
#define MLEA(y, x, L, M)  (y)  = (y) + ((((x) - (y)) >> (L)) - (((x) - (y)) >> (M))); // multiplierless exponental averaging [Lyons 13.33.1], with alpha=1/2^L - 1/2^M

inline int16_t process_nr_old2(int16_t ac)
{  
  int16_t x = ac;
  static int16_t ea1;
  //ea1 = MLEA(ea1, ac, 5, 6); // alpha=0.0156
  ea1 = EA(ea1, ac, 64); // alpha=1/64=0.0156
  //static int16_t ea2;
  //ea2 = EA(ea2, ea1, 64); // alpha=1/64=0.0156
 
  return ea1;
}

inline int16_t process_nr(int16_t in)
{ 
/*
  static int16_t avg;
  avg = EA(avg, abs(in), 64); // alpha=1/64=0.0156
param_c = avg;
*/

/*
  int32_t _avg = 64 * avg;
//  if(_avg > 4) _avg = 4;  // clip
//  uint16_t brs_avgsq = 1 << (_avg * _avg);
  if(_avg > 14) _avg = 14;  // clip
  uint16_t brs_avgsq = 1 << (_avg);

  
  int16_t inv_gain;
  if(brs_avgsq > 1) inv_gain = brs_avgsq / (brs_avgsq - 1);  // = 1 / (1 - 1/(1 << (1*avg*avg)) );
  else inv_gain = 32768;*/

  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr-1) );
  //static int16_t ea2;
  //ea2 = EA(ea2, ea1, inv_gain);

  return ea1;
}

#define N_FILT 7
volatile int8_t filt = 0;
int8_t prev_filt[] = { 0 , 4 }; // default filter for modes resp. CW, SSB

inline int16_t filt_var(int16_t za0)  //filters build with www.micromodeler.com
{ 
  static int16_t za1,za2;
  static int16_t zb0,zb1,zb2;
  static int16_t zc0,zc1,zc2;
  
  if(filt < 4)
  {  // for SSB filters
    // 1st Order (SR=8kHz) IIR in Direct Form I, 8x8:16
    static int16_t zz1,zz2;
    za0=(29*(za0-zz1)+50*za1)/64;                               //300-Hz
    zz2=zz1;
    zz1=za0;

    // 4th Order (SR=8kHz) IIR in Direct Form I, 8x8:16
    switch(filt){
      case 1: zb0=za0; break; //0-4000Hz (pass-through)
      case 2: zb0=(10*(za0+2*za1+za2)+16*zb1-17*zb2)/32; break;    //0-2500Hz  elliptic -60dB@3kHz
      case 3: zb0=(7*(za0+2*za1+za2)+48*zb1-18*zb2)/32; break;     //0-1700Hz  elliptic
    }
  
    switch(filt){
      case 1: zc0=zb0; break; //0-4000Hz (pass-through)
      case 2: zc0=(8*(zb0+zb2)+13*zb1-43*zc1-52*zc2)/64; break;   //0-2500Hz  elliptic -60dB@3kHz
      case 3: zc0=(4*(zb0+zb1+zb2)+22*zc1-47*zc2)/64; break;   //0-1700Hz  elliptic
    }
  
    zc2=zc1;
    zc1=zc0;
  
    zb2=zb1;
    zb1=zb0;
  
    za2=za1;
    za1=za0;
    
    return zc0;
  } else { // for CW filters
    //   (2nd Order (SR=4465Hz) IIR in Direct Form I, 8x8:16), adding 64x front-gain (to deal with later division)
    if(cw_tone == 0){
      switch(filt){
        case 4: zb0=(za0+2*za1+za2)/2+(41L*zb1-23L*zb2)/32; break;   //500-1000Hz
        case 5: zb0=5*(za0-2*za1+za2)+(105L*zb1-58L*zb2)/64; break;   //650-840Hz
        case 6: zb0=3*(za0-2*za1+za2)+(108L*zb1-61L*zb2)/64; break;   //650-750Hz
        case 7: zb0=(2*za0-3*za1+2*za2)+(111L*zb1-62L*zb2)/64; break; //630-680Hz
      }
    
      switch(filt){
        case 4: zc0=(zb0-2*zb1+zb2)/4+(105L*zc1-52L*zc2)/64; break;      //500-1000Hz
        case 5: zc0=((zb0+2*zb1+zb2)+97L*zc1-57L*zc2)/64; break;      //650-840Hz
        case 6: zc0=((zb0+zb1+zb2)+104L*zc1-60L*zc2)/64; break;       //650-750Hz
        case 7: zc0=((zb1)+109L*zc1-62L*zc2)/64; break;               //630-680Hz
      }
    }
    if(cw_tone == 1){
      switch(filt){
        case 4: zb0=(5*za0+9*za1+5*za2)+(30L*zb1-38L*zb2)/64; break; //720Hz+-250Hz
        case 5: zb0=(2*za0+4*za1+2*za2)+(51L*zb1-52L*zb2)/64; break; //720Hz+-100Hz
        case 6: zb0=(1*za0+2*za1+1*za2)+(59L*zb1-58L*zb2)/64; break; //720Hz+-50Hz
        case 7: zb0=(0*za0+1*za1+0*za2)+(66L*zb1-61L*zb2)/64; break; //720Hz+-25Hz
      }
    
      switch(filt){
        case 4: zc0=(zb0-2*zb1+zb2)/4+(76L*zc1-44L*zc2)/64; break; //720Hz+-250Hz
        case 5: zc0=(zb0-2*zb1+zb2)/8+(72L*zc1-53L*zc2)/64; break; //720Hz+-100Hz
        case 6: zc0=(zb0-2*zb1+zb2)/16+(70L*zc1-58L*zc2)/64; break; //720Hz+-50Hz
        case 7: zc0=(zb0-2*zb1+zb2)/32+(70L*zc1-62L*zc2)/64; break; //720Hz+-25Hz
      } 
    }
    zc2=zc1;
    zc1=zc0;
  
    zb2=zb1;
    zb1=zb0;
  
    za2=za1;
    za1=za0;
    
    return zc0 / 64; // compensate the 64x front-end gain
  }
}

static uint32_t absavg256 = 0;
volatile uint32_t _absavg256 = 0;
volatile int16_t i, q;

inline int16_t slow_dsp(int16_t ac)
{
  static uint8_t absavg256cnt;
  if(!(absavg256cnt--)){ _absavg256 = absavg256; absavg256 = 0; 
//#define AUTO_ADC_BIAS  1
#ifdef AUTO_ADC_BIAS
    if(param_b < 0){
      pinMode(AUDIO1, INPUT_PULLUP);
      pinMode(AUDIO1, INPUT);
    }
    if(param_c < 0){
      pinMode(AUDIO2, INPUT_PULLUP);
      pinMode(AUDIO2, INPUT);
    }
    if(param_b > 500){
      pinMode(AUDIO1, OUTPUT);
      digitalWrite(AUDIO1, LOW);
      pinMode(AUDIO1, INPUT);
    }
    if(param_c > 500){
      pinMode(AUDIO2, OUTPUT);
      digitalWrite(AUDIO2, LOW);
      pinMode(AUDIO2, INPUT);
    }
#endif
  } else absavg256 += abs(ac);

  if(mode == AM) { // (12%CPU for the mode selection etc)
    ac = magn(i, q);  //(25%CPU)
    { static int16_t dc;
      dc += (ac - dc) / 2;
      ac = ac - dc; }  // DC decoupling
  } else if(mode == FM){
    static int16_t z1;
    int16_t z0 = arctan3(q, i);
    ac = z0 - z1; // Differentiator
    z1 = z0;
    //ac = ac * (F_SAMP_RX/R) / _UA;  // =ac*3.5 -> skip
  }  // needs: p.12 https://www.veron.nl/wp-content/uploads/2014/01/FmDemodulator.pdf
  else { ; }  // USB, LSB, CW
  if(agc) ac = process_agc(ac);
  ac = ac >> (16-volume);
  if(nr) ac = process_nr(ac);

  if(filt) ac = filt_var(ac) << 2;
  if(mode == CW){
    if(cwdec){  // CW decoder enabled?
      char ch = cw(ac >> 0);
      if(ch){
        for(int i=0; i!=15;i++) out[i]=out[i+1];
        out[15] = ch;
        cw_event = true;
      }
    }
  }
  //if(!(absavg256cnt--)){ _absavg256 = absavg256; absavg256 = 0; } else absavg256 += abs(ac);  //hack
  
  //static int16_t dc;
  //dc += (ac - dc) / 2;
  //dc = (15*dc + ac)/16;
  //dc = (15*dc + (ac - dc))/16;
  //ac = ac - dc;    // DC decoupling

  ac = min(max(ac, -512), 511);
  //ac = min(max(ac, -128), 127);
  return ac;
}

typedef void (*func_t)(void);
volatile func_t func_ptr;
#undef  R  // Decimating 2nd Order CIC filter
#define R 4  // Rate change from 62500/2 kSPS to 7812.5SPS, providing 12dB gain

//#define SIMPLE_RX  1
#ifndef SIMPLE_RX
volatile uint8_t admux[3];
volatile int16_t ocomb, qh;
volatile uint8_t rx_state = 0;

// Non-recursive CIC Filter (M=2, R=4) implementation, so two-stages of (followed by down-sampling with factor 2):
// H1(z) = (1 + z^-1)^2 = 1 + 2*z^-1 + z^-2 = (1 + z^-2) + (2) * z^-1 = FA(z) + FB(z) * z^-1;
// with down-sampling before stage translates into poly-phase components: FA(z) = 1 + z^-1, FB(z) = 2
// source: Lyons Understanding Digital Signal Processing 3rd edition 13.24.1
void sdr_rx()
{
  // process I for even samples  [75% CPU@R=4;Fs=62.5k] (excluding the Comb branch and output stage)
  ADMUX = admux[1];  // set MUX for next conversion
  ADCSRA |= (1 << ADSC);    // start next ADC conversion
  int16_t adc = ADC - 511; // current ADC sample 10-bits analog input, NOTE: first ADCL, then ADCH
  func_ptr = sdr_rx_q;    // processing function for next conversion
  sdr_rx_common();
  
  // Only for I: correct I/Q sample delay by means of linear interpolation
  static int16_t prev_adc;
  int16_t corr_adc = (prev_adc + adc) / 2;
  prev_adc = adc;
  adc = corr_adc;

  //static int16_t dc;
  //dc += (adc - dc) / 2;  // we lose LSB with this method
  //dc = (3*dc + adc)/4;
  //int16_t ac = adc - dc;     // DC decoupling
  int16_t ac = adc;

#ifdef AUTO_ADC_BIAS
  param_b = (7*param_b + adc)/8;
#endif
  int16_t ac2;
  static int16_t z1;
  if(rx_state == 0 || rx_state == 4){  // 1st stage: down-sample by 2
    static int16_t za1;
    int16_t _ac = ac + za1 + z1 * 2;           // 1st stage: FA + FB
    za1 = ac;
    static int16_t _z1;
    if(rx_state == 0){                   // 2nd stage: down-sample by 2
      static int16_t _za1;
      ac2 = _ac + _za1 + _z1 * 2;              // 2nd stage: FA + FB
      _za1 = _ac;
      {
        ac2 >>= att2;  // digital gain control
        // post processing I and Q (down-sampled) results
        static int16_t v[7];
        i = v[0]; v[0] = v[1]; v[1] = v[2]; v[2] = v[3]; v[3] = v[4]; v[4] = v[5]; v[5] = v[6]; v[6] = ac2;  // Delay to match Hilbert transform on Q branch
        
        int16_t ac = i + qh;
        ac = slow_dsp(ac);

        // Output stage
        static int16_t ozd1, ozd2;
        if(_init){ ac = 0; ozd1 = 0; ozd2 = 0; _init = 0; } // hack: on first sample init accumlators of further stages (to prevent instability)
#define SECOND_ORDER_DUC  1
#ifdef SECOND_ORDER_DUC
        int16_t od1 = ac - ozd1; // Comb section
        ocomb = od1 - ozd2;
        ozd2 = od1;
#else
        ocomb = ac - ozd1; // Comb section
#endif
        ozd1 = ac;
      }
    } else _z1 = _ac;
  } else z1 = ac;

  rx_state++;
}

void sdr_rx_q()
{
  // process Q for odd samples  [75% CPU@R=4;Fs=62.5k] (excluding the Comb branch and output stage)
  ADMUX = admux[0];  // set MUX for next conversion
  ADCSRA |= (1 << ADSC);    // start next ADC conversion
  int16_t adc = ADC - 511; // current ADC sample 10-bits analog input, NOTE: first ADCL, then ADCH
  func_ptr = sdr_rx;    // processing function for next conversion
#ifdef SECOND_ORDER_DUC
//  sdr_rx_common();  //necessary? YES!... Maybe NOT!
#endif

  //static int16_t dc;
  //dc += (adc - dc) / 2;  // we lose LSB with this method
  //dc = (3*dc + adc)/4;
  //int16_t ac = adc - dc;     // DC decoupling
  int16_t ac = adc;

#ifdef AUTO_ADC_BIAS
  param_c = (7*param_c + adc)/8;
#endif
  int16_t ac2;
  static int16_t z1;
  if(rx_state == 3 || rx_state == 7){  // 1st stage: down-sample by 2
    static int16_t za1;
    int16_t _ac = ac + za1 + z1 * 2;           // 1st stage: FA + FB
    za1 = ac;
    static int16_t _z1;
    if(rx_state == 7){                   // 2nd stage: down-sample by 2
      static int16_t _za1;
      ac2 = _ac + _za1 + _z1 * 2;              // 2nd stage: FA + FB
      _za1 = _ac;
      {
        ac2 >>= att2;  // digital gain control
        // Process Q (down-sampled) samples
        static int16_t v[14];
        q = v[7];
        qh = ((v[0] - ac2) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 + (v[6] - v[8]) * 15) / 128 + (v[6] - v[8]) / 2; // Hilbert transform, 40dB side-band rejection in 400..1900Hz (@4kSPS) when used in image-rejection scenario; (Hilbert transform require 5 additional bits)
        for(uint8_t j = 0; j != 13; j++) v[j] = v[j + 1]; v[13] = ac2;
      }
      rx_state = 0; return;
    } else _z1 = _ac;
  } else z1 = ac;

  rx_state++;
}

inline void sdr_rx_common()
{
  static int16_t ozi1, ozi2;
  if(_init){ ocomb=0; ozi1 = 0; ozi2 = 0; } // hack
  // Output stage [25% CPU@R=4;Fs=62.5k]
#ifdef SECOND_ORDER_DUC
  ozi2 = ozi1 + ozi2;          // Integrator section
#endif
  ozi1 = ocomb + ozi1;
#ifdef SECOND_ORDER_DUC
  if(volume) OCR1AL = min(max((ozi2>>5) + 128, 0), 255);  //if(volume) OCR1AL = min(max((ozi2>>5) + ICR1L/2, 0), ICR1L);  // center and clip wrt PWM working range
#else
  if(volume) OCR1AL = (ozi1>>5) + 128;
  //if(volume) OCR1AL = min(max((ozi1>>5) + 128, 0), 255);  //if(volume) OCR1AL = min(max((ozi2>>5) + ICR1L/2, 0), ICR1L);  // center and clip wrt PWM working range
#endif
}
#endif

#ifdef SIMPLE_RX
volatile uint8_t admux[3];
static uint8_t rx_state = 0;

static struct rx {
  int16_t z1;
  int16_t za1;
  int16_t _z1;
  int16_t _za1;
} rx_inst[2];

void sdr_rx()
{
  static int16_t ocomb;
  static int16_t qh;

  uint8_t b = !(rx_state & 0x01);
  rx* p = &rx_inst[b];
  uint8_t _rx_state;
  int16_t ac;
  if(b){  // rx_state == 0, 2, 4, 6 -> I-stage
    ADMUX = admux[1];  // set MUX for next conversion
    ADCSRA |= (1 << ADSC);    // start next ADC conversion
    ac = ADC - 512; // current ADC sample 10-bits analog input, NOTE: first ADCL, then ADCH

    //sdr_common
    static int16_t ozi1, ozi2;
    if(_init){ ocomb=0; ozi1 = 0; ozi2 = 0; } // hack
    // Output stage [25% CPU@R=4;Fs=62.5k]
    #define SECOND_ORDER_DUC 1
    #ifdef SECOND_ORDER_DUC
    ozi2 = ozi1 + ozi2;          // Integrator section
    #endif
    ozi1 = ocomb + ozi1;
    #ifdef SECOND_ORDER_DUC
    if(volume) OCR1AL = min(max((ozi2>>5) + 128, 0), 255);  //if(volume) OCR1AL = min(max((ozi2>>5) + ICR1L/2, 0), ICR1L);  // center and clip wrt PWM working range
    #else
    if(volume) OCR1AL = (ozi1>>5) + 128;
    //if(volume) OCR1AL = min(max((ozi1>>5) + 128, 0), 255);  //if(volume) OCR1AL = min(max((ozi2>>5) + ICR1L/2, 0), ICR1L);  // center and clip wrt PWM working range
    #endif
    // Only for I: correct I/Q sample delay by means of linear interpolation
    static int16_t prev_adc;
    int16_t corr_adc = (prev_adc + ac) / 2;
    prev_adc = ac;
    ac = corr_adc;
    _rx_state = ~rx_state;
  } else {
    ADMUX = admux[0];  // set MUX for next conversion
    ADCSRA |= (1 << ADSC);    // start next ADC conversion
    ac = ADC - 512; // current ADC sample 10-bits analog input, NOTE: first ADCL, then ADCH
    _rx_state = rx_state;
  }
    
  if(_rx_state & 0x02){  // rx_state == I: 0, 4  Q: 3, 7  1st stage: down-sample by 2
    int16_t _ac = ac + p->za1 + p->z1 * 2;           // 1st stage: FA + FB
    p->za1 = ac;
    if(_rx_state & 0x04){                   // rx_state == I: 0  Q:7   2nd stage: down-sample by 2
      int16_t ac2 = _ac + p->_za1 + p->_z1 * 2;              // 2nd stage: FA + FB
      p->_za1 = _ac;
      if(b){
        // post processing I and Q (down-sampled) results
        ac2 >>= att2;  // digital gain control
        // post processing I and Q (down-sampled) results
        static int16_t v[7];
        i = v[0]; v[0] = v[1]; v[1] = v[2]; v[2] = v[3]; v[3] = v[4]; v[4] = v[5]; v[5] = v[6]; v[6] = ac2;  // Delay to match Hilbert transform on Q branch

        int16_t ac = i + qh;
        ac = slow_dsp(ac);

        // Output stage
        static int16_t ozd1, ozd2;
        if(_init){ ac = 0; ozd1 = 0; ozd2 = 0; _init = 0; } // hack: on first sample init accumlators of further stages (to prevent instability)
        #ifdef SECOND_ORDER_DUC
        int16_t od1 = ac - ozd1; // Comb section
        ocomb = od1 - ozd2;
        ozd2 = od1;
        #else
        ocomb = ac - ozd1; // Comb section
        #endif
        ozd1 = ac;
      } else {
        ac2 >>= att2;  // digital gain control
        // Process Q (down-sampled) samples
        static int16_t v[14];
        q = v[7];
        qh = ((v[0] - ac2) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 + (v[6] - v[8]) * 15) / 128 + (v[6] - v[8]) / 2; // Hilbert transform, 40dB side-band rejection in 400..1900Hz (@4kSPS) when used in image-rejection scenario; (Hilbert transform require 5 additional bits)
        for(uint8_t j = 0; j != 13; j++) v[j] = v[j + 1]; v[13] = ac2;
      }
    } else p->_z1 = _ac;
  } else p->z1 = ac;  // rx_state == I: 2, 6  Q: 1, 5

  rx_state++;
}
//#pragma GCC push_options
//#pragma GCC optimize ("Ofast")  // compiler-optimization for speed
//#pragma GCC pop_options  // end of DSP section
// */
#endif

ISR(TIMER2_COMPA_vect)  // Timer2 COMPA interrupt
{
  func_ptr();
#ifdef DEBUG
  numSamples++;
#endif
}

void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs)
{
#ifndef AUTO_ADC_BIAS
  DIDR0 |= (1 << adcpin); // disable digital input
#endif  
  ADCSRA = 0;             // clear ADCSRA register
  ADCSRB = 0;             // clear ADCSRB register
  ADMUX = 0;              // clear ADMUX register
  ADMUX |= (adcpin & 0x0f);    // set analog input pin
  ADMUX |= ((ref1v1) ? (1 << REFS1) : 0) | (1 << REFS0);  // set AREF=1.1V (Internal ref); otherwise AREF=AVCC=(5V)
  ADCSRA |= ((uint8_t)log2((uint8_t)(F_CPU / 13 / fs))) & 0x07;  // ADC Prescaler (for normal conversions non-auto-triggered): ADPS = log2(F_CPU / 13 / Fs) - 1; ADSP=0..7 resulting in resp. conversion rate of 1536, 768, 384, 192, 96, 48, 24, 12 kHz
  //ADCSRA |= (1 << ADIE);  // enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN);  // enable ADC
  //ADCSRA |= (1 << ADSC);  // start ADC measurements
#ifdef ADC_NR
//  set_sleep_mode(SLEEP_MODE_ADC);  // ADC NR sleep destroys the timer2 integrity, therefore Idle sleep is better alternative (keeping clkIO as an active clock domain)
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
#endif
}

void adc_stop()
{
  //ADCSRA &= ~(1 << ADATE); // disable auto trigger
  ADCSRA &= ~(1 << ADIE);  // disable interrupts when measurement complete
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);    // 128 prescaler for 9.6kHz
#ifdef ADC_NR
  sleep_disable();
#endif
  ADMUX = (1 << REFS0);  // restore reference voltage AREF (5V)
}

void timer1_start(uint32_t fs)
{  // Timer 1: OC1A and OC1B in PWM mode
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1A |= (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11); // Clear OC1A/OC1B on compare match, set OC1A/OC1B at BOTTOM (non-inverting mode)
  TCCR1B |= (1 << CS10) | (1 << WGM13) | (1 << WGM12); // Mode 14 - Fast PWM;  CS10: clkI/O/1 (No prescaling)
  ICR1H = 0x00;
  ICR1L = min(255, (float)F_CPU / (float)fs - 0.5);  // PWM value range (fs>78431):  Fpwm = F_CPU / [Prescaler * (1 + TOP)]
  //TCCR1A |= (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10); // Clear OC1A/OC1B on compare match, set OC1A/OC1B at BOTTOM (non-inverting mode)
  //TCCR1B |= (1 << CS10) | (1 << WGM12); // Mode 5 - Fast PWM, 8-bit;  CS10: clkI/O/1 (No prescaling)
  OCR1AH = 0x00;
  OCR1AL = 0x00;  // OC1A (SIDETONE) PWM duty-cycle (span defined by ICR).
  OCR1BH = 0x00;
  OCR1BL = 0x00;  // OC1B (KEY_OUT) PWM duty-cycle (span defined by ICR).
}

void timer1_stop()
{
  OCR1AL = 0x00;
  OCR1BL = 0x00;
}

void timer2_start(uint32_t fs)
{  // Timer 2: interrupt mode
  ASSR &= ~(1 << AS2);  // Timer 2 clocked from CLK I/O (like Timer 0 and 1)
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  TCCR2A |= (1 << WGM21); // WGM21: Mode 2 - CTC (Clear Timer on Compare Match)
  TCCR2B |= (1 << CS22);  // Set C22 bits for 64 prescaler
  TIMSK2 |= (1 << OCIE2A);  // enable timer compare interrupt TIMER2_COMPA_vect
  uint8_t ocr = (((float)F_CPU / (float)64) / (float)fs + 0.5) - 1;   // OCRn = (F_CPU / pre-scaler / fs) - 1;
  OCR2A = ocr;
}

void timer2_stop()
{ // Stop Timer 2 interrupt
  TIMSK2 &= ~(1 << OCIE2A);  // disable timer compare interrupt
  delay(1);  // wait until potential in-flight interrupts are finished
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Below a radio-specific implementation based on the above components (seperation of concerns)
//
// Feel free to replace it with your own custom radio implementation :-)

char blanks[] = "        ";
#define lcd_blanks() lcd.print(blanks);

#define N_FONTS  8
const byte fonts[N_FONTS][8] PROGMEM = {
{ 0b01000,  // 1; logo
  0b00100,
  0b01010,
  0b00101,
  0b01010,
  0b00100,
  0b01000,
  0b00000 },
{ 0b00000,  // 2; s-meter, 0 bars
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000 },
{ 0b10000,  // 3; s-meter, 1 bars
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000 },
{ 0b10000,  // 4; s-meter, 2 bars
  0b10000,
  0b10100,
  0b10100,
  0b10100,
  0b10100,
  0b10100,
  0b10100 },
{ 0b10000,  // 5; s-meter, 3 bars
  0b10000,
  0b10101,
  0b10101,
  0b10101,
  0b10101,
  0b10101,
  0b10101 },
{ 0b01100,  // 6; vfo-a
  0b10010,
  0b11110,
  0b10010,
  0b10010,
  0b00000,
  0b00000,
  0b00000 },
{ 0b11100,  // 7; vfo-b
  0b10010,
  0b11100,
  0b10010,
  0b11100,
  0b00000,
  0b00000,
  0b00000 },
{ 0b00000,  // 8; tbd
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000 }
};

int analogSafeRead(uint8_t pin)
{  // performs classical analogRead with default Arduino sample-rate and analog reference setting; restores previous settings
  noInterrupts();
  uint8_t adcsra = ADCSRA;
  uint8_t admux = ADMUX;
  ADCSRA &= ~(1 << ADIE);  // disable interrupts when measurement complete
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);    // 128 prescaler for 9.6kHz
  ADMUX = (1 << REFS0);  // restore reference voltage AREF (5V)
  delay(1);  // settle
  int val = analogRead(pin);
  ADCSRA = adcsra;
  ADMUX = admux;
  interrupts();
  return val;
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

void initPins(){  
  // initialize
  digitalWrite(SIG_OUT, LOW);
  digitalWrite(RX, HIGH);
  digitalWrite(KEY_OUT, LOW);
  digitalWrite(SIDETONE, LOW);

  // pins
  pinMode(SIDETONE, OUTPUT);
  pinMode(SIG_OUT, OUTPUT);
  pinMode(RX, OUTPUT);
  pinMode(KEY_OUT, OUTPUT);
//#define ONEBUTTON  1
#ifdef ONEBUTTON
  pinMode(BUTTONS, INPUT_PULLUP);  // rotary button
#else
  pinMode(BUTTONS, INPUT);  // L/R/rotary button
#endif
  pinMode(DIT, INPUT_PULLUP);
  //pinMode(DAH, INPUT);  
  pinMode(DAH, INPUT_PULLUP); // Could this replace D4?

  digitalWrite(AUDIO1, LOW);  // when used as output, help can mute RX leakage into AREF
  digitalWrite(AUDIO2, LOW);
  pinMode(AUDIO1, INPUT);
  pinMode(AUDIO2, INPUT);

#ifdef NTX
  digitalWrite(NTX, HIGH);
  pinMode(NTX, OUTPUT);
#endif
}

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

static char cat[20];  // temporary here
static uint8_t nc = 0;
static uint16_t cat_cmd = 0;

void parse_cat(uint8_t in){   // TS480 CAT protocol:  https://www.kenwood.com/i/products/info/amateur/ts_480/pdf/ts_480_pc.pdf
  if(nc == 0) cat_cmd = in << 8;
  if(nc == 1) cat_cmd += in;
  if(in == ';'){  // end of cat command -> parse and process

    switch(cat_cmd){
      case 'I'<<8|'F': Serial.write("IF00010138000     +00000000002000000 ;"); lcd.setCursor(0, 0); lcd.print("IF"); break;
    }
    nc = 0;       // reset
  } else {
    if(nc < (sizeof(cat) - 1)) cat[nc++] = in; // buffer and count-up
  }
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
