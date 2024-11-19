.text
.globl _main
.align 2
_main:
stp FP, LR, [SP, -64]!
sub SP, SP, #16
mov X8, #10
str X8, [SP, #8]
