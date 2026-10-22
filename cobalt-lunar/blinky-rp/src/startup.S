.section .vectors, "a"
.align 2
.global vectors
vectors:
    .word _stack_start   /* Vector 0: Initial Stack Pointer */
    .word entry          /* Vector 1: Reset Entry Point */
    .rept 46
        .word panic_handler
    .endr

.section .text.entry, "ax"
.global entry
.type entry, %function
entry:
    ldr     r0, =_stack_start
    mov     sp, r0
    bl      main
    b       .

.global panic_handler
.type panic_handler, %function
panic_handler:
    b       .
