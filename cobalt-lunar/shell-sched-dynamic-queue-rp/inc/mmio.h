#ifndef MMIO_H
#define MMIO_H

#include <stdint.h>

#define mmio32(addr) (*(volatile uint32_t *)(addr))
#define mmio8(addr)  (*(volatile uint8_t *)(addr))

#endif /* MMIO_H */
