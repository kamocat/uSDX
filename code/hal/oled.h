#include "fonts.h"
class SSD1306Device: public Print {
public:
  #define SSD1306_ADDR 0x3C  // Slave address  
  #define SSD1306_PAGES 4
  #define SSD1306_COMMAND 0x00
  #define SSD1306_DATA 0x40
  uint8_t oledX = 0, oledY = 0;
  uint8_t renderingFrame = 0xB0;
  bool wrap = false;

  void begin(uint8_t cols, uint8_t rows, uint8_t charsize = 0){
    Wire.begin();
    Wire.beginTransmission(SSD1306_ADDR); Wire.write(SSD1306_COMMAND);
    for (uint8_t i = 0; i < sizeof(ssd1306_init_sequence); i++) {
      Wire.write(pgm_read_byte(&ssd1306_init_sequence[i]));
    }
    Wire.endTransmission();
    delayMicroseconds(100);
  }
  void noCursor(){}
  void cursor(){}
  void noDisplay(){}
  void createChar(uint8_t l, uint8_t glyph[]){}

  void _setCursor(uint8_t x, uint8_t y) { oledX = x; oledY = y;
    Wire.beginTransmission(SSD1306_ADDR); Wire.write(SSD1306_COMMAND);
    Wire.write(renderingFrame | (oledY & 0x07));
    Wire.write(0x10 | ((oledX & 0xf0) >> 4));
    Wire.write(oledX & 0x0f);
    Wire.endTransmission();
  }
  void setCursor(uint8_t x, uint8_t y) { _setCursor(x * FONT_W, y * FONT_H); }
    
  void newLine() {
    oledY+=FONT_H;
    if (oledY > SSD1306_PAGES - FONT_H) {
      oledY = SSD1306_PAGES - FONT_H;
    }
    setCursor(0, oledY);
  }
    
  size_t write(byte c) {
    if((c == '\n') || (oledX > ((uint8_t)128 - FONT_W))) {
      if(wrap)  newLine();
      return 1;
    }
    
    uint16_t offset = ((uint16_t)c - ' ') * FONT_W/(FONT_STRETCHH+1) * FONT_H;
    uint8_t line = FONT_H;
    do
    {
      if(FONT_STRETCHV) offset = ((uint16_t)c - ' ') * FONT_W/(FONT_STRETCHH+1) * FONT_H/(2*FONT_STRETCHV);
      Wire.beginTransmission(SSD1306_ADDR); Wire.write(SSD1306_DATA);
      for (uint8_t i = 0; i < (FONT_W/(FONT_STRETCHH+1)); i++) {
        uint8_t b = pgm_read_byte(&(font[offset++]));
        if(FONT_STRETCHV){
          uint8_t b2 = 0;
          if(line > 1) for(int i = 0; i!=4; i++) b2 |=/* ! */(b & (1<<i)) ? (1<<(i*2)) | (1<<(i*2)+1): 0x00;
          else         for(int i = 0; i!=4; i++) b2 |=/* ! */(b & (1<<(i+4))) ? (1<<(i*2)) | (1<<(i*2)+1): 0x00;
          Wire.write(b2);
          if(FONT_STRETCHH) Wire.write(b2);
        } else { Wire.write(b); if(FONT_STRETCHH) Wire.write(b); }
      }
      Wire.endTransmission();
      if (FONT_H == 1) {
        oledX+=FONT_W;
      }
      else {
        if (line > 1) {
          _setCursor(oledX, oledY + 1);
        }
        else {
          _setCursor(oledX + FONT_W, oledY - (FONT_H - 1));
        }
      }
    }
    while (--line);
    return 1;
  }

  void bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t bitmap[]) {
    uint16_t j = 0;
    for (uint8_t y = y0; y < y1; y++) {
      _setCursor(x0, y);
      Wire.beginTransmission(SSD1306_ADDR); Wire.write(SSD1306_DATA);
      for (uint8_t x = x0; x < x1; x++) {
        Wire.write(pgm_read_byte(&bitmap[j++]));
      }
      Wire.endTransmission();
    }
    setCursor(0, 0);
  }
};
//#define OLED  1   // SDD1306 connection on display header: 1=GND(black), 2=5V(red), 13=SDA(brown), 14=SCK(orange)
#ifdef OLED
SSD1306Device lcd;
#else
LCD lcd;     // highly-optimized LCD driver, OK for QCX supplied displays
//LCD_ lcd;  // slower LCD, suitable for non-QCX supplied displays
//#include <LiquidCrystal.h>
//LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
#endif
