# System Features Specification — Appu OS (RP2040)

Detailed technical specification of the multitasking architecture, execution models, and shell features implemented in `shell-sched-interactive-live-task-rp`.

---

## 1. Multitasking & Kernel Features

* **Dual ARM Cortex-M0+ Target:** Bare-metal execution on the RP2040 with ARMv6-M architecture.
* **Preemptive Round-Robin Scheduling:** Hardware SysTick generates a periodic 1 ms interrupt tick triggering `PendSV` context switches.
* **Process Isolation:**
  * Dedicated 1024-byte (256-word) stack allocated per Task Control Block (TCB).
  * Assembly context saving (`push {r4-r7}`, `mov r4-r7, r8-r11`, `push {r4-r7}`) and hardware exception frame management (`r0-r3, r12, lr, pc, xpsr`).
* **Process Lifecycle Management:**
  * Dynamic monotonic PID assignment (`1, 2, 3...`).
  * Process states: `READY`, `RUNNING`, `WAITING`, `QUIT`.
  * Dynamic TCB slot reclaiming upon process termination.

---

## 2. Interactive Shell Subsystem

* **Full-Duplex USB CDC Terminal:** Serial streaming over standard USB CDC ACM profile (115200 baud).
* **Command Dispatcher:**
  * `help` — Prints the system command menu.
  * `ls` — Lists all registered tasks in the Application Registry with default modes and descriptions.
  * `run <app> [&]` — Spawns applications with optional `&` flag to force background execution.
  * `ps` — Formatted process status table displaying PID, Name, State, Mode, current Stack Pointer (`STACK_SP`), and Telemetry.
  * `kill <app1> [app2...] | kill all` — Multi-target process termination engine.
* **Root Shell Safety Guard:**
  * Prevents suicide of PID 0 (`shell`).
  * Rejects `kill shell` or `kill 0` with an explicit safety error message.

---

## 3. Relative In-Line Live Background Telemetry Engine

* **Relative Cursor Movement:**
  * Emits ANSI relative controls: `\r` (carriage return), `\033[K` (clear line), and `\033[1A` (cursor UP 1 line).
  * Avoids fixed screen row assumptions and hardware scroll margins (`DECSTBM`).
* **Active Input Buffer Preservation:**
  * When background tasks tick, the telemetry line updates in place directly above the command prompt (`appu_os>`).
  * The current user input buffer is re-emitted with the hardware cursor kept at the end of typed characters.
* **Multi-Task Telemetry Synthesis:**
  * Combines outputs of multiple active background tasks on a single status line.
  * Automatically removes the status line when all background tasks are terminated.

---

## 4. Task Execution Models

| Application | Execution Mode | Behavior | Lifecycle |
| :--- | :--- | :--- | :--- |
| **`shell`** | `MODE_SYSTEM` | Root interactive CLI; manages process spawning and signals. | Permanent (PID 0, cannot be killed) |
| **`breathe`** | `MODE_BACKGROUND` | Modulates hardware PWM duty cycle (0% to 100%) on GPIO 25 LED at 2 kHz with 10 ms steps. | Continuous background task |
| **`display`** | `MODE_FOREGROUND` or `MODE_BACKGROUND` | Advances monotonic 2-second tick count and rotating spinner (`/`, `-`, `\`, `|`). Streams to active line in foreground; updates telemetry in background. | Continuous; interruptible via `Ctrl+C` in foreground or `kill` in background |
| **`hello`** | `MODE_FOREGROUND` or `MODE_BACKGROUND` | One-shot job that prints greeting banner. Auto-reaps its slot via `task_exit_guard`. | Ephemeral (runs once and auto-exits) |

---

## 5. Concurrency & Stream Protection

* **Serial Mutex Lock:** Atomic lock (`console_lock` / `console_unlock`) ensures serial writes from SysTick preemption do not interleave with user typing.
* **Transparent Escape Code Interceptor:**
  * Swallows burst escape sequences (e.g. arrow keys `\033[A`, `\033[B`, mouse wheel packets).
  * Prevents garbage characters (`[A[A[A[B`) from leaking into the shell input buffer.
* **Zero Host Collision:** Terminal appends naturally on the next line without wiping screen history or colliding with host prompts.
