# RP2040 Project Tracker & User Directives

A dedicated tracker for project milestones, user directives, testing workflows, and high-priority architectural takeaways for bare-metal RP2040 development.

---

## 1. Project Overview & Environment

* **Target Board:** Raspberry Pi Pico (Original Green Board, non-W, Micro-USB)
* **Target SoC:** Raspberry Pi RP2040 (Dual-Core ARM Cortex-M0+, up to 133 MHz)
* **Flash:** 2 MB external Winbond QSPI Flash (W25Q16JV)
* **RAM:** 264 KB internal SRAM (`0x20000000` to `0x20042000`)
* **Project Location:** [`mini-projects/cobalt-lunar/blinky-rp/`](file:///home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp/)
* **Toolchain Locations:**
  * ARM GCC Toolchain: [`mini-projects/tools/arm-toolchain/bin/arm-none-eabi-*`](file:///home/apoorva/Workspace/mini-projects/tools/arm-toolchain/)
  * Picotool (v2.3.1): [`mini-projects/tools/picotool/bin/picotool`](file:///home/apoorva/Workspace/mini-projects/tools/picotool/)

---

## 2. Current Implementation Checklist

| Component | Source File | Status | Notes |
| :--- | :--- | :--- | :--- |
| **Boot Stage 2 (`boot2`)** | [`src/boot2.S`](file:///home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp/src/boot2.S) | Completed | 256 bytes QSPI continuous read init + CRC32 checksum |
| **Vector Table & Entry** | [`src/startup.S`](file:///home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp/src/startup.S) | Completed | Vector 0 (SP), Vector 1 (Entry), 46 `panic_handler` traps |
| **Runtime Memory Init** | [`src/string.c`](file:///home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp/src/string.c) | Completed | Bare-metal `memcpy` (.data Flash->RAM) & `memset` (.bss zero) |
| **Platform Hardware** | [`src/platform.c`](file:///home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp/src/platform.c) | Completed | Subsystem resets (`IO_BANK0`), GPIO 25 SIO mux, atomic XOR |
| **Main Application** | [`src/main.c`](file:///home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp/src/main.c) | Completed | Calls `plat_mem_init()`, `plat_gpio_init()`, runs `for` delay |
| **Linker Script** | [`src/link.ld`](file:///home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp/src/link.ld) | Completed | Places `.boot2` at `0x10000000`, `.vectors` at `0x10000100` |
| **Build & Flash Rules** | [`Makefile`](file:///home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp/Makefile) | Completed | Builds `.elf`, `.bin`, `.lst`, `.uf2`, supports `make flash` |

---

## 3. Flashing & Testing Instructions

### Hardware Connection:
1. Use a **Micro-USB Data Cable** (must have D+/D- data lines, not a 2-wire charge-only cable).
2. **Enter BOOTSEL Mode:**
   * Unplug USB.
   * Hold down the white **`BOOTSEL`** button on the Pico.
   * Plug USB cable into PC.
   * Release button after 2 seconds.
3. Verify connection in Linux:
   ```bash
   lsusb | grep "2e8a:0003"
   ```

### Build & Flash:
```bash
cd /home/apoorva/Workspace/mini-projects/cobalt-lunar/blinky-rp

# 1. Clean and build all artifacts (.elf, .bin, .uf2, .lst):
make clean && make

# 2. Flash and automatically reboot into application mode:
sudo ../../tools/picotool/bin/picotool load out/Blinky_RP2040.elf
sudo ../../tools/picotool/bin/picotool reboot
```

---

## 4. Key Architectural Insights & Critical Takeaways (Quick Reference)

*Points to revisit quickly to reflect, learn, and build upon:*

1. **The `boot2` Silicon Contract:**
   * The RP2040 has **zero on-chip flash**. Flash is external (Winbond W25Q16JV).
   * The on-chip Boot ROM **mandates** that the first 256 bytes of flash contain a valid boot stage 2 image with a CRC32 checksum.
   * If `boot2` is missing, the Boot ROM refuses to boot and halts into BOOTSEL mode.
   * In `link.ld`, `.boot2` MUST be at `0x10000000`, and `.vectors` MUST start at `0x10000100`.

2. **Automatic Hardware Defaults (No Code Required for Blinky):**
   * **Voltage Regulator:** Automatically powered up to **1.10V nominal** (`DVDD`) by the Power-On State Machine (PSM).
   * **System Clock:** Automatically started by the on-chip Ring Oscillator (**ROSC**) at **~6 MHz**. Both CPU cores and SIO run on this clock out of reset.
   * Custom clock/PLL code is only needed for high-speed peripherals (USB, UART baud rates, 125 MHz computing).

3. **Subsystem Resets (`RESETS` at `0x4000C000`):**
   * Hardware holds `IO_BANK0` asleep out of reset.
   * Code must de-assert bit 5 in `RESETS_RESET` and wait for bit 5 in `RESETS_RESET_DONE` before reading/writing GPIO registers.

4. **Single-Cycle I/O (`SIO` at `0xD0000000`):**
   * Connected directly to the Cortex-M0+ IOPORT auxiliary bus. Reads/writes take **1 single clock cycle**.
   * Atomic XOR register (`SIO_GPIO_OUT_XOR` at `0xD000001C`): Writing `(1 << 25)` flips the LED state without needing a slow read-modify-write cycle.

5. **ARM Cortex-M0+ vs. RISC-V Vector Table:**
   * RISC-V vector tables contain **jump instructions** (`j isr`).
   * ARM Cortex-M0+ vector tables contain **pointers/addresses** (`.word`).
   * Vector 0 is the **Main Stack Pointer (`_stack_top`)**.
   * Vector 1 is the **Reset Execution Address (`entry`)**.

---

## 5. "How to Improve" Action Items & Project Backlog

*A tracking list of architectural features to study, design, and add to the project:*

- [ ] **Milestone 1: Verify Physical Blinky on Hardware**
  - Connect with data cable, run `picotool load -x`, and verify the green LED on GPIO 25 blinks.
- [ ] **Milestone 2: Implement Custom 125 MHz PLL Clock Driver (`plat_clock_init`)**
  - Configure `XOSC` (12 MHz external crystal at `0x40024000`).
  - Configure `PLL_SYS` (`0x40028000`) with `FBDIV = 125`, `POSTDIV1 = 6`, `POSTDIV2 = 2` to generate 125.0 MHz.
  - Switch `clk_sys` and `clk_ref` to `PLL_SYS`.
  - Recalculate delay loops for 125 MHz execution.
- [ ] **Milestone 3: Bare-Metal UART Driver (`plat_uart_init` & `plat_uart_puts`)**
  - De-assert reset on `UART0`.
  - Configure GPIO 0 (TX) and GPIO 1 (RX) muxing to UART (Function 2).
  - Set integer and fractional baud rate dividers for 115200 baud.
  - Print "Hello World from RP2040 Bare Metal!" to a serial terminal over USB-to-UART.
- [ ] **Milestone 4: Core 1 Dual-Core Launch via SIO Mailbox FIFO**
  - Implement the multi-core handshake state machine across SIO FIFOs (`SIO_FIFO_ST`, `SIO_FIFO_WR`, `SIO_FIFO_RD`).
  - Wake up Core 1 from its deep sleep (WFE) state.
  - Core 0 runs main logic while Core 1 independently runs a separate task.
- [ ] **Milestone 5: Dynamic Voltage Scaling (VREG)**
  - Experiment with `VREG_AND_CHIP_RESET` (`0x40064000`) to test core voltage steps (`1.10V` -> `1.20V` -> `1.30V`).

---

## 6. User Directives & Decisions Log

*A chronological log of specific directives, architectural rules, and project directions given by the user:*

* **2026-10-21:** Decided to structure `blinky-rp` to match `timer_irqs` and `helloworld_make` 1-to-1 without external framework bloat.
* **2026-10-21:** Established that `plat_mem_init()` must be called at the very beginning of `main()` in C (matching `timer_irqs/src/main.c`), keeping `startup.S` minimal.
* **2026-10-21:** Decided not to clutter the `tools/` directory with full SDKs; keep only `arm-toolchain` and `picotool`.
* **2026-10-21:** Established two-file documentation policy:
  * `rp2040_project_tracker.md` (This file): User-directed roadmap, tracking, testing, and key conceptual points.
  * `rp2040_learning_notes.md`: Automatically updated technical reference for RP2040 hardware and bare-metal mechanics.
