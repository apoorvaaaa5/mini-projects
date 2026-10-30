#ifndef STRING_H
#define STRING_H

#include <stdint.h>

void *memset(void *dest, int c, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int n);
unsigned int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, unsigned int n);
char *strncpy(char *dest, const char *src, unsigned int n);
int atoi(const char *s);

#endif /* STRING_H */
