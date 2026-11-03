#include <heap.h>
#include <arch.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <console.h>

/* Linker symbols defining the 16 KB heap carved out in link.ld */
extern uint8_t __heap_start;
extern uint8_t __heap_end;

/*
 * Block header for first-fit dynamic heap manager.
 * Exactly 16 bytes (multiple of 8) to ensure 8-byte payload alignment
 * required by ARM Cortex-M0+ stack frames (AAPCS compliance).
 */
typedef struct block_header {
	size_t size;                /* Total block size including this 16-byte header */
	bool is_free;               /* true = Free block, false = In use */
	uint8_t padding[3];         /* 3-byte padding to align pointer field */
	struct block_header *next;  /* Next physical block in memory order */
	uint32_t align_pad;         /* Padded to exactly 16 bytes (8-byte boundary) */
} block_header_t;

static block_header_t *heap_start = NULL;

void heap_init(void)
{
	heap_start = (block_header_t *)&__heap_start;
	heap_start->size = (size_t)(&__heap_end - &__heap_start);
	heap_start->is_free = true;
	heap_start->next = NULL;
	heap_start->padding[0] = 0;
	heap_start->padding[1] = 0;
	heap_start->padding[2] = 0;
	heap_start->align_pad = 0;
}

void *malloc(size_t size)
{
	if (size == 0) return NULL;

	/* 1. Payload must be aligned to 8 bytes for Cortex-M0+ stack */
	size_t aligned_payload = (size + 7) & ~7UL;
	size_t total_needed = sizeof(block_header_t) + aligned_payload;

	arch_di();

	if (heap_start == NULL) {
		heap_init();
	}

	block_header_t *curr = heap_start;
	while (curr != NULL) {
		if (curr->is_free && curr->size >= total_needed) {
			/* Check if block is large enough to split */
			size_t excess = curr->size - total_needed;
			if (excess >= (sizeof(block_header_t) + 8)) {
				block_header_t *split = (block_header_t *)((uint8_t *)curr + total_needed);
				split->size = excess;
				split->is_free = true;
				split->next = curr->next;
				split->padding[0] = 0;
				split->padding[1] = 0;
				split->padding[2] = 0;
				split->align_pad = 0;

				curr->size = total_needed;
				curr->next = split;
			}

			curr->is_free = false;
			arch_ei();
			return (void *)((uint8_t *)curr + sizeof(block_header_t));
		}
		curr = curr->next;
	}

	arch_ei();
	return NULL; /* Out of memory */
}

void free(void *ptr)
{
	if (ptr == NULL) return;

	arch_di();

	block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
	block->is_free = true;

	/* Coalesce adjacent free blocks to prevent fragmentation */
	block_header_t *curr = heap_start;
	while (curr != NULL && curr->next != NULL) {
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

	if (heap_start == NULL) {
		heap_init();
	}

	block_header_t *curr = heap_start;
	while (curr != NULL) {
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
	return (size_t)(&__heap_end - &__heap_start);
}

void heap_get_stats(heap_stats_t *stats)
{
	if (!stats) return;

	arch_di();
	if (heap_start == NULL) heap_init();

	stats->total_bytes = (size_t)(&__heap_end - &__heap_start);
	stats->free_bytes = 0;
	stats->used_bytes = 0;
	stats->total_blocks = 0;
	stats->free_blocks = 0;
	stats->used_blocks = 0;
	stats->largest_free_block = 0;

	block_header_t *curr = heap_start;
	while (curr != NULL) {
		stats->total_blocks++;
		if (curr->is_free) {
			stats->free_blocks++;
			size_t payload = (curr->size > sizeof(block_header_t)) ? (curr->size - sizeof(block_header_t)) : 0;
			stats->free_bytes += payload;
			if (payload > stats->largest_free_block) {
				stats->largest_free_block = payload;
			}
		} else {
			stats->used_blocks++;
			size_t payload = (curr->size > sizeof(block_header_t)) ? (curr->size - sizeof(block_header_t)) : 0;
			stats->used_bytes += payload;
		}
		curr = curr->next;
	}

	arch_ei();
}

void heap_audit(void)
{
	arch_di();
	if (heap_start == NULL) heap_init();

	console_puts("\r\n============================ DYNAMIC HEAP AUDIT DUMP ============================\r\n");
	console_puts("BLK  HDR ADDR    PAYLOAD ADDR  TOTAL SZ  PAYLOAD   STATUS     NEXT BLOCK\r\n");
	console_puts("--------------------------------------------------------------------------------\r\n");

	block_header_t *curr = heap_start;
	int blk_idx = 0;
	bool corrupted = false;

	while (curr != NULL) {
		console_print_dec(blk_idx++);
		if (blk_idx <= 10) console_puts("    ");
		else console_puts("   ");

		console_print_hex((uint32_t)curr);
		console_puts("  ");

		uint8_t *payload = (uint8_t *)curr + sizeof(block_header_t);
		console_print_hex((uint32_t)payload);
		console_puts("  ");

		console_print_dec(curr->size);
		console_puts(" B");
		int pad = 8;
		uint32_t tmp = curr->size;
		while (tmp > 0) { pad--; tmp /= 10; }
		while (pad-- > 0) console_putc(' ');

		size_t payload_sz = (curr->size > sizeof(block_header_t)) ? (curr->size - sizeof(block_header_t)) : 0;
		console_print_dec(payload_sz);
		console_puts(" B");
		pad = 8;
		tmp = payload_sz;
		while (tmp > 0) { pad--; tmp /= 10; }
		while (pad-- > 0) console_putc(' ');

		if (curr->is_free) {
			console_puts("FREE       ");
		} else {
			console_puts("ALLOCATED  ");
		}

		if (curr->next != NULL) {
			console_print_hex((uint32_t)curr->next);
		} else {
			console_puts("0x00000000 (END)");
		}
		console_puts("\r\n");

		/* Payload Hex & ASCII Preview (first 16 bytes) */
		console_puts("     Payload [0..15]: ");
		for (int i = 0; i < 16; i++) {
			if (i < (int)payload_sz) {
				console_print_byte_hex(payload[i]);
				console_putc(' ');
			} else {
				console_puts(".. ");
			}
		}
		console_puts("| ");
		for (int i = 0; i < 16; i++) {
			if (i < (int)payload_sz) {
				char c = (char)payload[i];
				if (c >= 32 && c <= 126) console_putc(c);
				else console_putc('.');
			} else {
				console_putc(' ');
			}
		}
		console_puts("\r\n");

		/* Integrity verification */
		if (curr->next != NULL) {
			if ((uint8_t *)curr + curr->size != (uint8_t *)curr->next) {
				console_puts(">>> [CORRUPTION DETECTED] Block gap or header overlap at ");
				console_print_hex((uint32_t)curr);
				console_puts("! Expected next: ");
				console_print_hex((uint32_t)((uint8_t *)curr + curr->size));
				console_puts(", Found next: ");
				console_print_hex((uint32_t)curr->next);
				console_puts("\r\n");
				corrupted = true;
				break;
			}
			if ((uint32_t)curr->next >= (uint32_t)&__heap_end) {
				console_puts(">>> [CORRUPTION DETECTED] Next pointer out of heap bounds: ");
				console_print_hex((uint32_t)curr->next);
				console_puts("\r\n");
				corrupted = true;
				break;
			}
		}

		curr = curr->next;
	}

	console_puts("--------------------------------------------------------------------------------\r\n");
	if (!corrupted) {
		console_puts("Integrity Check: OK (Heap block structure is contiguous and physically valid)\r\n");
	} else {
		console_puts("Integrity Check: FAILED (Memory corruption detected in block chain)\r\n");
	}
	console_puts("================================================================================\r\n\r\n");

	arch_ei();
}
