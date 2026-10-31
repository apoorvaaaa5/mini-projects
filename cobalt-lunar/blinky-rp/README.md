# Blinky RP (RP2040)

A bare-metal LED blink implementation for the Raspberry Pi Pico (RP2040 dual Cortex-M0+), written from scratch without the Raspberry Pi Pico SDK.

---

## 🌟 Overview & Key Concepts

- **Direct Hardware Register Control:** Configures the RP2040 Single-Cycle IO (SIO) peripheral directly via memory-mapped registers.
- **Onboard LED Pin:** Drives GPIO 25 (the green onboard LED on the Raspberry Pi Pico).
- **Custom Linker & Vector Table:** Custom linker script (`link.ld`) and Cortex-M0+ exception vector table (`startup.S`).
- **Second-Stage Bootloader:** Includes custom 256-byte `boot2.S` configuring the Winbond W25Q080 QSPI flash memory for Execute-in-Place (XIP) execution.
- **Busy-Wait Delay:** Demonstrates basic time delay via volatile software countdown loop.

---

## 📁 Repository Structure

```text
blinky-rp/
├── Makefile            # Cross-compilation Makefile (arm-none-eabi)
├── inc/
│   ├── platform.h      # Memory-mapped SIO, GPIO, and IO_BANK0 register definitions
│   └── string.h        # Bare-metal string and memory helpers
├── src/
│   ├── boot2.S         # Second-stage QSPI flash bootloader
│   ├── link.ld         # Memory layout linker script
│   ├── main.c          # Entrypoint and LED toggle loop
│   ├── platform.c      # Low-level GPIO configuration routines
│   └── startup.S       # Vector table and reset handler
└── README.md
```

---

## 🛠️ Build and Flash

### 1. Compile
```bash
make clean
make
```
Produces `build/BLINKY_RP2040.uf2`.

### 2. Flash
Hold the **BOOTSEL** button while connecting the Pico via USB, then copy the UF2 file:
```bash
cp build/BLINKY_RP2040.uf2 /media/$USER/RPI-RP2/
```
The onboard green LED will begin blinking continuously.
