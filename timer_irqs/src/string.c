#include <string.h>

void *memset(void *srcptr, int n, unsigned int size)
{
	char *temp = srcptr;
	while(size--)
		*temp++ = n;

	return srcptr;
}
