# Shell Sched Split Screen (RP2040)

A bare-metal preemptive multitasking operating system featuring a dynamic tiling split-screen console interface on the Raspberry Pi Pico (RP2040 dual Cortex-M0+).

---

## 🌟 Overview & Key Concepts

### 1. Dynamic Tiling Multi-Pane Window Manager
- **Zero Background Tasks (Full-Screen Shell):**
  - Full-width interactive shell prompt (`appu_os> █`) across the entire terminal width (columns 1 to 80).
- **One Background Task (Vertical 50/50 Split):**
  - **Left Pane (Columns 1–39):** Active interactive shell with automatic line wrapping at column 38.
  - **Vertical Divider (Column 40):** Pinned boundary line (`|`).
  - **Right Pane (Columns 41–80):** Dedicated live telemetry pane displaying task state, carrier parameters, and real-time graphics.
- **Two Background Tasks (Vertical + Horizontal 25/25 Split):**
  - **Left Pane (Columns 1–39):** Interactive shell prompt.
  - **Top-Right Pane (Rows 1–11):** Background Task 1 (e.g. `breathe` duty-cycle bar).
  - **Horizontal Divider (Row 12, Columns 40–80):** Boundary divider (`+---------------------------------------`).
  - **Bottom-Right Pane (Rows 13–24):** Background Task 2 (e.g. `display` tick counter & rotating spinner).
- **Automatic Dynamic Collapse:**
  - When tasks are killed (`kill <app>` or `kill all`), the right side automatically collapses from dual panes to a single pane, or cleanly removes the dividers and restores the full-screen shell.

### 2. Left Shell Pane Line-Wrapping Engine
- Prevents user typing or command output from overflowing across column 40 into the right pane.
- Automatically inserts `\r\n` when typed or printed text reaches column 38, preserving right-pane integrity even with long commands.

### 3. Preemptive Round-Robin Scheduling
- 1 ms hardware SysTick periodic quantum with assembly context switching (`PendSV`).
- Per-task stack isolation (1 KB allocated per TCB).
- One-shot tasks (`hello`) auto-reap upon completion via `task_exit_guard`.

### 4. Concurrency & Stream Safety
- Serial TX mutex (`console_lock` / `console_unlock`) eliminates packet interleaving.
- ANSI escape interceptor transparently swallows arrow keys and mouse scrolls so the input buffer never corrupts.
- Zero `DECSTBM` scroll margin dependencies, ensuring compatibility across GNU `screen` and terminal multiplexers.

---

## 📁 Repository Structure

```text
shell-sched-split-screen-rp/
├── Makefile            # Build configuration (target: SHELL_SCHED_SPLIT_SCREEN_RP2040)
├── inc/                # Architecture and driver headers
│   ├── arch.h          # Interrupt control primitives
│   ├── console.h       # Split-screen tiling engine and console I/O
│   ├── platform.h      # RP2040 peripheral register maps
│   ├── pwm.h           # Hardware PWM slice driver
│   ├── scheduler.h     # TCB definitions and scheduler prototypes
│   ├── shell.h         # Shell command parser and registry
│   ├── string.h        # Bare-metal string utilities
│   ├── usb_cdc.h       # Bare-metal USB CDC driver
│   └── usb_descriptors.h # USB enumeration descriptors
├── src/                # Implementation sources
│   ├── boot2.S         # QSPI flash bootloader
│   ├── context_switch.S# Cortex-M0+ context switch routines
│   ├── startup.S       # Vector table
│   ├── console.c       # Split-screen renderer and escape interceptor
│   ├── main.c          # Task implementations (breathe, display, hello)
│   ├── platform.c      # System clocks, PLL, and GPIO configuration
│   ├── pwm.c           # Hardware PWM modulation
│   ├── scheduler.c     # Preemptive round-robin scheduler
│   ├── shell.c         # Shell command processor and layout coordinator
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
Produces `build/SHELL_SCHED_SPLIT_SCREEN_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/SHELL_SCHED_SPLIT_SCREEN_RP2040.uf2 /media/$USER/RPI-RP2/
```

### 3. Connect via Serial Terminal (GNU screen)
```bash
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl+A, then \, then y
```

---

## 🎮 How to Test the Split-Screen Engine

1. **Boot into Full Screen:**
   ```text
   appu_os> help
   appu_os> ls
   ```
2. **Trigger Vertical Split (1 Background Task):**
   ```text
   appu_os> run breathe
   ```
   * The vertical divider (`|`) appears at column 40.
   * Right pane displays live PWM duty cycle bargraph and carrier frequency.
3. **Trigger Dual Horizontal Split on Right Half (2 Background Tasks):**
   ```text
   appu_os> run display &
   ```
   * Right half splits horizontally: Top-Right runs `breathe`, Bottom-Right runs `display`.
4. **Test Left-Pane Line Wrapping:**
   * Type a long line (e.g. `run breathe hello display ...`). Notice it wraps cleanly at column 38 without crossing the vertical divider!
5. **Test Dynamic Collapse:**
   ```text
   appu_os> kill breathe
   ```
   * Right half collapses back to a single pane for `display`.
   ```text
   appu_os> kill all
   ```
   * Right half and dividers disappear, restoring full-screen shell.
