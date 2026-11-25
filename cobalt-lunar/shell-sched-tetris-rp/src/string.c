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

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, unsigned int n)
{
	while (n && *s1 && (*s1 == *s2)) {
		s1++;
		s2++;
		n--;
	}
	if (n == 0) return 0;
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strncpy(char *dest, const char *src, unsigned int n)
{
	char *d = dest;
	while (n && *src) {
		*d++ = *src++;
		n--;
	}
	while (n--) {
		*d++ = '\0';
	}
	return dest;
}

int atoi(const char *s)
{
	int res = 0;
	while (*s == ' ' || *s == '\t') s++;
	while (*s >= '0' && *s <= '9') {
		res = res * 10 + (*s - '0');
		s++;
	}
	return res;
}
