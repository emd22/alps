.globl _main
test:
stp fp, lr, [sp, -64]!
sub sp, sp, #16
mov w0, #7
add sp, sp, #16
ldp fp, lr, [sp], 64
ret
_main:
stp fp, lr, [sp, -64]!
sub sp, sp, #16
mov w8, wzr
bl test
mov w8, w0
str w8, [sp, #12]
mov w8, wzr
add w8, w8, #5
ldr w9, [sp, #12]
add w8, w8, w9
str w8, [sp, #8]
ldr w0, [sp, #8]
add sp, sp, #16
ldp fp, lr, [sp], 64
ret
