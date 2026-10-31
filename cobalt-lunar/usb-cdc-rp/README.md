# USB CDC RP (RP2040)

A bare-metal USB 2.0 Full-Speed CDC-ACM (Communication Device Class / Virtual Serial Port) implementation for the Raspberry Pi Pico (RP2040).

---

## 🌟 Overview & Key Concepts

- **Zero Third-Party SDK:** Full USB 2.0 device controller stack implemented directly in bare-metal C without TinyUSB or Pico SDK dependencies.
- **CDC-ACM Specification:**
  - Implements USB Control Endpoint 0 (standard enumeration, descriptor requests, SET_CONFIGURATION).
  - Implements Communication Class Interface (CCI) with Notification Endpoint.
  - Implements Data Class Interface (DCI) with Bulk IN and Bulk OUT endpoints.
- **Hardware USB Controller (DPRAM):**
  - Manages RP2040 USB dual-port RAM (DPRAM) buffers, buffer control registers, and endpoint control registers.
- **Serial Heartbeat Monitor:**
  - Emits dynamic rotating spinner (`/`, `-`, `\`, `|`) and live heartbeat statistics over the virtual COM port.
  - Automatically detects when a terminal attaches via DTR / RTS line assertions.

---

## 📁 Repository Structure

```text
usb-cdc-rp/
├── Makefile            # Build configuration
├── inc/
│   ├── console.h       # Console streaming APIs
│   ├── platform.h      # RP2040 register definitions
│   ├── usb_cdc.h       # USB controller and CDC class interfaces
│   └── usb_descriptors.h # USB device, configuration, and interface descriptors
├── src/
│   ├── boot2.S         # QSPI flash bootloader
│   ├── startup.S       # Vector table
│   ├── console.c       # Console formatted output routines
│   ├── main.c          # USB polling and serial heartbeat loop
│   ├── platform.c      # Clocks, GPIO, and USB reset unmasking
│   ├── usb_cdc.c       # Hardware USB controller driver and transaction engine
│   └── usb_descriptors.c # USB enumeration descriptors
└── README.md
```

---

## 🛠️ Build and Flash

### 1. Compile
```bash
make clean
make
```
Produces `build/USB_CDC_RP2040.uf2`.

### 2. Flash to RP2040

#### Method A: Direct Automated Flash (via picotool)
```bash
make flash
```

#### Method B: Manual BOOTSEL Mount
Hold the **BOOTSEL** button on the Raspberry Pi Pico while plugging it into the USB port, then copy the binary:
```bash
cp build/USB_CDC_RP2040.uf2 /media/$USER/RPI-RP2/
```

### 3. Connect via Serial Terminal (GNU screen)
```bash
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl+A, then \, then y
```
