#ifndef STDDEF_H
#define STDDEF_H

#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef uint32_t size_t;
typedef int32_t  ptrdiff_t;

#endif /* STDDEF_H */
