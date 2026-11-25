#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

void bootstrap(void);
void plat_led_set(int on);
void plat_led_toggle(void);

#endif /* PLATFORM_H */
