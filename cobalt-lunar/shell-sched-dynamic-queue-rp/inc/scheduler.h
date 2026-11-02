#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#define DEFAULT_STACK_SIZE_BYTES  1024

/* Task state enumeration */
typedef enum status {
	t_quit = 0,
	t_ready,
	t_busy,
	t_wait
} status;

/* Execution mode */
typedef enum task_mode {
	MODE_SYSTEM,
	MODE_FOREGROUND,
	MODE_BACKGROUND
} task_mode_t;

/* Dynamic Task Control Block (TCB) */
typedef struct TaskControlBlock {
	uint32_t stack_pointer_end;    /* Offset 0: Saved Stack Pointer (MANDATORY 1st field for context_switch.S) */
	uint32_t pid;                  /* Monotonic process ID (0, 1, 2, 3...) */
	status state;
	uint32_t *stack_raw;           /* Buffer pointer returned by malloc() for free() */
	uint32_t stack_size_bytes;     /* Allocated stack size in bytes */
	uint32_t stack_pointer_begin;  /* Pristine stack top address */
	void (*fnctpt)(void);          /* Entry function pointer */
	char name[16];                 /* Task name string */
	task_mode_t mode;              /* SYSTEM, FOREGROUND, BACKGROUND */

	struct TaskControlBlock *next; /* Circular Doubly-Linked Ready Queue */
	struct TaskControlBlock *prev;
} tcb;

extern tcb *current_task;
extern volatile uint32_t system_millis;

/* Scheduler APIs */
int  task_create_named(const char *name, void (*entry)(void), task_mode_t mode);
int  task_create_dynamic(const char *name, void (*entry)(void), task_mode_t mode, uint32_t stack_bytes);
int  task_kill(uint32_t pid);
int  task_kill_by_name(const char *name);
tcb *task_get_by_pid(uint32_t pid);
tcb *task_get_queue_head(void);
uint32_t task_get_count(void);
void task_reap_zombies(void);

void scheduler_start(void);
void start_first_task(uint32_t stack_top, void (*entry)(void));
uint32_t schedule_next(uint32_t current_sp);
void mdelay(uint32_t ms);

#endif /* SCHEDULER_H */
