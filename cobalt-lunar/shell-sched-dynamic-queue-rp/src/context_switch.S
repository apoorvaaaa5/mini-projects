.syntax unified
.cpu cortex-m0plus
.thumb

.global systick_isr
.type systick_isr, %function
systick_isr:
    /* -------------------------------------------------------------
     * SysTick Periodic Timer Interrupt (1 kHz / 1 ms tick):
     * Always increments system_millis.
     * ------------------------------------------------------------- */
    ldr     r0, =system_millis
    ldr     r1, [r0]
    adds    r1, #1
    str     r1, [r0]

    /* Check EXC_RETURN in LR: bit 2 indicates MSP (0 = bare-metal) vs PSP (1 = OS task) */
    mov     r0, lr
    movs    r1, #4
    tst     r0, r1
    beq     .Lsystick_done

    /* Running on PSP (OS mode): Trigger PendSV for round-robin preemption */
    ldr     r0, =0xE000ED04    /* ICSR (Interrupt Control and State Register) */
    ldr     r1, =(1 << 28)     /* PENDSVSET */
    str     r1, [r0]

.Lsystick_done:
    bx      lr

.global pendsv_isr
.type pendsv_isr, %function
pendsv_isr:
    /* -------------------------------------------------------------
     * PendSV Exception: Performs Context Switching
     * Triggered by:
     *   1) systick_isr (preemptive round-robin time slice expiration)
     *   2) task_yield() (cooperative voluntary slice release)
     * ------------------------------------------------------------- */
    /* 1. Read current task's Process Stack Pointer (PSP) */
    mrs     r0, psp

    /* 2. Save software registers R4-R11 onto task stack */
    subs    r0, #32         /* Allocate 32 bytes (8 words) */
    mov     r1, r0
    stmia   r1!, {r4-r7}    /* Save R4-R7 */
    mov     r4, r8
    mov     r5, r9
    mov     r6, r10
    mov     r7, r11
    stmia   r1!, {r4-r7}    /* Save R8-R11 */

    /* 3. Call C schedule_next(current_sp in r0)
     *    Returns next task's stack_pointer_end directly in r0!
     *    push {r3, lr} ensures 8-byte stack alignment on MSP for AAPCS.
     */
    push    {r3, lr}
    bl      schedule_next
    pop     {r2, r3}

    /* 4. Restore R8-R11 from next task's stack (pointed to by r0) */
    mov     r1, r0
    adds    r1, #16
    ldmia   r1!, {r4-r7}
    mov     r8, r4
    mov     r9, r5
    mov     r10, r6
    mov     r11, r7

    /* 5. Restore R4-R7 from next task's stack */
    mov     r1, r0
    ldmia   r1!, {r4-r7}

    /* 6. Point PSP to the hardware frame */
    adds    r0, #32
    msr     psp, r0

    /* 7. Return from exception using PSP (0xFFFFFFFD) */
    ldr     r0, =0xFFFFFFFD
    bx      r0

.global start_first_task
.type start_first_task, %function
start_first_task:
    /* r0 = task[0].stack_pointer_begin (top of clean stack)
     * r1 = task[0].fnctpt (task entry function)
     */
    msr     psp, r0         /* Point PSP to Task 0 stack */
    movs    r2, #2
    msr     control, r2     /* Switch Thread mode to use PSP: CONTROL[1] = 1 */
    isb                     /* Flush instruction pipeline */
    cpsie   i               /* Enable global interrupts */
    bx      r1              /* Jump directly to Task 0 entry point! */
