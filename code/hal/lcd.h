#ifndef LCD_HEADER
#define LCD_HEADER
class LCD : public Print {  // inspired by: http://www.technoblogy.com/show?2BET
public:  // LCD1602 display in 4-bit mode, RS is pull-up and kept low when idle to prevent potential display RFI via RS line
  #define _dn  0      // PD0 to PD3 connect to D4 to D7 on the display
  #define _en  4      // PC4 - MUST have pull-up resistor
  #define _rs  4      // PC4 - MUST have pull-up resistor
  //#define LCD_RS_HI() DDRC &= ~(1 << _rs);         // RS high (pull-up)
  //#define LCD_RS_LO() DDRC |= 1 << _rs;            // RS low (pull-down)
  #define LCD_RS_LO() PORTC &= ~(1 << _rs);        // RS low
  #define LCD_RS_HI() PORTC |= (1 << _rs);         // RS high
  #define LCD_EN_LO() PORTD &= ~(1 << _en);        // EN low
  #define LCD_EN_HI() PORTD |= (1 << _en);         // EN high
  #define LCD_PREP_NIBBLE(b) (PORTD & ~(0xf << _dn)) | (b) << _dn | 1 << _en // Send data and enable high
  void begin(uint8_t x = 0, uint8_t y = 0){        // Send command , make sure at least 40ms after power-up before sending commands
    bool reinit = (x == 0) && (y == 0);
    DDRD |= 0xf << _dn | 1 << _en;                 // Make data, EN outputs
    DDRC |= 1 << _rs;
    //PORTC &= ~(1 << _rs);                          // Set RS low in case to support pull-down when DDRC is output
    delayMicroseconds(50000);                      // *
    LCD_RS_LO(); LCD_EN_LO();
    cmd(0x33);                                     // Ensures display is in 8-bit mode
    delayMicroseconds(4500); cmd(0x33); delayMicroseconds(4500); cmd(0x33); delayMicroseconds(150); // * Ensures display is in 8-bit mode
    cmd(0x32);                                     // Puts display in 4-bit mode
    cmd(0x28);                                     // * Function set: 2-line, 5x8 
    cmd(0x0c);                                     // Display on
    if(reinit) return;
    cmd(0x01);                                     // Clear display
    delay(3);                                      // Allow to execute Clear on display [https://www.sparkfun.com/datasheets/LCD/HD44780.pdf, p.49, p58]
    cmd(0x06);                                     // * Entrymode: left, shift-dec
  }
  void nib(uint8_t b){                             // Send four bit nibble to display
    PORTD = LCD_PREP_NIBBLE(b);                    // Send data and enable high
    //asm("nop");                                    // Enable high pulse width must be at least 230ns high, data-setup time 80ns
    delayMicroseconds(4);
    LCD_EN_LO();
    //delayMicroseconds(52);                         // Execution time
    delayMicroseconds(60);                         // Execution time
  }
  void cmd(uint8_t b){ nib(b >> 4); nib(b & 0xf); }// Write command: send nibbles while RS low
  size_t write(uint8_t b){                         // Write data:    send nibbles while RS high
    //LCD_EN_HI();                                   // Complete Enable cycle must be at least 500ns (so start early)
    uint8_t nibh = LCD_PREP_NIBBLE(b >>  4);       // Prepare high nibble data and enable high
    PORTD = nibh;                                  // Send high nibble data and enable high
    uint8_t nibl = LCD_PREP_NIBBLE(b & 0xf);       // Prepare low nibble data and enable high
    //asm("nop");                                    // Enable high pulse width must be at least 230ns high, data-setup time 80ns; ATMEGA clock-cycle is 50ns (so at least 5 cycles)
    LCD_RS_HI();
    LCD_EN_LO();
    PORTD = nibl;                                  // Send low nibble data and enable high
    LCD_RS_LO();
    //asm("nop"); asm("nop");                        // Complete Enable cycle must be at least 500ns
    //PORTD = nibl;                                  // Send low nibble data and enable high
    //asm("nop");                                    // Enable high pulse width must be at least 230ns high, data-setup time 80ns; ATMEGA clock-cycle is 50ns (so at least 5 cycles)
    LCD_RS_HI();
    LCD_EN_LO();
    LCD_RS_LO();
    delayMicroseconds(60);                         // Execution time  (37+4)*1.25 us
    PORTD |= 0x02;                                 // To support serial-interface keep LCD_D5 high, so that DVM is not pulled-down via D
    return 1;
  }
  void setCursor(uint8_t x, uint8_t y){ cmd(0x80 | (x + y * 0x40)); }
  void cursor(){ cmd(0x0e); }
  void noCursor(){ cmd(0x0c); }
  void noDisplay(){ cmd(0x08); }
  void createChar(uint8_t l, uint8_t glyph[]){ cmd(0x40 | ((l & 0x7) << 3)); for(int i = 0; i != 8; i++) write(glyph[i]); }
};

/*
class LCD : public Print {  // inspired by: http://www.technoblogy.com/show?2BET
public:  // LCD1602 display in 4-bit mode, RS is pull-up and kept low when idle to prevent potential display RFI via RS line
  #define _dn  0      // PD0 to PD3 connect to D4 to D7 on the display
  #define _en  4      // PC4 - MUST have pull-up resistor
  #define _rs  4      // PC4 - MUST have pull-up resistor
  #define LCD_RS_HI() DDRC &= ~(1 << _rs);         // RS high (pull-up)
  #define LCD_RS_LO() DDRC |= 1 << _rs;            // RS low (pull-down)
  #define LCD_EN_LO() PORTD &= ~(1 << _en);        // EN low
  #define LCD_PREP_NIBBLE(b) (PORTD & ~(0xf << _dn)) | (b) << _dn | 1 << _en // Send data and enable high
  void begin(uint8_t x, uint8_t y){                // Send command
    DDRD |= 0xf << _dn | 1 << _en;                 // Make data, EN and RS pins outputs
    PORTC &= ~(1 << _rs);                          // Set RS low in case to support pull-down when DDRC is output
    delayMicroseconds(50000);                      // * At least 40ms after power rises above 2.7V before sending commands
    LCD_RS_LO();
    cmd(0x33);                                     // Ensures display is in 8-bit mode
    cmd(0x32);                                     // Puts display in 4-bit mode
    cmd(0x0e);                                     // Display and cursor on
    cmd(0x01);                                     // Clear display
    delay(3);                                      // Allow to execute on display [https://www.sparkfun.com/datasheets/LCD/HD44780.pdf, p.49, p58]
  }
  void nib(uint8_t b){                             // Send four bit nibble to display
    PORTD = LCD_PREP_NIBBLE(b);                    // Send data and enable high
    delayMicroseconds(4);
    LCD_EN_LO();
    delayMicroseconds(60);                         // Execution time  (was: 37)
  }
  void cmd(uint8_t b){ nib(b >> 4); nib(b & 0xf); }// Write command: send nibbles while RS low
  size_t write(uint8_t b){                         // Write data:    send nibbles while RS high
    uint8_t nibh = LCD_PREP_NIBBLE(b >>  4);       // Prepare high nibble data and enable high
    uint8_t nibl = LCD_PREP_NIBBLE(b & 0xf);       // Prepare low nibble data and enable high
    PORTD = nibh;                                  // Send high nibble data and enable high
    LCD_RS_HI();
    LCD_EN_LO();
    PORTD = nibl;                                  // Send low nibble data and enable high
    LCD_RS_LO();
    LCD_RS_HI();
    LCD_EN_LO();
    LCD_RS_LO();
    delayMicroseconds(41);                         // Execution time
    PORTD |= 0x02;                                 // To support serial-interface keep LCD_D5 high, so that DVM is not pulled-down via D
    return 1;
  }
  void setCursor(uint8_t x, uint8_t y){ cmd(0x80 | (x + y * 0x40)); }
  void cursor(){ cmd(0x0e); }
  void noCursor(){ cmd(0x0c); }
  void noDisplay(){ cmd(0x08); }
  void createChar(uint8_t l, uint8_t glyph[]){ cmd(0x40 | ((l & 0x7) << 3)); for(int i = 0; i != 8; i++) write(glyph[i]); }
};
*/

#include <LiquidCrystal.h>
class LCD_ : public LiquidCrystal {
public: // QCXLiquidCrystal extends LiquidCrystal library for pull-up driven LCD_RS, as done on QCX. LCD_RS needs to be set to LOW in advance of calling any operation.
  //LCD_(uint8_t rs = LCD_RS, uint8_t en = LCD_EN, uint8_t d4 = LCD_D4, uint8_t d5, = LCD_D5 uint8_t d6 = LCD_D6, uint8_t d7 = LCD_D7) : LiquidCrystal(rs, en, d4, d5, d6, d7){ };
  LCD_() : LiquidCrystal(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7){ };
  virtual size_t write(uint8_t value){ // overwrites LiquidCrystal::write() and re-implements LCD data writes
    pinMode(LCD_RS, INPUT);  // pull-up LCD_RS
    write4bits(value >> 4);
    write4bits(value);
    pinMode(LCD_RS, OUTPUT); // pull-down LCD_RS
    return 1;
  };
  void write4bits(uint8_t value){
    digitalWrite(LCD_D4, (value >> 0) & 0x01);
    digitalWrite(LCD_D5, (value >> 1) & 0x01);
    digitalWrite(LCD_D6, (value >> 2) & 0x01);
    digitalWrite(LCD_D7, (value >> 3) & 0x01);
    digitalWrite(LCD_EN, LOW);  // pulseEnable
    delayMicroseconds(1);
    digitalWrite(LCD_EN, HIGH);
    delayMicroseconds(1);    // enable pulse must be >450ns
    digitalWrite(LCD_EN, LOW);
    delayMicroseconds(100);   // commands need > 37us to settle
  };
};

#define BRIGHT  1
static const uint8_t ssd1306_init_sequence [] PROGMEM = {  // Initialization Sequence
//  0xAE,     // Display OFF (sleep mode)
    0x20, 0b10,   // Set Memory Addressing Mode
          // 00=Horizontal Addressing Mode; 01=Vertical Addressing Mode;
          // 10=Page Addressing Mode (RESET); 11=Invalid
 0xB0,     // Set Page Start Address for Page Addressing Mode, 0-7
  0xC8,     // Set COM Output Scan Direction.  Flip Veritically.
 0x00,     // Set low nibble of column address
 0x10,     // Set high nibble of column address
   0x40,     // Set display start line address
#ifdef BRIGHT
  0x81, /*32*/ 0x7F,   // Set contrast control register
#else
  0x81, 32,   // Set contrast control register
#endif
  0xA1,     // Set Segment Re-map. A0=column 0 mapped to SEG0; A1=column 127 mapped to SEG0. Flip Horizontally
   0xA6,     // Set display mode. A6=Normal; A7=Inverse
  0xA8, 0x1F,   // Set multiplex ratio(1 to 64)
   0xA4,     // Output RAM to Display
          // 0xA4=Output follows RAM content; 0xA5,Output ignores RAM content
  0xD3, 0x00,   // Set display offset. 00 = no offset
   0xD5, 0x80,   // --set display clock divide ratio/oscillator frequency
#ifdef BRIGHT
  0xD9, 0xF1, // 0xF1=brighter //0x22,   // Set pre-charge period
#else
  0xD9, 0x22,   // Set pre-charge period
#endif
  0xDA, 0x02,   // Set com pins hardware configuration
//   0xDB, 0x40, //0x20,   // --set vcomh 0x20 = 0.77xVcc
  0x8D, 0x14,    // Set DC-DC enable
  0xAF,     // Display ON
};

#endif
