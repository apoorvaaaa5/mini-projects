#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

typedef struct {
	volatile int breathe_duty;
	volatile uint32_t display_ticks;
	volatile char display_spinner;
	volatile uint8_t dirty;
} shared_telemetry_t;

extern shared_telemetry_t g_telemetry;

#endif /* TELEMETRY_H */
