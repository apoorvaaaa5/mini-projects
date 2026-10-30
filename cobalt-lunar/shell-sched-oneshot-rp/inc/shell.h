#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

/* Application Descriptor Structure (User Registry) */
typedef struct app_reg {
	const char *name;         /* App name used in 'run <name>' */
	void (*entry)(void);      /* Task entry function pointer */
	uint8_t bg;               /* 1 = Background task, 0 = Foreground task */
	uint8_t is_oneshot;       /* 1 = One-shot task (runs once and exits), 0 = Continuous */
	const char *description;  /* Description shown in 'ls' */
} app_reg_t;

void shell_task(void);

#endif /* SHELL_H */
