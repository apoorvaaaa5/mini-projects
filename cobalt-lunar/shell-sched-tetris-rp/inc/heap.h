#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
	size_t total_bytes;
	size_t free_bytes;
	size_t used_bytes;
	size_t total_blocks;
	size_t free_blocks;
	size_t used_blocks;
	size_t largest_free_block;
} heap_stats_t;

void   heap_init(void);
void  *malloc(size_t size);
void   free(void *ptr);
size_t heap_get_free_bytes(void);
size_t heap_get_total_bytes(void);
void   heap_get_stats(heap_stats_t *stats);
void   heap_audit(void);

#endif /* HEAP_H */
