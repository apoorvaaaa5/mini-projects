#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

typedef uint32_t size_t;

#define HEAP_SIZE_BYTES (16 * 1024) /* 16 KB Dynamic Heap Pool */

void   heap_init(void);
void  *malloc(size_t size);
void   free(void *ptr);
size_t heap_get_free_bytes(void);
size_t heap_get_total_bytes(void);

#endif /* HEAP_H */
