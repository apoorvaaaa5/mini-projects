#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include <stdbool.h>

/* Application Descriptor Structure (User Registry) */
typedef struct app_reg {
	const char *name;         /* App name used in 'run <name>' */
	void (*entry)(void);      /* Task entry function pointer */
	bool bg;                  /* true = Background task, false = Foreground task */
	const char *description;  /* Description shown in 'ls' */
} app_reg_t;

void shell_task(void);
void shell_prompt(void);
void shell_update_bg_telemetry(char spinner, uint32_t ticks);
bool shell_is_foreground(void);

#endif /* SHELL_H */
