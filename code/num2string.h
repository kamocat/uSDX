char * memcpy2(char * dest, char * src, unsigned int n);

static const unsigned int max_strlen = 128;    // to prevent infinite loop
unsigned int strlen2(char * src);

short format_int(char * dest, long val);

short format_hex(char * dest, long val);
