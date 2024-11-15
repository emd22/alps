.globl _main
_main:
sub sp, sp, #16
mov w8, #0
str w8, [sp, #12]
mov w8, wzr
ldr w9, [sp, #12]
add w8, w8, w9
add w8, w8, #5
str w8, [sp, #12]
mov w8, wzr
add w8, w8, #10
ldr w9, [sp, #12]
add w8, w8, w9
str w8, [sp, #12]
ldr w0, [sp, #12]
add sp, sp, #16
ret
