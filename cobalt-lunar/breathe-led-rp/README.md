# Breathe LED RP (RP2040)

Bare-metal hardware PWM pulse-width modulation and SysTick-calibrated timebase on the Raspberry Pi Pico (RP2040).

---

## 🌟 Overview & Key Concepts

- **Hardware PWM Controller:**
  - Configures the RP2040 hardware PWM slice associated with GPIO 25 (Slice 4, Channel B).
  - Operates at a 2 kHz carrier frequency, delivering smooth analog dimming across 100 duty-cycle steps.
  - Generates a seamless breathing (fade-in / fade-out) effect without software CPU bit-banging.
- **SysTick Timer Timebase:**
  - Calibrated 100 µs SysTick interrupt rate for accurate millisecond delays (`mdelay()`).
  - 10 ms delay per step producing a 1.0 s inhale and 1.0 s exhale cycle.
- **USB CDC Connection Awareness:**
  - Integrated USB CDC device driver displaying live serial banner and breathing duty cycle metrics upon connection.

---

## 📁 Repository Structure

```text
breathe-led-rp/
├── Makefile            # Build script
├── inc/
│   ├── arch.h          # CPU interrupt primitives (CPSIE / CPSID)
│   ├── console.h       # Serial output APIs
│   ├── platform.h      # RP2040 peripheral register maps
│   ├── pwm.h           # Hardware PWM slice configuration
│   ├── systick.h       # SysTick timer driver
│   └── usb_cdc.h       # USB CDC driver
├── src/
│   ├── boot2.S         # QSPI flash bootloader
│   ├── startup.S       # Vector table
│   ├── main.c          # PWM breathing loop and USB status output
│   ├── platform.c      # Clocks, GPIO, and reset unmasking
│   ├── pwm.c           # Hardware PWM configuration and duty-cycle control
│   ├── systick.c       # SysTick ISR handler
│   └── usb_cdc.c       # USB CDC device driver
└── README.md
```

---

## 🛠️ Build and Flash

### 1. Compile
```bash
make clean
make
```
Produces `build/BREATHE_LED_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/BREATHE_LED_RP2040.uf2 /media/$USER/RPI-RP2/
```

### 3. Connect via Serial Terminal (GNU screen)
```bash
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl+A, then \, then y
```
