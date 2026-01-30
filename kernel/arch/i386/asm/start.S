.section .text
.global _start
.type _start, @function

_start:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    movl $stack_top, %esp

    call kernel_main

    cli
1:  hlt
    jmp 1b
.size _start, . - _start
