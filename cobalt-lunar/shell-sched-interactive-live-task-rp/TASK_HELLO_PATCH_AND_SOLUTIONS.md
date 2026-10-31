# Task Hello: Implementation Patch, Analysis & Proposed Solutions

This document preserves the complete implementation of the one-shot `task_hello` (task without a `while(1)` loop), the exact code diffs, the interactive shell behavior analysis, and the proposed solutions for clean background/foreground terminal handling.

---

## 1. Preserved Code Diffs

### A. `src/shell.c`

#### 1. Declaration & Application Registry Entry
```c
// Added declaration:
extern void task_hello(void);

// Added to static const app_reg_t app_registry[]:
    { "hello",   task_hello,       0, "One-shot Hello World task (runs once and exits)" },
```

#### 2. Auto-Reap / Foreground Prompt Restoration in `shell_task()`
```c
// At the top of while (1) loop in shell_task():
/* If a foreground task was running and has exited on its own, restore shell prompt */
if (foreground_pid > 0 && (task[foreground_pid].state == t_quit || task[foreground_pid].name[0] == '\0')) {
    foreground_pid = 0;
    console_puts("appu_os> ");
    len = 0;
}
```

---

### B. `src/scheduler.c`

#### Initial Stack Frame Synthesis (Thumb Mode LR Guard)
```c
// In init_task_stack():
sp[15] = 0x01000000UL;                    /* xPSR (Thumb bit 24 = 1) */
sp[14] = ((uint32_t)entry) | 1UL;           /* PC (Task entry function in Thumb mode) */
sp[13] = ((uint32_t)task_exit_guard) | 1UL; /* LR (Fallback exit guard in Thumb mode) */
```

Where `task_exit_guard` is defined as:
```c
static void task_exit_guard(void)
{
    /* Auto-reap task when its function finishes and returns */
    task_kill(currentTaskId);
    while (1) {
        asm volatile("wfi");
    }
}
```

---

### C. `src/main.c`

```c
/*
 * This task does not have a while loop
 * It cleanly exits after printing via task_exit_guard
 */
void task_hello(void)
{
    console_puts("Hello World!\r\n");
}
```

---

## 2. Issue Analysis: The "Orphaned Prompt" in `run hello &`

### The Scenario
When running `run hello &`:
1. `cmd_run` spawns the task in `MODE_BACKGROUND` (`foreground_pid` remains `0`).
2. Shell immediately outputs `appu_os> `.
3. Scheduler gives a timeslice to `task_hello`.
4. `task_hello` prints `"Hello World!\r\n"`.

### Visual Impact on Terminal
```text
appu_os> run hello &
[OK] Spawned 'hello' in background with PID 1 (forced by '&').
appu_os> Hello World!
_
```
* The newline in `Hello World!\r\n` pushes the terminal cursor down to an empty line.
* The cursor `_` is left on a line without an `appu_os> ` prompt in front of it.
* Even though the shell is still listening for commands (such as `ps` or `kill`), visually it appears as though the prompt is missing or hung.

---

## 3. Evaluated Solutions for Future Implementation

### Solution Option 1: Foreground-Only Terminal Streaming (Muted in Background)
Follow the same pattern used for `display`:
```c
void task_hello(void)
{
    /* Only print to console if running in FOREGROUND */
    if (task[currentTaskId].mode == MODE_FOREGROUND && usb_cdc_is_connected()) {
        console_puts("Hello World!\r\n");
    }
}
```
* **Pros:** Clean, zero prompt corruption. Background tasks run completely silently.
* **Cons:** Running `run hello &` produces no terminal output (only runs logic in RAM).

---

### Solution Option 2: Asynchronous Output Notification & Prompt Re-Rendering
Allow background tasks to print, but when any background output occurs, re-render the prompt line:
```c
// When background task finishes, or after asynchronous console_puts:
console_puts("\r\nappu_os> ");
```
* **Pros:** Output is visible to user, and the prompt is always restored immediately under the new output.
* **Cons:** Requires a shared console lock or shell redraw mechanism if the user was actively typing a command when the output arrived.

---

### Solution Option 3: Tailored Foreground Launch Message
In `cmd_run`:
Instead of a generic `"Press Ctrl+C to stop..."` for all foreground tasks, check if the app is one-shot or continuous:
```c
if (app->is_oneshot) {
    console_puts("[RUN] Running '");
    console_puts(app->name);
    console_puts("'...\r\n");
} else {
    console_puts("[RUN] Running '");
    console_puts(app->name);
    console_puts("' in foreground. Press Ctrl+C to stop...\r\n");
}
```
