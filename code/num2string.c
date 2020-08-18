#include "num2string.h"

char * memcpy2(char * dest, char * src, unsigned int n){
  while(n--){
    *dest++ = *src++;
  }
  return dest;
}

unsigned int strlen2(char * str){
  unsigned int i = 0;
  while(*str++ && (i<max_strlen)){
    ++i;
  }
  return i;
}

short format_int(char * dest, long val){
  if(!val){
    *dest++ = '0';
    return 1;
  } else if(val < 0){
    *dest++ = '-';
    val = -val;
  }
  char * tmp = dest + 10;
  short n = 0;
  while(val){
    ++n;
    *(--tmp) = '0' + val % 10;
    val = val/10;
  }
  if(tmp != dest){
    memcpy2(dest, tmp, n);
  }
  return n;
}

short format_hex(char * dest, long val){
  *dest++='0';
  *dest++='x';
  char * lookup = "0123456789ABCDEF";
  short i = 32;
  do{
    i -= 4;
    *dest++ = lookup[(val>>i)&0xF];
  }while(i);
  return 2+8; //2 character prefix, 8 hex nibbles
}
