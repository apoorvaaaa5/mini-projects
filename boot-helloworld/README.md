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
- Cyancore Platform: Cyancore is an open source unified software platform for embedded system projects. It  enables a developer/user to write a portable project which can run on any of the target. It is designed to be a goto framework for almost all projects, be it an Embedded applications, IoT, firmware, OS, etc. For more information click below. 
[CyanCore GitHub Repository](https://github.com/VisorFolks/cyancore)

## Let us look into the required files:

### 1. main.c 
[main.c](https://github.com/apoorvaaaa5/mini-projects/blob/main/boot-helloworld/main.c)

#### Let's understand line by line:
- `#define mmio32(x)`: This defines macro called mmio32(x).
- `(volatile unsigned int *) x` : 
* We mean that x is the address of a 32 bit unsigned interger that might change to any value during compile time hence we give the word volatile. 
* Then we use `* (...)` to derefernce that pointer to access the value from the address i.e 32 bit unsigned integer.
- `static void putc(char c)` : 
```Inside:
mmio32(0x20000) = c;
```
* This defines a private function that writes the value of character `c` into the memory-mapped register at `0x20000`.
-  `static void puts(char* rs)` :
```Inside:
while (*rs)
{
    putc(*rs);
    rs++;
}
```
* This is basic `puts()` function. 
* It takes  a pointer to a C string `(char* rs)` — a sequence of characters ending with `\0`.
* It loops through each character:
    * Sends it using putc()
    * Moves the pointer forward (rs++)
* Stops when it hits the null terminator `(\0)`, which is how C knows the string is done.

- `static void stopsim()`:
``` Inside:
mmio32(0x20008) = 1;
```
* It writes 1 to address 0x20008.
* It signals the simulator that “the program is done” — like ending the simulation run or halting hardware.

- `int main()` :
This is entry point of C program.

``` c
puts("Hello World\n");
```
* Calls puts() function which internally calls putc() function to write string output one character at a time.

```c
stopsim();
return 0;
```
Ends the program and returns 0 which indicates program is successfully completed.

### 1. startup.S
[startup.S](https://github.com/apoorvaaaa5/mini-projects/blob/main/boot-helloworld/startup.S)






