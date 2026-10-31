# Blinky Clock RP (RP2040)

Bare-metal LED blink with integrated voltage regulator (VREG) and system clock (PLL) initialization for the Raspberry Pi Pico (RP2040).

---

## 🌟 Overview & Key Concepts

- **Voltage Regulator (VREG) Setup:** Initializes the RP2040 onboard switching/linear voltage regulator (`plat_vreg_init`).
- **Phase-Locked Loop (PLL) & System Clocks:**
  - Configures the External Crystal Oscillator (XOSC at 12 MHz).
  - Programs the system PLL (`PLL_SYS`) to multiply the 12 MHz reference clock to 125 MHz.
  - Switches the core system clock from the internal Ring Oscillator (ROSC) to `clk_sys` running at full 125 MHz.
- **Accurate Timing:** Demonstrates clock-scaled timing delays running at 125 MHz clock frequency.
- **Direct SIO GPIO:** Drives GPIO 25 (Pico onboard green LED) through memory-mapped SIO registers.

---

## 📁 Repository Structure

```text
blinky-clock-rp/
├── Makefile            # Build configuration using arm-none-eabi toolchain
├── inc/
│   ├── platform.h      # Clocks, PLL, VREG, SIO, and GPIO register maps
│   └── string.h        # Low-level memory utilities
├── src/
│   ├── boot2.S         # QSPI flash bootloader (XIP mode)
│   ├── link.ld         # Memory map and section placement
│   ├── main.c          # Clock setup and main blink loop
│   ├── platform.c      # VREG, XOSC, PLL, and clock muxing routines
│   └── startup.S       # Cortex-M0+ reset vector table
└── README.md
```

---

## 🛠️ Build and Flash

### 1. Compile
```bash
make clean
make
```
Produces `build/BLINKY_CLOCK_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/BLINKY_CLOCK_RP2040.uf2 /media/$USER/RPI-RP2/
```
