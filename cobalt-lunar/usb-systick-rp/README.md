# USB SysTick RP (RP2040)

Bare-metal integration of ARM Cortex-M0+ SysTick periodic interrupts with the hardware USB CDC serial stack on the Raspberry Pi Pico (RP2040).

---

## 🌟 Overview & Key Concepts

- **Hardware SysTick Timer Interrupts:**
  - Configures the ARM Cortex-M0+ SysTick timer (`SYST_CSR`, `SYST_RVR`, `SYST_CVR`) for periodic 100 µs interrupts.
  - Implements calibrated non-blocking millisecond delays (`mdelay()`) while servicing background USB transactions.
- **Power Optimization (`WFI`):**
  - Utilizes `asm volatile("wfi")` (Wait For Interrupt) inside idle loops to reduce core power consumption between timer ticks.
- **Concurrent USB Processing:**
  - Demonstrates concurrent servicing of full-speed USB CDC endpoints while maintaining an accurate hardware timebase.
- **Live Output:**
  - Emits real-time seconds counter and active rotating spinner over `/dev/ttyACM0`.

---

## 📁 Repository Structure

```text
usb-systick-rp/
├── Makefile            # Build configuration
├── inc/
│   ├── arch.h          # CPU interrupt control
│   ├── console.h       # Console I/O
│   ├── platform.h      # Memory-mapped registers
│   ├── systick.h       # SysTick timer configuration
│   └── usb_cdc.h       # Bare-metal USB CDC driver
├── src/
│   ├── boot2.S         # QSPI flash bootloader
│   ├── startup.S       # Vector table
│   ├── main.c          # SysTick-driven main loop with WFI
│   ├── platform.c      # System clock and hardware setup
│   ├── systick.c       # SysTick ISR handler
│   └── usb_cdc.c       # USB device controller
└── README.md
```

---

## 🛠️ Build and Flash

### 1. Compile
```bash
make clean
make
```
Produces `build/USB_SYSTICK_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/USB_SYSTICK_RP2040.uf2 /media/$USER/RPI-RP2/
```

### 3. Connect via Serial Terminal (GNU screen)
```bash
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl+A, then \, then y
```
