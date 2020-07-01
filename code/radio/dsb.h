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
