#ifndef STRING_H
#define STRING_H

#include <stdint.h>

void *memset(void *dest, int c, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int n);
unsigned int strlen(const char *s);

#endif /* STRING_H */
