# Shell Sched Interactive Live Task (RP2040)

A bare-metal preemptive multitasking operating system and interactive shell for the Raspberry Pi Pico (RP2040 dual Cortex-M0+).

---

## 🌟 Key Features

### 1. Preemptive Round-Robin Multitasking Kernel
- **Periodic Quantum:** 1 ms hardware SysTick timer driving preemptive round-robin context switching (`PendSV` / assembly context save and restore).
- **Process Isolation:** Independent stack pointers allocated per task with dynamic Stack Pointer (SP) tracking.
- **Process Table:** Tracks up to 4 concurrent processes (PID 0 reserved for root interactive system shell).

### 2. Interactive Shell Subsystem
- **Standard Command Set:**
  - `help` — Display system command menu.
  - `ls` — Inspect registered applications in the App Registry.
  - `run <app> [&]` — Spawn tasks in foreground or background (`&` flag).
  - `ps` — List active process table with PIDs, execution states (`READY`, `RUNNING`, `WAITING`, `QUIT`), runtime modes (`SYSTEM`, `FOREGROUND`, `BACKGROUND`), stack pointers, and telemetry.
  - `kill <app1> [app2...] | kill all` — Multi-target process termination with root shell protection (`kill shell` / `kill 0` safety guard).

### 3. Relative In-Line Live Background Telemetry
- **Non-Intrusive Telemetry:** Background tasks continuously update their live state (spinner, monotonic ticks, PWM frequency) on a dedicated line directly above the active command prompt (`appu_os>`).
- **Relative In-Line Redrawing:** Uses pure relative cursor repositioning (`\r`, `\033[K`, `\033[1A`) rather than hardcoded screen rows or scroll margins (`DECSTBM`).
- **Typing Preservation:** Re-emits active user keystrokes in place, keeping the hardware cursor locked at the end of the input buffer without flicker or stranding.
- **Dynamic Adaptation:** Automatically combines multi-task telemetry lines and cleanly removes the telemetry line when background tasks terminate.

### 4. Comprehensive Task Lifecycle Models
- **Continuous Background Tasks (`breathe`):** Modulates hardware PWM on GPIO 25 (2 kHz carrier) with smooth 10 ms analog brightness breathing.
- **Interactive Foreground Tasks (`display`):** Streams live counter telemetry directly to the active cursor line; cleanly terminates on `Ctrl+C` (ASCII `0x03`) and restores shell prompt.
- **One-Shot Auto-Reaped Tasks (`hello`):** Executes single-pass jobs in both foreground and background, automatically cleaning up TCB slots and returning resources without creating zombie processes.

### 5. Terminal Stream Sanitization & Concurrency
- **ANSI Escape Interceptor:** Transparently intercepts and swallows arrow keys (`\033[A`, `\033[B`), terminal queries, and mouse wheel bursts, preventing stray `[A[A[A[B` escape codes from leaking into the shell input buffer.
- **Serial TX Mutex:** Atomic mutex lock (`console_lock` / `console_unlock`) protects serial writes across concurrent SysTick task preemption.
- **Universal Compatibility:** Verified for standard VT100 / ANSI serial terminals, GNU `screen`, and `tmux` split panes.

---

## 📁 Repository Structure

```text
shell-sched-interactive-live-task-rp/
├── Makefile            # Build configuration (cross-compilation with arm-none-eabi)
├── inc/                # Architecture and driver headers
│   ├── arch.h          # Cortex-M0+ interrupt enable/disable primitives
│   ├── console.h       # Full-duplex console I/O and TX mutex APIs
│   ├── platform.h      # RP2040 memory-mapped registers (SIO, Clocks, Resets)
│   ├── pwm.h           # Hardware PWM slice configuration
│   ├── scheduler.h     # Task Control Block (TCB) definitions and scheduling APIs
│   ├── shell.h         # Shell command engine and relative telemetry APIs
│   ├── string.h        # Bare-metal string manipulation utilities
│   ├── usb_cdc.h       # USB CDC ACM class device driver
│   └── usb_descriptors.h # USB device, configuration, and interface descriptors
├── src/                # Assembly bootstrap and C source implementations
│   ├── boot2.S         # Second-stage bootloader (W25Q080 QSPI Flash XIP)
│   ├── context_switch.S# SysTick / PendSV context save and restore routines
│   ├── startup.S       # Vector table and reset handler
│   ├── console.c       # Escape-intercepting console stream driver
│   ├── main.c          # System entrypoint and task implementations
│   ├── platform.c      # RP2040 clock initialization and GPIO setup
│   ├── pwm.c           # Hardware PWM driver
│   ├── scheduler.c     # Preemptive round-robin scheduler implementation
│   ├── shell.c         # Interactive shell, command dispatcher, and telemetry engine
│   ├── string.c        # String implementation
│   └── usb_cdc.c       # USB CDC driver
└── README.md
```

---

## 🛠️ Build and Flash

### 1. Compile
```bash
make clean
make
```
Generates the UF2 binary in `build/SHELL_SCHED_INTERACTIVE_LIVE_TASK_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/SHELL_SCHED_INTERACTIVE_LIVE_TASK_RP2040.uf2 /media/$USER/RPI-RP2/
```

### 3. Connect via Serial Terminal (GNU screen)
```bash
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl+A, then \, then y
```
