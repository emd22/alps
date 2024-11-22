.text
.globl _main
.align 2
prints:
	stp FP, LR, [SP, -64]!
	sub SP, SP, #16
	str X0, [SP, #8]
	ldr X0, [SP, 8]
	bl _puts
	mov X0, #0
	add SP, SP, #16
	ldp FP, LR, [SP], 64
	ret
_main:
	stp FP, LR, [SP, -64]!
	sub SP, SP, #16
	adrp X8, .L.Str0@PAGE
	add X8, X8, .L.Str0@PAGEOFF
	str X8, [SP, #8]
	ldr X0, [SP, 8]
	bl prints
	mov X0, #0
	add SP, SP, #16
	ldp FP, LR, [SP], 64
	ret
.data
.L.Str0: .asciz "Hello"
