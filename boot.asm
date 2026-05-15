; Multiboot header
section .multiboot
align 4
    dd 0x1BADB002
    dd 0x03
    dd -(0x1BADB002 + 0x03)

section .text
global _start
_start:
    mov esp, stack_top
    push eax
    push ebx
    extern kernel_main
    call kernel_main
    cli
.halt:
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
