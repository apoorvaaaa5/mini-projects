# Shell Sched One-Shot RP (RP2040)

A preemptive multitasking shell supporting continuous tasks, one-shot ephemeral jobs with automatic lifecycle reaping, and multi-target process termination on the Raspberry Pi Pico (RP2040).

---

## 🌟 Overview & Key Concepts

- **One-Shot Task Execution (`hello`):**
  - Introduces finite, single-pass tasks that do not run inside an infinite loop.
  - Automatically reaps TCB slots upon function return via `task_exit_guard` without creating zombie processes.
  - Runs in both foreground (`run hello`) and background (`run hello &`).
- **Preemptive Round-Robin Scheduling:**
  - 1 ms SysTick time slicing driving assembly context switches (`PendSV`).
  - Supports continuous tasks (`breathe`, `display`) running concurrently with the interactive shell (PID 0).
- **Application Registry & Command Set:**
  - `help` — Command menu.
  - `ls` — Lists all registered applications with execution modes and descriptions.
  - `run <app> [&]` — Launches an application; `&` forces background execution.
  - `ps` — Displays active tasks, states, runtime modes, stack pointers, and telemetry.
  - `kill <app1> [app2...] | kill all` — Multi-target process termination with safety guard against killing root PID 0 (`shell`).
- **Signal Handling & Interrupts:**
  - `Ctrl+C` (ASCII `0x03`) cleanly terminates active foreground tasks and restores the `halo>` prompt.
- **Hardware Integration:**
  - Integrated 2 kHz hardware PWM LED breathing on GPIO 25.
  - Full-duplex bare-metal USB CDC serial communication.

---

## 📁 Repository Structure

```text
shell-sched-oneshot-rp/
├── Makefile            # Build configuration
├── inc/
│   ├── console.h       # Console stream interfaces
│   ├── scheduler.h     # TCB definitions and scheduling prototypes
│   ├── shell.h         # Command parser and registry table
│   └── usb_cdc.h       # Bare-metal USB CDC driver
├── src/
│   ├── boot2.S         # QSPI flash bootloader
│   ├── context_switch.S# Cortex-M0+ context switch routines
│   ├── startup.S       # Vector table
│   ├── main.c          # Application definitions (breathe, display, hello)
│   ├── scheduler.c     # Preemptive scheduler and one-shot exit guard
│   ├── shell.c         # Interactive shell and command processor
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
Produces `build/SHELL_SCHED_ONESHOT_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/SHELL_SCHED_ONESHOT_RP2040.uf2 /media/$USER/RPI-RP2/
```

### 3. Connect via Serial Terminal (GNU screen)
```bash
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl+A, then \, then y
```
