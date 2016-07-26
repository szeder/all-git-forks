#include <stdio.h>

const char * sha_to_phrase(const unsigned char *sha1, int len);
const unsigned char * phrase_to_sha(const char *phrase, int * shaLen);
