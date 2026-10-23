#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#define maxTask           4
#define STACK_SIZE_WORDS  256   /* 256 * 4 = 1024 bytes per task */

/* Task state enumeration */
typedef enum status {
	t_ready,
	t_busy,
	t_wait,
	t_quit
} status;

/* Task Control Block (TCB) */
typedef struct TaskControlBlock {
	uint32_t stack_pointer_end;	/* Current saved stack pointer (1st field for assembly) */
	uint8_t task_id;
	status state;
	uint8_t priority;
	uint32_t stack_pointer_begin;
	void (*fnctpt)(void);
} tcb;

extern tcb task[maxTask];
extern uint8_t R, currentTaskId, nextTaskId, num_tasks;
extern volatile uint32_t system_millis;

/* Scheduler APIs */
void timer_init(void);
int  task_create(void (*entry)(void));
void scheduler_start(void);
void start_first_task(uint32_t stack_top, void (*entry)(void));
uint32_t schedule_next(uint32_t current_sp);
void mdelay(uint32_t ms);

#endif /* SCHEDULER_H */
