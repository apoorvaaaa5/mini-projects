# RP2040 Bare-Metal Technical Learning Reference

A permanent low-level systems engineering reference guide for the Raspberry Pi RP2040 microcontroller (ARM Cortex-M0+ dual-core architecture).

---

## Module 1: The Boot Stage 2 (`boot2.S`) Silicon Contract

### Q1: Why does the RP2040 require a second-stage bootloader (`boot2`) when booting from flash?
* **Zero Internal Non-Volatile Memory:** The RP2040 chip contains 264 KB of internal SRAM, but zero on-chip flash. Code is stored on an external SPI/QSPI NOR flash chip (such as the Winbond W25Q16JV on the Raspberry Pi Pico) connected via 6 pins (Clock, CSn, and IO0-IO3).
* **The Boot ROM Hardware State:** Out of reset, the RP2040 internal Boot ROM only knows standard 1-bit SPI read commands. To execute instructions at full processor speed (133 MHz) through the memory-mapped XIP address window (`0x10000000`–`0x1FFFFFFF`), the external flash chip and the Synopsys Synchronous Serial Interface (`SSI`) controller must be configured into **Quad I/O Continuous Read Mode (Command `0xEB`)**.
* **The 256-Byte Contract:** 
  1. The Boot ROM reads exactly 256 bytes from address `0x10000000`.
  2. The last 4 bytes (offset 252 to 255) must contain a **CRC32 checksum** matching polynomial `0x04C11DB7`.
  3. If the checksum matches, the Boot ROM copies these 256 bytes into SRAM at `0x20041F00` and executes them.
  4. `boot2` configures the SSI registers, issues the `0xEB` mode continuation prefix, and branches to address **`0x10000100`** (the user application vector table).
  5. If the checksum fails, the Boot ROM rejects the flash image and halts into USB `BOOTSEL` mode.

---

### Warning Alert (Style C): The "External Flash is Instantly Memory-Mapped" Fallacy

> [!WARNING]
> **Misconception:** *"If I write my Cortex-M vector table and code starting directly at Flash address `0x10000000`, the microcontroller program counter will simply begin executing instructions on reset."*
> 
> **Why this causes complete boot failure:**
> 1. **Silicon Bootrom Rejection:** The on-chip Boot ROM hard-checks the first 256 bytes for a valid `boot2` signature and CRC32 checksum.
> 2. **Silent Halt:** If the application vector table is placed at `0x10000000`, the CRC32 check fails. The Boot ROM declares the flash image corrupted and enters USB BOOTSEL mode. The user `entry` point is never called, and the chip appears dead.
> 3. **The Solution:** Always reserve the first 256 bytes (`0x10000000` to `0x100000FF`) in the linker script for `.boot2`, and locate the user vector table at `0x10000100`.

---

## Module 2: Hardware Defaults: Voltage Regulator & Clock Initialization

### Q1: Why does a bare-metal RP2040 Blinky program run without software-configuring the Voltage Regulator or System Clock?
* **Integrated Hardware State Machine (PSM):** On the RP2040, the on-chip Power-On State Machine automatically boots the internal Low-Dropout (LDO) voltage regulator to **1.10V nominal** (`DVDD`) as soon as 3.3V power is applied to the board. Software only needs to touch `VREG_AND_CHIP_RESET` (`0x40064000`) when overclocking past 250 MHz (bumping VSEL to 1.20V or 1.30V).
* **The Ring Oscillator (ROSC) Safety Net:** The RP2040 contains an on-chip Ring Oscillator that requires no external crystals or capacitors. It starts running within nanoseconds of power-on at nominal **~6 MHz** (guaranteed 1.8 to 12 MHz). Both Cortex-M0+ cores and the Single-Cycle I/O (`SIO`) block run automatically on this ROSC clock out of reset. A simple delay loop (`for (volatile int i = 0; i < 500000; i++);`) delays execution on this default 6 MHz clock without requiring complex Phase-Locked Loop (`PLL_SYS`) configuration.

---

### Q2: How does custom 125 MHz PLL clocking differ from the default ROSC?
* **ROSC (~6 MHz):** Low-power, variable with temperature and voltage, zero external components. Ideal for early boot, simple GPIO toggling, and sleep states.
* **XOSC + PLL_SYS (125 MHz):** Requires a 12 MHz external crystal connected to `XIN`/`XOUT`.
  * **Phase-Locked Loop (`PLL_SYS` at `0x40028000`):** Multiplies 12 MHz up to 1500 MHz VCO (`FBDIV = 125`), then divides down by 12 (`POSTDIV1 = 6`, `POSTDIV2 = 2`) to produce a precise, jitter-free **125.0 MHz system clock**.
  * Essential for exact timing: USB 1.1 (48 MHz), UART baud rates (115200), and max-speed computation.

---

## Module 3: Subsystem Resets (`RESETS`) & Peripheral Wakeup

### Q1: Why does writing directly to a GPIO register fail if `RESETS` is not cleared?
* **Architectural Clock Gating & Reset Protection:** Out of power-on reset, the RP2040 Reset Controller holds almost all internal peripherals (including `IO_BANK0`, `PADS_BANK0`, `UART0`, `SPI0`, `PWM`) in reset to minimize inrush current and power consumption.
* **The Wakeup Handshake:**
  1. De-assert reset by clearing the peripheral's bit in `RESETS_RESET` (`0x4000C000`). For `IO_BANK0`, clear bit 5:
     ```c
     mmio32(0x4000C000) &= ~(1 << 5);
     ```
  2. Wait until the hardware peripheral clock stabilizes and confirms it is awake by polling `RESETS_RESET_DONE` (`0x4000C008`):
     ```c
     while ((mmio32(0x4000C008) & (1 << 5)) == 0);
     ```
  3. Attempting to write to `IO_BANK0_GPIO25_CTRL` before this handshake completes results in bus lockup or silently discarded writes.

---

## Module 4: GPIO Pin Multiplexing & Single-Cycle I/O (`SIO`)

### Q1: What is the architectural difference between `IO_BANK0` and `SIO`?
* **`IO_BANK0` (Pin Muxing at `0x40014000`):**
  Controls the physical pad routing. Each GPIO pin has a `GPIOx_CTRL` register where bits 4:0 (`FUNCSEL`) select which internal peripheral connects to the external copper pin (UART, SPI, PWM, I2C, or SIO).
  * On the Raspberry Pi Pico, the green LED is wired to GPIO 25.
  * Setting `FUNCSEL = 5` connects GPIO 25 to the SIO block:
    ```c
    mmio32(0x400140CC) = 5; // GPIO25_CTRL = SIO
    ```
* **`SIO` (Single-Cycle I/O at `0xD0000000`):**
  An auxiliary peripheral accessed directly via the Cortex-M0+ **IOPORT bus**, completely bypassing the main AHB system bus.
  * All reads and writes take **exactly 1 CPU clock cycle** with zero wait states.
  * **Atomic Hardware Registers:**
    * `SIO_GPIO_OE_SET` (`0xD0000024`): Enables pin output without altering other pins.
    * `SIO_GPIO_OUT_XOR` (`0xD000001C`): Inverts pin output state in a single clock cycle without requiring a slow read-modify-write cycle.

---

## Module 5: ARM Cortex-M0+ vs. RISC-V Startup Architecture

```text
  ┌─────────────────────────────────────────────────────────────┐
  │                 Bare-Metal Vector Table                     │
  ├──────────────────────┬──────────────────────────────────────┤
  │ Vector 0 (SP)        │ Initial Main Stack Pointer (MSP)     │
  │ Vector 1 (Reset)     │ Reset_Handler (entry)                │
  │ Vector 2 (NMI)       │ ──► panic_handler() (Catch-all Trap) │
  │ Vector 3 (HardFault) │ ──► panic_handler() (Catch-all Trap) │
  │ Vector 4..15         │ ──► panic_handler() (Catch-all Trap) │
  │ Vector 16..47 (IRQs) │ ──► panic_handler() (Default weak)   │
  └──────────────────────┴──────────────────────────────────────┘
```

| Architectural Concept | RISC-V (`timer_irqs`) | ARM Cortex-M0+ RP2040 (`blinky-rp`) |
| :--- | :--- | :--- |
| **Vector Table Contents** | Executable jump instructions (`j isr`) | 32-bit function addresses / pointers (`.word`) |
| **Initial Stack Pointer** | Set via software instruction (`mv sp, t0`) | Loaded automatically by hardware from **Vector 0** |
| **Reset Execution Point** | Reset jumps directly to linker entry | Loaded automatically by hardware from **Vector 1** |
| **Vector Base Pointer** | Registered in software via CSR: `csrw mtvec, t0` | Hardwired at `0x00000000` / `0x10000100` / `VTOR` |
| **Calling Main** | `jal main` | `bl main` |
| **Halt Trap** | `j .` | `b .` or `wfi; b .` (`panic_handler`) |
| **Memory Init Timing** | First call in `main()`: `plat_mem_init()` | First call in `main()`: `plat_mem_init()` |

---

## Module 6: Linker Memory Map & Runtime Data Copying

### The Flash LMA to SRAM VMA Layout:
* **Flash (LMA `0x10000000`):** Non-volatile storage. Contains `.boot2` (first 256 bytes), `.vectors` (starting at `0x10000100`), `.text`, `.rodata`, and the initial values of global variables in `.data`.
* **SRAM (VMA `0x20000000`):** Fast, volatile read/write memory. Holds running `.data`, zero-initialized `.bss`, and the downward-growing execution `.stack`.

```text
    [ Flash (LMA at 0x10000000) ]                 [ SRAM (VMA at 0x20000000) ]
  ┌───────────────────────────────┐             ┌─────────────────────────────┐
  │ 0x10000000: .boot2 (256 B)    │             │ 0x20000000: .data section   │ (Read/Write)
  ├───────────────────────────────┤             │   (Copied by plat_mem_init) │
  │ 0x10000100: .vectors          │             ├─────────────────────────────┤
  ├───────────────────────────────┤             │ .bss section (Zeroed)       │
  │ .text & .rodata               │             ├─────────────────────────────┤
  ├───────────────────────────────┤             │ .stack (Grows downward from │
  │ .data initial values (LMA)    │──(memcpy)──►│  _stack_top at 0x20001000)  │
  └───────────────────────────────┘             └─────────────────────────────┘
```

#### Runtime Copy Function (`plat_mem_init`):
```c
extern void *_data_start, *_data_vstart, *_data_size;
extern void *_bss_start, *_bss_size;

void plat_mem_init(void)
{
    /* 1. Copy initialized variables from Flash LMA to SRAM VMA */
    memcpy(&_data_vstart, &_data_start, (unsigned int)&_data_size);

    /* 2. Zero-out uninitialized variables in SRAM */
    memset(&_bss_start, 0, (unsigned int)&_bss_size);
}
```
