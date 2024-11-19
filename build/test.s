	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 15, 0	sdk_version 15, 0
	.globl	_main                           ; -- Begin function main
	.p2align	2
_main:                                  ; @main
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #80
	stp	x29, x30, [sp, #64]             ; 16-byte Folded Spill
	add	x29, sp, #64
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	mov	w8, #0                          ; =0x0
	str	w8, [sp, #28]                   ; 4-byte Folded Spill
	stur	wzr, [x29, #-4]
	adrp	x8, l_.str@PAGE
	add	x8, x8, l_.str@PAGEOFF
	stur	x8, [x29, #-16]
	adrp	x8, l_.str.1@PAGE
	add	x8, x8, l_.str.1@PAGEOFF
	stur	x8, [x29, #-24]
	mov	x8, #10                         ; =0xa
	str	x8, [sp, #32]
	ldur	x0, [x29, #-16]
	ldur	x10, [x29, #-24]
	ldr	x8, [sp, #32]
	mov	x9, sp
	str	x10, [x9]
	str	x8, [x9, #8]
	bl	_printf
	ldr	w0, [sp, #28]                   ; 4-byte Folded Reload
	ldp	x29, x30, [sp, #64]             ; 16-byte Folded Reload
	add	sp, sp, #80
	ret
	.cfi_endproc
                                        ; -- End function
	.section	__TEXT,__cstring,cstring_literals
l_.str:                                 ; @.str
	.asciz	"Hello, %s %lu\n"

l_.str.1:                               ; @.str.1
	.asciz	"Ethan"

.subsections_via_symbols
