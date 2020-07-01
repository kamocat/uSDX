class SI5351 {
public:
  volatile int32_t _fout;
  volatile uint8_t _div;  // note: uint8_t asserts fout > 3.5MHz with R_DIV=1
  volatile uint16_t _msa128min512;
  volatile uint32_t _msb128;
  volatile uint8_t pll_regs[8];

  #define BB0(x) ((uint8_t)(x))           // Bash byte x of int32_t
  #define BB1(x) ((uint8_t)((x)>>8))
  #define BB2(x) ((uint8_t)((x)>>16))

  #define FAST __attribute__((optimize("Ofast")))

  #define F_XTAL 27005000            // Crystal freq in Hz, nominal frequency 27004300
  //#define F_XTAL 25004000          // Alternate SI clock
  //#define F_XTAL 20004000          // A shared-single 20MHz processor/pll clock
  volatile uint32_t fxtal = F_XTAL;

  inline void FAST freq_calc_fast(int16_t df)  // note: relies on cached variables: _msb128, _msa128min512, _div, _fout, fxtal
  { 
    #define _MSC  0x80000  //0x80000: 98% CPU load   0xFFFFF: 114% CPU load
    uint32_t msb128 = _msb128 + ((int64_t)(_div * (int32_t)df) * _MSC * 128) / fxtal;

    //#define _MSC  0xFFFFF  // Old algorithm 114% CPU load, shortcut for a fixed fxtal=27e6
    //register uint32_t xmsb = (_div * (_fout + (int32_t)df)) % fxtal;  // xmsb = msb * fxtal/(128 * _MSC);
    //uint32_t msb128 = xmsb * 5*(32/32) - (xmsb/32);  // msb128 = xmsb * 159/32, where 159/32 = 128 * 0xFFFFF / fxtal; fxtal=27e6

    //#define _MSC  (F_XTAL/128)  // 114% CPU load  perfect alignment
    //uint32_t msb128 = (_div * (_fout + (int32_t)df)) % fxtal;

    uint32_t msp1 = _msa128min512 + msb128 / _MSC;  // = 128 * _msa + msb128 / _MSC - 512;
    uint32_t msp2 = msb128 % _MSC;  // = msb128 - msb128/_MSC * _MSC;

    //pll_regs[0] = BB1(msc);  // 3 regs are constant
    //pll_regs[1] = BB0(msc);
    //pll_regs[2] = BB2(msp1);
    pll_regs[3] = BB1(msp1);
    pll_regs[4] = BB0(msp1);
    pll_regs[5] = ((_MSC&0xF0000)>>(16-4))|BB2(msp2); // top nibble MUST be same as top nibble of _MSC !
    pll_regs[6] = BB1(msp2);
    pll_regs[7] = BB0(msp2);
  }
  #define SI5351_ADDR 0x60              // I2C address of Si5351   (typical)

  inline void SendPLLBRegisterBulk(){
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(26+1*8 + 3);  // Write to PLLB
    i2c.SendByte(pll_regs[3]);
    i2c.SendByte(pll_regs[4]);
    i2c.SendByte(pll_regs[5]);
    i2c.SendByte(pll_regs[6]);
    i2c.SendByte(pll_regs[7]);
    i2c.stop();
  }

  void SendRegister(uint8_t reg, uint8_t* data, uint8_t n){
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(reg);
    while (n--) i2c.SendByte(*data++);
    i2c.stop();      
  }
  void SendRegister(uint8_t reg, uint8_t val){ SendRegister(reg, &val, 1); }
/*
  bool dirty;

  void set_pll(uint8_t pll, uint32_t fvco){  // Set PLL (fractional) PLLA=0, PLLB=1
    uint8_t msa; uint32_t msb, msc, msp1, msp2, msp3p2;
    msa = fvco / fxtal;     // Integer part of vco/fxtal
    msb = ((uint64_t)(fvco % fxtal)*_MSC) / fxtal; // Fractional part
    msc = _MSC;

    msp1 = 128*msa + 128*msb/msc - 512;
    msp2 = 128*msb - 128*msb/msc * msc;    // msp3 == msc
    msp3p2 = (((msc & 0x0F0000) <<4) | msp2);  // msp3 on top nibble
    uint8_t pll_regs[8] = { BB1(msc), BB0(msc), BB2(msp1), BB1(msp1), BB0(msp1), BB2(msp3p2), BB1(msp2), BB0(msp2) };
    SendRegister(26+pll*8, pll_regs, 8); // Write to PLL[pll]
  }

  void set_ms(uint8_t clk, uint8_t linked_pll, uint8_t msa, uint8_t rdiv, uint8_t phase){  // Set Multisynth divider (integer) CLK0=0, CLK1=1, CLK2=2
    uint32_t msp1 = (128*msa - 512) | (((uint32_t)rdiv)<<20);     // msp1 and msp2=0, msp3=1, not fractional
    uint8_t ms_regs[8] = {0, 1, BB2(msp1), BB1(msp1), BB0(msp1), 0, 0, 0};
    SendRegister(42+clk*8, ms_regs, 8); // Write to MS[clk]
    SendRegister(16+clk, (linked_pll*0x20)|0x0C|3|0x40);       // CLK[clk]: PLL[pll] local msynth; 3=8mA; 0x40=integer division, bit7:6=0->power-up
    SendRegister(165+clk, phase * msa / 90);  // CLK[clk]: phase (on change -> reset PLL)
    static uint16_t pm[3]; // to detect a need for a PLL reset
    if(pm[clk] != ((phase)*msa/90)){ pm[clk] = (phase)*msa/90; dirty=true; } // 0x20 reset PLLA; 0x80 reset PLLB
  }

  void set_freq(uint8_t clk, uint8_t pll, uint32_t fout, uint8_t phase){
    uint8_t rdiv = 0;             // CLK pin sees fout/(2^rdiv)
    if(fout < 500000){ rdiv = 7; fout *= 128; }; // Divide by 128 for fout 4..500kHz

    uint16_t d = (16 * fxtal) / fout;  // Integer part
    if(fout > 30000000) d = (34 * fxtal) / fout; // when fvco is getting too low (400 MHz)

    if( (d * (fout - 5000) / fxtal) != (d * (fout + 5000) / fxtal) ) d--; // Test if multiplier remains same for freq deviation +/- 5kHz, if not use different divider to make same
    uint32_t fvco = d * fout;  // Variable PLL VCO frequency at integer multiple of fout at around 27MHz*16 = 432MHz

    set_pll(pll, fvco);
    set_ms(clk, pll, fvco / fout, rdiv, phase);

    _fout = fout;  // cache
    _div = d;
    _msa128min512 = fvco / fxtal * 128 - 512;
    _msb128=((uint64_t)(fvco % fxtal)*_MSC*128) / fxtal;
  }

  void pll_reset(){ if(dirty){ dirty=false; SendRegister(177, 0xA0); } } // reset PLLA and PLLB, if necessary

  void oe(uint8_t mask){ SendRegister(3, ~mask); } // Output-enable mask: CLK2=4; CLK1=2; CLK0=1

  void freq(uint32_t f, uint8_t i, uint8_t q){  // Set CLK0,1 (PLLA) to fout Hz with phase i, q, and prepare CLK2 (PLL2).
    set_freq(0, 0, f, i);
    set_freq(1, 0, f, q);
    set_freq(2, 1, f, 0);
    pll_reset();
    oe(3);
  }
*/
  int16_t iqmsa; // to detect a need for a PLL reset

  void freq(uint32_t fout, uint8_t i, uint8_t q){  // Set a CLK0,1 to fout Hz with phase i, q
      uint8_t msa; uint32_t msb, msc, msp1, msp2, msp3p2;
      uint8_t rdiv = 0;             // CLK pin sees fout/(2^rdiv)
      if(fout < 500000){ rdiv = 7; fout *= 128; }; // Divide by 128 for fout 4..500kHz

      uint16_t d = (16 * fxtal) / fout;  // Integer part
      if(fout > 30000000) d = (34 * fxtal) / fout; // when fvco is getting too low (400 MHz)

      if( (d * (fout - 5000) / fxtal) != (d * (fout + 5000) / fxtal) ) d--; // Test if multiplier remains same for freq deviation +/- 5kHz, if not use different divider to make same
      uint32_t fvcoa = d * fout;  // Variable PLLA VCO frequency at integer multiple of fout at around 27MHz*16 = 432MHz
      msa = fvcoa / fxtal;     // Integer part of vco/fxtal
      msb = ((uint64_t)(fvcoa % fxtal)*_MSC) / fxtal; // Fractional part
      msc = _MSC;
      
      msp1 = 128*msa + 128*msb/msc - 512;
      msp2 = 128*msb - 128*msb/msc * msc;    // msp3 == msc        
      msp3p2 = (((msc & 0x0F0000) <<4) | msp2);  // msp3 on top nibble
      uint8_t pll_regs[8] = { BB1(msc), BB0(msc), BB2(msp1), BB1(msp1), BB0(msp1), BB2(msp3p2), BB1(msp2), BB0(msp2) };
      SendRegister(26+0*8, pll_regs, 8); // Write to PLLA
      SendRegister(26+1*8, pll_regs, 8); // Write to PLLB

      msa = fvcoa / fout;     // Integer part of vco/fout
      msp1 = (128*msa - 512) | (((uint32_t)rdiv)<<20);     // msp1 and msp2=0, msp3=1, not fractional
      uint8_t ms_regs[8] = {0, 1, BB2(msp1), BB1(msp1), BB0(msp1), 0, 0, 0};
      SendRegister(42+0*8, ms_regs, 8); // Write to MS0
      SendRegister(42+1*8, ms_regs, 8); // Write to MS1
      SendRegister(42+2*8, ms_regs, 8); // Write to MS2
      SendRegister(16+0, 0x0C|3|0x40);  // CLK0: 0x0C=PLLA local msynth; 3=8mA; 0x40=integer division; bit7:6=0->power-up
      SendRegister(16+1, 0x0C|3|0x40);  // CLK1: 0x0C=PLLA local msynth; 3=8mA; 0x40=integer division; bit7:6=0->power-up
      SendRegister(16+2, 0x2C|3|0x40);  // CLK2: 0x2C=PLLB local msynth; 3=8mA; 0x40=integer division; bit7:6=0->power-up
      SendRegister(165, i * msa / 90);  // CLK0: I-phase (on change -> Reset PLL)
      SendRegister(166, q * msa / 90);  // CLK1: Q-phase (on change -> Reset PLL)
      if(iqmsa != ((i-q)*msa/90)){ iqmsa = (i-q)*msa/90; SendRegister(177, 0xA0); } // 0x20 reset PLLA; 0x80 reset PLLB
      SendRegister(3, 0b11111100);      // Enable/disable clock

      _fout = fout;  // cache
      _div = d;
      _msa128min512 = fvcoa / fxtal * 128 - 512;
      _msb128=((uint64_t)(fvcoa % fxtal)*_MSC*128) / fxtal;
  }

  uint8_t RecvRegister(uint8_t reg){
    i2c.start();  // Data write to set the register address
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(reg);
    i2c.stop();
    i2c.start(); // Data read to retrieve the data from the set address
    i2c.SendByte((SI5351_ADDR << 1) | 1);
    uint8_t data = i2c.RecvByte(true);
    i2c.stop();
    return data;
  }
  void powerDown(){
    for(int addr = 16; addr != 24; addr++) SendRegister(addr, 0b11000000);  // Conserve power when output is disabled
    SendRegister(3, 0b11111111); // Disable all CLK outputs    
  }
  #define SI_CLK_OE 3

};
static SI5351 si5351;
// */

 /*
class SI5351 : public I2C {
public:
  #define SI_I2C_ADDR 0x60  // SI5351A I2C address: 0x60 for SI5351A-B-GT; 0x62 for SI5351A-B-04486-GT; 0x6F for SI5351A-B02075-GT; see here for other variants: https://www.silabs.com/TimingUtility/timing-download-document.aspx?OPN=Si5351A-B02075-GT&OPNRevision=0&FileType=PublicAddendum
  #define SI_CLK_OE 3     // Register definitions
  #define SI_CLK0_CONTROL 16
  #define SI_CLK1_CONTROL 17
  #define SI_CLK2_CONTROL 18
  #define SI_SYNTH_PLL_A 26
  #define SI_SYNTH_PLL_B 34
  #define SI_SYNTH_MS_0 42
  #define SI_SYNTH_MS_1 50
  #define SI_SYNTH_MS_2 58
  #define SI_CLK0_PHOFF 165
  #define SI_CLK1_PHOFF 166
  #define SI_CLK2_PHOFF 167
  #define SI_PLL_RESET 177
  #define SI_MS_INT 0b01000000  // Clock control
  #define SI_CLK_SRC_PLL_A 0b00000000
  #define SI_CLK_SRC_PLL_B 0b00100000
  #define SI_CLK_SRC_MS 0b00001100
  #define SI_CLK_IDRV_8MA 0b00000011
  #define SI_CLK_INV 0b00010000
  volatile uint32_t fxtal = 27004300;  //myqcx1:27003847 myqcx2:27004900  Actual crystal frequency of 27MHz XTAL2 for CL = 10pF (default), calibrate your QCX 27MHz crystal frequency here
  #define SI_PLL_FREQ (16*fxtal)  //900000000, with 432MHz(=16*27M) PLL freq, usable range is 3.46..100MHz

  volatile uint8_t prev_divider;
  volatile int32_t raw_freq;
  volatile uint8_t divider;  // note: because of int8 only freq > 3.6MHz can be covered for R_DIV=1
  volatile uint8_t mult;
  volatile uint8_t pll_regs[8];
  volatile int32_t iqmsa;
  volatile int32_t pll_freq;   // temporary
  
  SI5351(){
    init();
    iqmsa = 0;
  }
  uint8_t RecvRegister(uint8_t reg)
  {
    // Data write to set the register address
    start();
    SendByte(SI_I2C_ADDR << 1);
    SendByte(reg);
    stop();
    // Data read to retrieve the data from the set address
    start();
    SendByte((SI_I2C_ADDR << 1) | 1);
    uint8_t data = RecvByte(true);
    stop();
    return data;
  }
  void SendRegister(uint8_t reg, uint8_t data)
  {
    start();
    SendByte(SI_I2C_ADDR << 1);
    SendByte(reg);
    SendByte(data);
    stop();
  }
  // Set up MultiSynth for register reg=MSNA, MNSB, MS0-5 with fractional divider, num and denom and R divider (for MSn, not for MSNA, MSNB)
  // divider is 15..90 for MSNA, MSNB,  divider is 8..900 (and in addition 4,6 for integer mode) for MS[0-5]
  // num is 0..1,048,575 (0xFFFFF)
  // denom is 0..1,048,575 (0xFFFFF)
  // num = 0, denom = 1 forces an integer value for the divider
  // r_div = 1..128 (1,2,4,8,16,32,64,128)
  void SetupMultisynth(uint8_t reg, uint8_t divider, uint32_t num, uint32_t denom, uint8_t r_div)
  {
    uint32_t P1; // Synth config register P1
    uint32_t P2; // Synth config register P2
    uint32_t P3; // Synth config register P3
  
    P1 = (uint32_t)(128 * ((float)num / (float)denom));
    P1 = (uint32_t)(128 * (uint32_t)(divider) + P1 - 512);
    P2 = (uint32_t)(128 * ((float)num / (float)denom));
    P2 = (uint32_t)(128 * num - denom * P2);
    P3 = denom;
  
    SendRegister(reg + 0, (P3 & 0x0000FF00) >> 8);
    SendRegister(reg + 1, (P3 & 0x000000FF));
    SendRegister(reg + 2, (P1 & 0x00030000) >> 16 | ((int)log2(r_div) << 4) );
    SendRegister(reg + 3, (P1 & 0x0000FF00) >> 8);
    SendRegister(reg + 4, (P1 & 0x000000FF));
    SendRegister(reg + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
    SendRegister(reg + 6, (P2 & 0x0000FF00) >> 8);
    SendRegister(reg + 7, (P2 & 0x000000FF));
  }
  inline void SendPLLBRegisterBulk()  // fast freq change of PLLB, takes about [ 2 + 7*(8+1) + 2 ] / 840000 = 80 uS
  {
    start();
    SendByte(SI_I2C_ADDR << 1);
    SendByte(SI_SYNTH_PLL_B + 3);  // Skip the first three pll_regs bytes (first two always 0xFF and third not often changing
    SendByte(pll_regs[3]);
    SendByte(pll_regs[4]);
    SendByte(pll_regs[5]);
    SendByte(pll_regs[6]);
    SendByte(pll_regs[7]);
    stop();
  }
  
  #define FAST __attribute__((optimize("Ofast")))

  // this function relies on cached (global) variables: divider, mult, raw_freq, pll_regs
  inline void FAST freq_calc_fast(int16_t freq_offset)
  { // freq_offset is relative to freq set in freq(freq)
    // uint32_t num128 = ((divider * (raw_freq + offset)) % fxtal) * (float)(0xFFFFF * 128) / fxtal;
    // Above definition (for fxtal=27.00491M) can be optimized by pre-calculating factor (0xFFFFF*128)/fxtal (=4.97) as integer constant (5) and
    // substracting the rest error factor (0.03). Note that the latter is shifted left (0.03<<6)=2, while the other term is shifted right (>>6)
    register int32_t z = ((divider * (raw_freq + freq_offset)) % fxtal);
    register int32_t z2 = -(z >> 5);
    int32_t num128 = (z * 5) + z2;
  
    // Set up specified PLL with mult, num and denom: mult is 15..90, num128 is 0..128*1,048,575 (128*0xFFFFF), denom is 0..1,048,575 (0xFFFFF)
    uint32_t P1 = 128 * mult + (num128 / 0xFFFFF) - 512;
    uint32_t P2 = num128 % 0xFFFFF;
    //pll_regs[0] = 0xFF;
    //pll_regs[1] = 0xFF;
    //pll_regs[2] = (P1 >> 14) & 0x0C;
    pll_regs[3] = P1 >> 8;
    pll_regs[4] = P1;
    pll_regs[5] = 0xF0 | (P2 >> 16);
    pll_regs[6] = P2 >> 8;
    pll_regs[7] = P2;
  }
  uint16_t div(uint32_t num, uint32_t denom, uint32_t* b, uint32_t* c)
  { // returns a + b / c = num / denom, where a is the integer part and b and c is the optional fractional part 20 bits each (range 0..1048575)
    uint16_t a = num / denom;
    if(b && c){
      uint64_t l = num % denom;
      l <<= 20; l--;  // l *= 1048575;
      l /= denom;     // normalize
      *b = l;
      *c = 0xFFFFF;    // for simplicity set c to the maximum 1048575
    }
    return a;
  }
  void freq(uint32_t freq, uint8_t i, uint8_t q)
  { // Fout = Fvco / (R * [MSx_a + MSx_b/MSx_c]),  Fvco = Fxtal * [MSPLLx_a + MSPLLx_b/MSPLLx_c]; MSx as integer reduce spur
    //uint8_t r_div = (freq > (SI_PLL_FREQ/256/1)) ? 1 : (freq > (SI_PLL_FREQ/256/32)) ? 32 : 128; // helps divider to be in range
    uint8_t r_div = (freq < 500000) ? 128 : 1;
    freq *= r_div;  // take r_div into account, now freq is in the range 1MHz to 150MHz
    raw_freq = freq;   // cache frequency generated by PLL and MS stages (excluding R divider stage); used by freq_calc_fast()
  
    divider = SI_PLL_FREQ / freq;  // Calculate the division ratio. 900,000,000 is the maximum internal PLL freq (official range 600..900MHz but can be pushed to 300MHz..~1200Mhz)
    if(divider % 2) divider--;  // divider in range 8.. 900 (including 4,6 for integer mode), even numbers preferred. Note that uint8 datatype is used, so 254 is upper limit
    if( (divider * (freq - 5000) / fxtal) != (divider * (freq + 5000) / fxtal) ) divider -= 2; // Test if multiplier remains same for freq deviation +/- 5kHz, if not use different divider to make same
    pll_freq = divider * freq; // Calculate the pll_freq: the divider * desired output freq
    uint32_t num, denom;
    mult = div(pll_freq, fxtal, &num, &denom); // Determine the mult to get to the required pll_freq (in the range 15..90)
  
    // Set up specified PLL with mult, num and denom: mult is 15..90, num is 0..1,048,575 (0xFFFFF), denom is 0..1,048,575 (0xFFFFF)
    // Set up PLL A and PLL B with the calculated  multiplication ratio
    SetupMultisynth(SI_SYNTH_PLL_A, mult, num, denom, 1);
    SetupMultisynth(SI_SYNTH_PLL_B, mult, num, denom, 1);
    //if(denom == 1) SendRegister(22, SI_MSx_INT); // FBA_INT: MSNA operates in integer mode
    //if(denom == 1) SendRegister(23, SI_MSx_INT); // FBB_INT: MSNB operates in integer mode
    // Set up MultiSynth 0,1,2 with the calculated divider, from 4, 6..1800.
    // The final R division stage can divide by a power of two, from 1..128
    // if you want to output frequencies below 1MHz, you have to use the final R division stage
    SetupMultisynth(SI_SYNTH_MS_0, divider, 0, 1, r_div);
    SetupMultisynth(SI_SYNTH_MS_1, divider, 0, 1, r_div);
    SetupMultisynth(SI_SYNTH_MS_2, divider, 0, 1, r_div);
    //if(prev_divider != divider){ lcd.setCursor(0, 0); lcd.print(divider); lcd_blanks();
    // Set I/Q phase
    SendRegister(SI_CLK0_PHOFF, i * divider / 90); // one LSB equivalent to a time delay of Tvco/4 range 0..127
    SendRegister(SI_CLK1_PHOFF, q * divider / 90); // one LSB equivalent to a time delay of Tvco/4 range 0..127
    // Switch on the CLK0, CLK1 output to be PLL A and set multiSynth0, multiSynth1 input (0x0F = SI_CLK_SRC_MS | SI_CLK_IDRV_8MA)
    SendRegister(SI_CLK0_CONTROL, 0x0F | SI_MS_INT | SI_CLK_SRC_PLL_A);
    SendRegister(SI_CLK1_CONTROL, 0x0F | SI_MS_INT | SI_CLK_SRC_PLL_A);
    // Switch on the CLK2 output to be PLL B and set multiSynth2 input
    SendRegister(SI_CLK2_CONTROL, 0x0F | SI_MS_INT | SI_CLK_SRC_PLL_B);
    SendRegister(SI_CLK_OE, 0b11111100); // Enable CLK1|CLK0
    // Reset the PLL. This causes a glitch in the output. For small changes to
    // the parameters, you don't need to reset the PLL, and there is no glitch
    if((abs(pll_freq - iqmsa) > 16000000L) || divider != prev_divider){
      iqmsa = pll_freq;
      prev_divider = divider;
      SendRegister(SI_PLL_RESET, 0xA0);
    }
    //SendRegister(24, 0b00000000); // CLK3-0 Disable State: CLK2=0 (BE CAREFUL TO CHANGE THIS!!!), CLK1/0=00 -> IC4-X0 selected -> 2,5V on IC5A/3(+), when IC5/2(-) leaks down below 2.5V -> 12V on IC5A/1, IC6A/2(-) -> 0V on IC6A/1, AUDIO2
  }
  void alt_clk2(uint32_t freq)
  {
    uint32_t num, denom;
    uint16_t mult = div(pll_freq, freq, &num, &denom);
  
    SetupMultisynth(SI_SYNTH_MS_2, mult, num, denom, 1);
  
    // Switch on the CLK2 output to be PLL A and set multiSynth2 input
    SendRegister(SI_CLK2_CONTROL, 0x0F | SI_CLK_SRC_PLL_A);
  
    SendRegister(SI_CLK_OE, 0b11111000); // Enable CLK2|CLK1|CLK0
  
    //SendRegister(SI_CLK0_PHOFF, 0 * divider / 90); // one LSB equivalent to a time delay of Tvco/4 range 0..127
    //SendRegister(SI_CLK1_PHOFF, 90 * divider / 90); // one LSB equivalent to a time delay of Tvco/4 range 0..127
    //SendRegister(SI_CLK2_PHOFF, 45 * divider / 90); // one LSB equivalent to a time delay of Tvco/4 range 0..127
    //SendRegister(SI_PLL_RESET, 0xA0);
  }
  void powerDown()
  {
    SendRegister(SI_CLK0_CONTROL, 0b11000000);  // Conserve power when output is disabled
    SendRegister(SI_CLK1_CONTROL, 0b11000000);
    SendRegister(SI_CLK2_CONTROL, 0b11000000);
    SendRegister(19, 0b11000000);
    SendRegister(20, 0b11000000);
    SendRegister(21, 0b11000000);
    SendRegister(22, 0b11000000);
    SendRegister(23, 0b11000000);
    SendRegister(SI_CLK_OE, 0b11111111); // Disable all CLK outputs
  }
};
static SI5351 si5351;
 */
