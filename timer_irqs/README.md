# Bare-Metal Timer Interrupt on RISC-V (Ibex)

This project demonstrates how to implement timer-based interrupts in a bare-metal RISC-V environment, specifically using the Ibex core.

## What does "Bare-Metal Timer Interrupt" mean?

It means handling hardware timer interrupts without an operating system (OS). The program:

- Directly configures hardware timer registers (mtime, mtimecmp)
- Enables and disables machine-level interrupts using Control and Status Registers (CSRs)
- Defines and handles Interrupt Service Routine (ISR) in C without an OS or interrupt manager

The code fully controls:
- Timer initialization
- Interrupt enabling/disabling
- ISR execution and acknowledgment

## Project Structure

```
timer_irq.elf          # Compiled ELF executable for the timer interrupt demo
timer_irq.elf.lst      # Disassembly (human-readable form) of the ELF file
startup.S              # Startup assembly: sets up stack pointer and jumps to main
main.c                 # Main application that configures CLINT timer
irq.c                  # Machine Timer Interrupt Service Routine (ISR)
platform.h             # Platform abstraction header
platform.c             # Platform-specific routines implementation
link.ld                # Linker script: defines memory layout
*.o                    # Object files for respective source files
```

## Key Files

### 1. main.c

This is the entry point of the program. It:
- Disables all interrupts
- Initializes memory and CPU
- Starts a one-shot timer
- Waits for the interrupt to set test_status

```c
int test_status;

int main() {
    arch_di();                // Disable all interrupts
    plat_mem_init();          // Clear BSS section
    plat_cpu_init();          // Enable global interrupts
    timer_one_shot(1000);     // Set timer for 1000 ticks

    puts("Hello world\n");

    for (int i = 0; i < 10000; i++); // Busy-wait loop

    if (test_status)
        puts("Timer test success!!\n");
    else
        puts("Timer test failed!!\n");

    plat_stop_sim();          // End simulation
    return 0;
}
```

Key Points:
- timer_one_shot(1000) schedules a timer interrupt after 1000 ticks
- After the loop, it checks test_status. If ISR ran, test_status will be 1

### 2. timer.c

Defines timer_one_shot() and timer_isr().

#### timer_one_shot()
```c
void timer_one_shot(unsigned int ticks) {
    unsigned long long timer;
    timer = plat_clint_timer_read();      // Read current time
    timer += (unsigned long long) ticks;  // Add interval
    plat_clint_timer_compare(timer);      // Set mtimecmp
    arch_ei_mtime();                      // Enable timer interrupt
}
```

Steps:
1. Read current CLINT mtime (64-bit)
2. Add the delay (ticks) to schedule next interrupt
3. Write to mtimecmp to trigger interrupt at that time
4. Enable timer interrupt via CSR (mie register)

#### timer_isr() (Interrupt Service Routine)
```c
void timer_isr() {
    extern int test_status;
    arch_di_mtime(); // Disable further timer interrupts
    test_status = 1; // Mark interrupt handled
    puts("\nThis is timer interrupttttt\n");
}
```

What happens here:
1. Disable timer interrupts to prevent re-entry
2. Set global test_status = 1
3. Print a message

### 3. platform.c / platform.h

Handles low-level MMIO operations and basic platform services.

Important Functions:
- `plat_clint_timer_read()` - Safely reads 64-bit mtime using loop to avoid overflow issue
- `plat_clint_timer_compare()` - Writes mtimecmp (low part first, then high) to avoid early interrupts
- `plat_ascii_out()` - Writes characters to UART output register at 0x20000
- `plat_stop_sim()` - Writes 1 to 0x20008 to signal simulation stop
- `plat_mem_init()` - Clears .bss section

### 4. arch.h

Provides functions for interrupt control at CSR level:
- `arch_ei()` - Enable global interrupts (mstatus.MIE)
- `arch_di()` - Disable global interrupts
- `arch_ei_mtime()` - Enable timer interrupt in mie
- `arch_di_mtime()` - Disable timer interrupt
- `arch_wfi()` - Wait For Interrupt (halts CPU until interrupt)

Example:
```c
static inline void arch_ei_mtime() {
    unsigned int bits = (1 << 7); // MTIE bit
    asm volatile("csrs mie, %0" : : "r" (bits));
}
```

## How It Works

1. main() calls timer_one_shot(1000)

2. timer_one_shot():
   - Reads current time (mtime)
   - Calculates next interrupt time
   - Sets mtimecmp
   - Enables timer interrupt

3. CPU runs main program until interrupt occurs

4. When mtime >= mtimecmp, interrupt triggers and timer_isr() executes:
   - Disables timer interrupt
   - Updates test_status
   - Prints ISR message

5. After main loop, program checks test_status:
   - If 1 → "Timer test success!!"
   - Else → "Timer test failed!!"

## Flow Diagram

```
main() 
   ↓
timer_one_shot(1000) → set mtimecmp → enable MTIE
   ↓
CPU runs main loop
   ↓
mtime reaches mtimecmp → interrupt occurs
   ↓
timer_isr() → test_status = 1
   ↓
main() checks status → prints result
```

## Expected Output

```
Hello world

This is timer interrupttttt

Timer test success!!
```

## What You Will Learn

- How RISC-V machine timer works (mtime and mtimecmp)
- How to enable and disable interrupts using CSRs
- How to write an ISR in C without an OS
- How to safely read 64-bit timer registers in a 32-bit system
- Bare-metal programming concepts