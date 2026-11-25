#include <stdint.h>
#include <console.h>
#include <usb_cdc.h>
#include <scheduler.h>

static const char *get_exception_name(uint32_t ipsr)
{
	switch (ipsr) {
	case 2:  return "NMI (Non-Maskable Interrupt)";
	case 3:  return "HardFault (Bus Error / Illegal Instruction / Memory Violation)";
	case 11: return "SVCall (Supervisor Call)";
	case 14: return "PendSV";
	case 15: return "SysTick";
	default:
		if (ipsr >= 16) return "External Peripheral IRQ";
		return "Reserved Exception";
	}
}

/*
 * C Panic & Crash Diagnostics Handler:
 * Invoked directly by startup.S when an unhandled exception or HardFault fires.
 */
void c_panic_handler(uint32_t ipsr, uint32_t *stacked_regs, uint32_t exc_return)
{
	uint32_t current_msp, current_psp;
	asm volatile("mrs %0, msp" : "=r"(current_msp));
	asm volatile("mrs %0, psp" : "=r"(current_psp));

	/* Ensure USB CDC is notified and unflushed buffers are pushed */
	usb_poll();

	console_puts("\r\n\r\n");
	console_puts("================================================================================\r\n");
	console_puts("                    *** SYSTEM PANIC / HARDFAULT DETECTED ***                   \r\n");
	console_puts("================================================================================\r\n");

	console_puts("Exception Reason : ");
	console_puts(get_exception_name(ipsr));
	console_puts(" [IPSR = ");
	console_print_dec(ipsr);
	console_puts("]\r\n");

	if (current_task != NULL) {
		console_puts("Faulting Task    : '");
		console_puts(current_task->name);
		console_puts("' (PID ");
		console_print_dec(current_task->pid);
		console_puts(")\r\n");
	} else {
		console_puts("Faulting Task    : NONE (Kernel Bootstrap / ISR Context)\r\n");
	}

	console_puts("EXC_RETURN       : ");
	console_print_hex(exc_return);
	if (exc_return & 4) {
		console_puts(" (Exception returned from Thread Mode using PSP)\r\n");
	} else {
		console_puts(" (Exception returned from Handler/MSP Mode)\r\n");
	}

	console_puts("MSP Stack Pointer: ");
	console_print_hex(current_msp);
	console_puts(" | PSP Stack Pointer: ");
	console_print_hex(current_psp);
	console_puts("\r\n");

	/* Check if stacked_regs is inside valid SRAM range (0x20000000 - 0x20042000) */
	uint32_t sp_addr = (uint32_t)stacked_regs;
	if (sp_addr < 0x20000000 || sp_addr > 0x20042000) {
		console_puts("\r\n[CRITICAL WARNING] Stack Pointer out of bounds: ");
		console_print_hex(sp_addr);
		console_puts("\r\nProbable Cause   : STACK OVERFLOW! Task stack exceeded allocation boundary.\r\n");
	} else {
		uint32_t r0  = stacked_regs[0];
		uint32_t r1  = stacked_regs[1];
		uint32_t r2  = stacked_regs[2];
		uint32_t r3  = stacked_regs[3];
		uint32_t r12 = stacked_regs[4];
		uint32_t lr  = stacked_regs[5];
		uint32_t pc  = stacked_regs[6];
		uint32_t psr = stacked_regs[7];

		console_puts("\r\n--- Exception Context Frame ---\r\n");
		console_puts("Faulting PC (Address of Crash) : ");
		console_print_hex(pc);
		console_puts("\r\nReturn LR   (Caller of Crash)  : ");
		console_print_hex(lr);
		console_puts("\r\nxPSR        (Processor State)  : ");
		console_print_hex(psr);
		console_puts("\r\n\r\n--- CPU Register Dump ---\r\n");

		console_puts("R0 : "); console_print_hex(r0);
		console_puts("  R1 : "); console_print_hex(r1);
		console_puts("  R2 : "); console_print_hex(r2);
		console_puts("  R3 : "); console_print_hex(r3);
		console_puts("\r\nR12: "); console_print_hex(r12);
		console_puts("  SP : "); console_print_hex(sp_addr);
		console_puts("\r\n");

		console_puts("\r\n--- Stack Memory Dump (at SP) ---\r\n");
		for (int i = 0; i < 8; i++) {
			console_print_hex(sp_addr + (i * 4));
			console_puts(": ");
			console_print_hex(stacked_regs[i]);
			console_puts("\r\n");
		}
	}

	console_puts("================================================================================\r\n");
	console_puts("System halted. USB CDC polling kept alive for diagnostics.\r\n\r\n");

	/* Keep polling USB indefinitely so host serial connection does not terminate */
	while (1) {
		usb_poll();
	}
}
