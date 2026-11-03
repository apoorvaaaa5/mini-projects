#ifndef TASK_H
#define TASK_H

#include <stdint.h>

/* Application Tasks */
void task_breathe_led(void);
void task_display(void);
void task_hello(void);
void task_coremark(void);

/* Display Task Telemetry Accessors */
uint32_t display_get_tick(void);
char     display_get_spinner(void);

#endif /* TASK_H */
