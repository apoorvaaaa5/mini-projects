.syntax unified
.cpu cortex-m0plus
.thumb

.section .vectors, "a"
.global vectors
vectors:
    .word _stack_start          /* 0: Initial Stack Pointer */
    .word main                  /* 1: Reset Handler */
    .word panic_handler         /* 2: NMI */
    .word panic_handler         /* 3: HardFault */
    .rept 7
        .word panic_handler     /* 4-10: Reserved */
    .endr
    .word panic_handler         /* 11: SVCall */
    .word panic_handler         /* 12: Reserved */
    .word panic_handler         /* 13: Reserved */
    .word panic_handler         /* 14: PendSV */
    .word systick_isr           /* 15: SysTick Exception Handler */
    .rept 32
        .word panic_handler     /* 16-47: External Peripheral IRQs */
    .endr

.section .text.panic, "ax"
.global panic_handler
.type panic_handler, %function
panic_handler:
    cpsid   i                   /* Disable global interrupts */

    /* Determine which stack was active when the fault occurred */
    mov     r0, lr
    movs    r1, #4
    tst     r0, r1
    bne     1f
    mrs     r1, msp             /* Exception frame is on MSP */
    b       2f
1:
    mrs     r1, psp             /* Exception frame is on PSP */
2:
    /* r1 = pointer to hardware-stacked frame (r0-r3, r12, lr, pc, xpsr) */
    mrs     r0, ipsr            /* r0 = IPSR exception number */
    mov     r3, lr              /* r3 = EXC_RETURN */

    /* Switch SP to guaranteed safe top of MSP stack */
    ldr     r2, =_stack_start
    mov     sp, r2

    /* Call C crash diagnostics handler: c_panic_handler(ipsr, stacked_regs, exc_return) */
    bl      c_panic_handler
3:
    b       3b
