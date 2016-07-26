#include <stdio.h>

const char * sha_to_phrase(const unsigned char *sha1, int len);
int phrase_to_prefix(const char *name, int * len, unsigned char *bin_pfx, char *hex_pfx);
