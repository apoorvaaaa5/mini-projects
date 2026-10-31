# Round-Robin Multitasking Scheduler (RP2040)

A preemptive round-robin multitasking kernel and assembly context switcher implemented from scratch on the Raspberry Pi Pico (RP2040 Cortex-M0+).

---

## 🌟 Overview & Key Concepts

- **Preemptive SysTick Scheduling:**
  - A 1 ms periodic SysTick interrupt slices CPU execution time across active tasks.
  - Automatically triggers the `PendSV` exception to perform low-overhead context switching.
- **Assembly Context Switching:**
  - Implements complete Cortex-M0+ context save and restore in `context_switch.S`.
  - Saves callee-saved registers (`r4-r11`) and swaps process stack pointers (`PSP` / `MSP`).
  - Restores the next task's stack frame seamlessly.
- **Concurrent Task Execution:**
  - **Task 1 (`task1_blinky_print`):** Streams periodic USB CDC serial terminal updates and a rotating spinner.
  - **Task 2 (`task2_breathe_led`):** Concurrently modulates hardware PWM duty cycles (2 kHz carrier) on GPIO 25 without blocking or stalling Task 1.
- **Dedicated Task Stacks:**
  - Independent 1 KB stack regions allocated per Task Control Block (TCB).

---

## 📁 Repository Structure

```text
round-robin-rp/
├── Makefile            # Build configuration
├── inc/
│   ├── arch.h          # CPU interrupt control
│   ├── console.h       # Console output APIs
│   ├── platform.h      # RP2040 register maps
│   ├── pwm.h           # Hardware PWM controller
│   ├── scheduler.h     # TCB definitions and scheduler prototypes
│   └── usb_cdc.h       # Bare-metal USB CDC driver
├── src/
│   ├── boot2.S         # QSPI flash bootloader
│   ├── context_switch.S# SysTick / PendSV assembly context switcher
│   ├── startup.S       # Vector table
│   ├── main.c          # Multi-task creation and entrypoints
│   ├── platform.c      # Clocks, GPIO, and PLL configuration
│   ├── pwm.c           # Hardware PWM modulation
│   ├── scheduler.c     # Task Control Block queue and round-robin scheduler
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
Produces `build/ROUND_ROBIN_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/ROUND_ROBIN_RP2040.uf2 /media/$USER/RPI-RP2/
```

### 3. Connect via Serial Terminal (GNU screen)
```bash
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl+A, then \, then y
```
