#include <heap.h>
#include <arch.h>
#include <stdint.h>

/*
 * Block header for first-fit dynamic heap manager.
 * Exactly 16 bytes (multiple of 8) to ensure 8-byte payload alignment
 * required by ARM Cortex-M0+ stack frames (AAPCS compliance).
 */
typedef struct block_header {
	size_t size;                /* Total block size including this 16-byte header */
	uint32_t is_free;           /* 1 = Free block, 0 = In use */
	struct block_header *next;  /* Next physical block in memory order */
	uint32_t padding;           /* Guarantees 8-byte payload alignment */
} block_header_t;

/* Dedicated 16 KB heap pool aligned to 8-byte boundary */
static uint8_t heap_pool[HEAP_SIZE_BYTES] __attribute__((aligned(8)));
static block_header_t *heap_start = (block_header_t *)0;

void heap_init(void)
{
	heap_start = (block_header_t *)heap_pool;
	heap_start->size = HEAP_SIZE_BYTES;
	heap_start->is_free = 1;
	heap_start->next = (block_header_t *)0;
	heap_start->padding = 0;
}

void *malloc(size_t size)
{
	if (size == 0) return (void *)0;

	/* 1. Payload must be aligned to 8 bytes for Cortex-M0+ stack */
	size_t aligned_payload = (size + 7) & ~7UL;
	size_t total_needed = sizeof(block_header_t) + aligned_payload;

	arch_di();

	if (!heap_start) {
		heap_init();
	}

	block_header_t *curr = heap_start;
	while (curr) {
		if (curr->is_free && curr->size >= total_needed) {
			/* Check if block is large enough to split */
			size_t excess = curr->size - total_needed;
			if (excess >= (sizeof(block_header_t) + 8)) {
				block_header_t *split = (block_header_t *)((uint8_t *)curr + total_needed);
				split->size = excess;
				split->is_free = 1;
				split->next = curr->next;
				split->padding = 0;

				curr->size = total_needed;
				curr->next = split;
			}

			curr->is_free = 0;
			arch_ei();
			return (void *)((uint8_t *)curr + sizeof(block_header_t));
		}
		curr = curr->next;
	}

	arch_ei();
	return (void *)0; /* Out of memory */
}

void free(void *ptr)
{
	if (!ptr) return;

	arch_di();

	block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
	block->is_free = 1;

	/* Coalesce adjacent free blocks to prevent fragmentation */
	block_header_t *curr = heap_start;
	while (curr && curr->next) {
		if (curr->is_free && curr->next->is_free &&
		    ((uint8_t *)curr + curr->size == (uint8_t *)curr->next)) {
			curr->size += curr->next->size;
			curr->next = curr->next->next;
		} else {
			curr = curr->next;
		}
	}

	arch_ei();
}

size_t heap_get_free_bytes(void)
{
	size_t free_bytes = 0;
	arch_di();

	if (!heap_start) {
		heap_init();
	}

	block_header_t *curr = heap_start;
	while (curr) {
		if (curr->is_free) {
			if (curr->size > sizeof(block_header_t)) {
				free_bytes += (curr->size - sizeof(block_header_t));
			}
		}
		curr = curr->next;
	}

	arch_ei();
	return free_bytes;
}

size_t heap_get_total_bytes(void)
{
	return HEAP_SIZE_BYTES;
}
