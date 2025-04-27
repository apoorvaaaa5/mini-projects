.section .vectors, "ax", "progbits"
vectors:
.org 0x00
.rept 32 
	j	.
.endr

.section .text.entry
.type entry, "function"
.global entry 
entry:
	la	t0, _stack_start
	mv	sp, t0
	jal	main
	j	.
	
