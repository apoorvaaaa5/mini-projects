#pragma once
#define _ARCH_H_

static inline void arch_ei()		//Enable all system interrupt sources of CPU
{
    unsigned int bits = (1 << 3) | (1 << 7);
    asm volatile("csrs mstatus, %0" : : "r" (bits));
}

static inline void arch_ei_mtime()	//Enable timer interrupt
{
    unsigned int bits = (1 << 7);
    asm volatile("csrs mie, %0" : : "r" (bits));
}

static inline void arch_di_mtime()	//Disable timer interrupt
{
    unsigned int bits = (1 << 7);
    asm volatile("csrc mie, %0" : : "r" (bits));
}

static inline void arch_di()		//Disable all system interrupt
{
    unsigned int bits = (1 << 3) | (1 << 7);
    asm volatile("csrc mstatus, %0" : : "r" (bits));
    bits |= (1 << 11);
    asm volatile("csrc mie, %0" : : "r" (bits));

}

static inline void arch_wfi()		//Stalls the CPU until another interrupt comes
{
	asm volatile("wfi");	
}
