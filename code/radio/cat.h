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
