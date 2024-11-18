.globl _main
printchar:
stp fp, lr, [sp, -64]!
sub sp, sp, #16
str w0, [sp, #12]
str w1, [sp, #8]
ldr w9, [sp, #12]
mov w8, w9
ldr w9, [sp, #8]
add w8, w8, w9
mov w0, w8
bl _putchar
mov w0, #0
add sp, sp, #16
ldp fp, lr, [sp], 64
ret
_main:
stp fp, lr, [sp, -64]!
sub sp, sp, #16
mov w0, #65
mov w1, #0
bl printchar
mov w0, #0
add sp, sp, #16
ldp fp, lr, [sp], 64
ret
