#include <string.h>

void *memset(void *dest, int c, unsigned int n)
{
	unsigned char *p = (unsigned char *)dest;
	while (n--) {
		*p++ = (unsigned char)c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, unsigned int n)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;
	while (n--) {
		*d++ = *s++;
	}
	return dest;
}

unsigned int strlen(const char *s)
{
	unsigned int len = 0;
	while (*s++) {
		len++;
	}
	return len;
}
