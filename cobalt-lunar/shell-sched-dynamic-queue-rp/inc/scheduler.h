#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DEFAULT_STACK_SIZE_BYTES  1024

/* Task state enumeration */
typedef enum status {
	t_quit = 0,
	t_ready,
	t_busy
} status;

/*
 * Minimal Task Control Block (TCB)
 * Restricted strictly to the task's runtime execution state.
 */
typedef struct TaskControlBlock {
	uint32_t stack_pointer_end;    /* Offset 0: Saved PSP for context_switch.S */
	uint32_t pid;                  /* Monotonic process ID (0, 1, 2, 3...) */
	status state;                  /* t_ready, t_busy, t_quit */
	void (*fnctpt)(void);          /* Entry function pointer */
	char name[16];                 /* Task name string */

	struct TaskControlBlock *next; /* Circular Singly-Linked Ready Queue */
} tcb;

extern tcb *current_task;
extern volatile uint32_t system_millis;

/* Scheduler APIs (Task queue and dequeue operations are handled purely inside scheduler.c) */
int  task_create_named(const char *name, void (*entry)(void));
int  task_create_dynamic(const char *name, void (*entry)(void), uint32_t stack_bytes);
int  task_kill(uint32_t pid);
int  task_kill_by_name(const char *name);
tcb *task_get_by_pid(uint32_t pid);
tcb *task_get_queue_head(void);
uint32_t task_get_count(void);

void scheduler_start(void);
void start_first_task(uint32_t stack_top, void (*entry)(void));
uint32_t schedule_next(uint32_t current_sp);
void task_yield(void);
void mdelay(uint32_t ms);

#endif /* SCHEDULER_H */
