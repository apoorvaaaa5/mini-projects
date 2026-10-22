.section .vectors, "a"
.global vectors
vectors:
    .word _stack_start   /* Vector 0: Initial Stack Pointer (Hardware loads SP!) */
    .word main          /* Vector 1: Reset Handler (Jumps directly to C main!) */
    .rept 46
        .word panic_handler
    .endr

.section .text.panic, "ax"
.global panic_handler
.type panic_handler, %function
panic_handler:
    b   .
