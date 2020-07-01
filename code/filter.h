#ifndef FILTER_HEADER
#define FILTER_HEADER

// FIXME: relies on CW_TONE

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

#endif
