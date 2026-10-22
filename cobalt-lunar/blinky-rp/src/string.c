#include <string.h>

void *memset(void *srcptr, int n, unsigned int size)
{
	char *temp = (char *)srcptr;
	while (size--)
		*temp++ = (char)n;

	return srcptr;
}

void *memcpy(void *dest, const void *src, unsigned int size)
{
	char *d = (char *)dest;
	const char *s = (const char *)src;
	while (size--)
		*d++ = *s++;

	return dest;
}
