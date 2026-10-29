#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#define MAX_APPS          2     /* breathe, display */
#define MAX_TASKS         (MAX_APPS + 1) /* Slot 0 (shell) + 2 app slots = 3 slots */
#define maxTask           MAX_TASKS
#define STACK_SIZE_WORDS  256   /* 256 * 4 = 1024 bytes per task */

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

/* Task Control Block (TCB) */
typedef struct TaskControlBlock {
	uint32_t stack_pointer_end;	/* Current saved stack pointer (1st field for assembly) */
	uint8_t slot_id;            /* Physical slot index (0 .. MAX_TASKS-1) */
	uint32_t pid;               /* 32-bit monotonic process ID (1, 2, 3...) */
	status state;
	uint8_t priority;
	uint32_t stack_pointer_begin;
	void (*fnctpt)(void);
	char name[16];
	task_mode_t mode;
} tcb;

extern tcb task[MAX_TASKS];
extern uint8_t R, currentTaskId, nextTaskId, num_tasks;
extern volatile uint32_t system_millis;

/* Scheduler APIs */
void timer_init(void);
int  task_create(void (*entry)(void));
int  task_create_named(const char *name, void (*entry)(void), task_mode_t mode);
int  task_kill(uint8_t slot_id);
int  task_kill_by_name(const char *name);
tcb *task_get_by_pid(uint32_t pid);
void scheduler_start(void);
void start_first_task(uint32_t stack_top, void (*entry)(void));
uint32_t schedule_next(uint32_t current_sp);
void mdelay(uint32_t ms);

#endif /* SCHEDULER_H */
