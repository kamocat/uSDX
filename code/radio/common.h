enum mode_t { LSB, USB, CW, AM, FM };
volatile int8_t mode = USB;
volatile uint16_t numSamples = 0;

volatile uint8_t tx = 0;
volatile bool vox = false;

inline void _vox(uint8_t trigger)
{
  if(trigger){
    //if(!tx){ /* TX can be enabled here */ }
    tx = (vox) ? 255 : 1; // hangtime = 255 / 4402 = 58ms (the time that TX at least stays on when not triggered again)
  } else {
    if(tx){
      tx--;
      //if(!tx){ /* RX can be enabled here */ }
    }
  }
}


static char out[] = "                ";
volatile bool cw_event = false;
uint8_t lut[256];
volatile uint8_t amp;
volatile uint8_t vox_thresh = (1 << 2);
volatile uint8_t drive = 2;   // hmm.. drive>2 impacts cpu load..why?



#define MIC_ATTEN  0  // 0*6dB attenuation (note that the LSB bits are quite noisy)
volatile int8_t mox = 0;
volatile int8_t volume = 12;

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

uint8_t reg;
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
