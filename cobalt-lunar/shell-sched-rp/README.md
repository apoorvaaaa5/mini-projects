# Shell Sched RP (RP2040)

An interactive bare-metal operating system shell integrated with a preemptive round-robin task scheduler on the Raspberry Pi Pico (RP2040).

---

## 🌟 Overview & Key Concepts

- **Interactive System Shell (`shell`):**
  - Runs as the root interactive task (PID 0) providing a command prompt (`appu_os>`).
  - Supports line editing (printable ASCII characters, backspace, carriage return).
  - Built-in commands: `help`, `ls`, `run <app> [&]`, `ps`, `kill <app>`.
- **Preemptive Round-Robin Engine:**
  - 1 ms SysTick time slicing with assembly context switching (`PendSV`).
  - Concurrent task execution alongside user shell interaction.
- **Dynamic Task Modes:**
  - **Foreground Tasks:** Takes focus of the active terminal session, with `Ctrl+C` interrupt capability.
  - **Background Tasks:** Launched via the `&` suffix or default registry mode; executes concurrently without blocking user input.
- **Application Registry:**
  - `breathe` — Background hardware PWM LED breathing on GPIO 25.
  - `display` — Interactive counter display task.
- **Process Status (`ps`):**
  - Displays PID, Task Name, State (`READY`, `RUNNING`, `WAITING`, `QUIT`), Mode (`SYSTEM`, `FOREGROUND`, `BACKGROUND`), and saved Stack Pointer.

---

## 📁 Repository Structure

```text
shell-sched-rp/
├── Makefile            # Build configuration
├── inc/
│   ├── console.h       # Full-duplex console stream APIs
│   ├── scheduler.h     # TCB and scheduler interfaces
│   ├── shell.h         # Shell command parser and application registry
│   └── usb_cdc.h       # Bare-metal USB CDC driver
├── src/
│   ├── boot2.S         # QSPI flash bootloader
│   ├── context_switch.S# Assembly context save and restore
│   ├── startup.S       # Vector table
│   ├── main.c          # System initialization and application tasks
│   ├── scheduler.c     # Preemptive scheduling implementation
│   ├── shell.c         # Interactive command processor
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
Produces `build/SHELL_SCHED_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/SHELL_SCHED_RP2040.uf2 /media/$USER/RPI-RP2/
```

### 3. Connect via Serial Terminal (GNU screen)
```bash
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl+A, then \, then y
```
