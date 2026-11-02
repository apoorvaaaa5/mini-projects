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
    b   .
