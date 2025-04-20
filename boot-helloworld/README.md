# Bare-Metal Hello World on RISC-V (Ibex)

This project demonstrates how to boot a bare-metal Hello World program on a RISC-V processor, specifically using the Ibex core and the Cyancore platform.

## What is "Booting on Bare Metal" mean?
"Booting on Bare Metal" means running a program directly on the hardware — without any operating system (OS), runtime, or abstraction layers like Linux, Windows.

It doesn't utilise OS to:
- Manage memory
- Handle input/output
- Schedule tasks or processes
So the program does everything from scratch which is setting up the stack, defining memory, handle interrupts and performing input/output operations by reading and writing to designated memory-mapped registers(MMIO)

## Project Structure
- `helloworld.elf`: The compiled ELF executable.
- `helloworld.elf.lst`: Readable(text) form of the ELF file.
- `startup.S`: Startup assembly: sets stack and calls main.
- `main.c`: Hello World code in C.
- `link.ld`: Linker script: places sections like .text, .data, .stack
- `main.o`, `startup.o`: Object files of the same.

## Required tools/source code
- RISC-V Toolchain (riscv64-unknown-elf)
- Ibex CPU core
- Cyancore Platform: Cyancore is an open source unified software platform for embedded system projects. It  enables a developer/user to write a portable project which can run on any of the target. It is designed to be a goto framework for almost all projects, be it an Embedded applications, IoT, firmware, OS, etc. For more information click on this button. 
[CyanCore GitHub Repository](https://github.com/VisorFolks/cyancore)

## Let us look into the required files:

### 1. main.c 
[main.c](https://github.com/apoorvaaaa5/mini-projects/blob/main/boot-helloworld/main.c)

#### Let's understand line by line:
- `#define mmio32(x)`: This defines macro called mmio32(x).
- `(volatile unsigned int *) x` : 
    - We mean that x is the address of a 32 bit unsigned interger that might change to any value during compile time hence we give the word volatile. 
    - Then we use `* (...)` to derefernce that pointer to access the value from the address i.e 32 bit unsigned integer.
- `static void putc(char c)` : 
```Inside:
mmio32(0x20000) = c;
```
This defines a private function that writes the value of character `c` into the memory-mapped register at `0x20000`.

-  `static void puts(char* rs)` :
```Inside:
while (*rs)
{
    putc(*rs);
    rs++;
}
```
  - This is basic `puts()` function. 
  - It takes  a pointer to a C string `(char* rs)` — a sequence of characters ending with `\0`.
  - It loops through each character:
    * Sends it using putc()
    * Moves the pointer forward (rs++)
  - Stops when it hits the null terminator `(\0)`, which is how C knows the string is done.

- `static void stopsim()`:
``` Inside:
mmio32(0x20008) = 1;
```
    - It writes 1 to address 0x20008.
    - It signals the simulator that “the program is done” — like ending the simulation run or halting hardware.

- `int main()` :
This is entry point of C program.

``` c
puts("Hello World\n");
```
Calls puts() function which internally calls putc() function to write string output one character at a time.

```c
stopsim();
return 0;
```
Ends the program and returns 0 which indicates program is successfully completed.

### 2. startup.S
[startup.S](https://github.com/apoorvaaaa5/mini-projects/blob/main/boot-helloworld/startup.S)

Below is a detailed explanation of `startup.S` file and how it connects with `main.c`. Will be mostly explaining about *interrupt handling* and *entry point*

The `startup.S` file acts as the very first code that executes when RISC-V processor `powers on or resets`. It's bare-metal boot sequence-- meaning there's no OS involved to do the same. This code does two kinda important tasks:

1. Interrupt Vector Table: `.section` `.vectors`
```asm
.section .vectors, "ax", "progbits"
vectors:
.org 0x00
.rept 32
    j .
.endr
```
- `.section .vectors`: This creates and places a block of code in a dedicated section called `.vectors`
- `.org 0x00` : Starts placing the next instrcutions at *memory address 0x0*.
- `.rept 32` / `j .` / `.endr`:
    - This repeats the instruction `j .` (jump to self) 32 times. This creates 32 interrupt vector entries.
    - So this works as a *default interrupt handler table*, ensuring safe behavior if an unexpected interrupt occurs before setup.

2. Program Entry Point: `.section .text.entry`
```asm
.section .text.entry
.type entry, "function"
.global entry
entry:
    la      t0, _stack_start
    mv      sp, t0
    jal     main
    j       .
```
- `.global entry`: Makes the entry label visible to the linker — this becomes the program's entry point.
- `la t0, _stack_start`: Loads the address of the top of the stack into register `t0`. This _stack_start is typically defined in your linker script.

- `mv sp, t0`: Sets up the stack pointer using the value in `t0`, which is crucial for function calls and local variables.

- `jal main`: Jumps to the main() function in your C code — this is where your actual program logic starts (like printing "Hello World").

- `j .`: An infinite loop after main returns. Since there's no OS to return to, we stall the CPU here safely.

So, in summary we can tell that this file sets up a safe boot environment, configuring interrupt vector table and preparing stack before trasferring control to C program via main().

### 3. link.ld: The linker script
When you compile your code (`startup.S`, `main.c`), the compiler turns them into object files (`.o`). The linker then combines them into a single executable (`helloworld.elf`).
The script link.ld tells where should the code go in memory?

#### 1. Memory Block:
```ld
MEMORY
{
    mem : ORIGIN = 0X100000, LENGTH = 0X100000
}
```
This tells the linker:

  - We have a memory region named `mem`
  - It starts at address `0x100000` (this is the starting address of RAM for Ibex-core)
  - The length is `0x100000` which means the size of *RAM is 1MB*

So, from the memory `0x100000` to `0x199999` is where all instruction code and data will reside.

#### 2. SECTIONS Block
```ld
.text :
{
    *(.vectors)
    *(.text.entry)
    *(.text.*)
} > mem
```
- `.text` section is where executable code (instructions) go.

- `*(.vectors)` → grabs your interrupt vector from `startup.S`:

    - `vectors`: was placed inside .section `.vectors`

    - This gets mapped to the start of `.text`, hence address `0x100000`.

- `*(.text.entry)` → includes your entry: label (boot/startup code).

- `*(.text.*)` → includes all normal C functions, like `main()` from `main.c`.

```ld
.rodata :
{
    *(.rodata.*)
} > mem
```
- `.rodata` means Read-Only Data. This is where string literals go. `For example: puts("Hello World\n");`. 
The `"Hello World\n"` string is stored here — in memory it doesn't get modified hence the name `Read-Only Memory`.

```ld
.stack :
{
    *(.stack)
    . = . + 100;
    PROVIDE(_stack_start = .);
} > mem
```
Stack is used by functions to store:

  - local variables,

  - return addresses,

  - and temporary data.

Line-by-line:

  - `*(.stack)` → collects anything tagged as stack (often empty)

  - `. = . + 100;` → Reserves 100 bytes for the stack

  - `PROVIDE(_stack_start = .);` → Tells the linker to define a label `_stack_start` at this point

This label is used in startup.S:
```asm
la t0, _stack_start
mv sp, t0
```

This sets up the stack pointer so your C functions (like main) can work properly.

## Steps to run:
Firstly, install and build [Cyancore](https://github.com/VisorFolks/cyancore/wiki/Getting-Started). Then run the below commands:
```bash
# 1. Assemble startup
riscv64-unknown-elf-as startup.S -o startup.o -march=rv32i -mabi=ilp32

# 2. Compile C code
riscv64-unknown-elf-gcc -c main.c -o main.o -march=rv32i -mabi=ilp32 -nostdlib -nostdinc -fno-builtin

# 3. Link with linker script
riscv64-unknown-elf-ld main.o startup.o -o helloworld.elf -T link.ld -b elf32-littleriscv -nostdlib

# 4. Disassemble (optional, to inspect ELF)
riscv64-unknown-elf-objdump -Dx helloworld.elf > helloworld.elf.lst

# 5. Convert to binary/hex if needed
riscv64-unknown-elf-objcopy -O binary helloworld.elf helloworld.bin
riscv64-unknown-elf-objcopy -O ihex helloworld.elf helloworld.hex
```

## Summary of readable executable file
The `helloworld.elf.lst` script is the disassembled output of a RISC-V ELF executable built from `main.c`, `startup.S`, and `link.ld`. It shows how the compiled program is laid out in memory, starting at address `0x00100000` as defined by the linker. The `.text` section contains executable code, beginning with the vectors in `startup.S`, which loops indefinitely as a placeholder. Then comes the entry routine that sets the stack pointer using the `_stack_start` symbol and jumps to the main function in `main.c`. The main function prints `"Hello World"` using a `puts function`, which internally calls putc to send each character to a memory-mapped I/O address `(0x20000)`. After printing, it calls `stopsim`, which signals the end of simulation by writing to `0x20008`. The `.rodata` section holds the string literal, and the `.stack `section reserves space for the stack. This script provides a low-level view of how a simple C program is bootstrapped and executed on a RISC-V system.

## What I learnt?
- How C programs get mapped to memory
- How a stack is initialized manually
- What happens before `main()` is called
- How to write and link bare-metal code with no `libc`
- How `puts()` and `putc()` can be implemented with MMIO
