; Entry point for C kernel at 0x8000
[BITS 32]
[SECTION .text]

global _start
extern kernel_main

_start:
    ; Stack is already set up by bootloader at 0x90000
    ; Just call the C kernel
    call kernel_main
    
    ; If kernel returns (it shouldn't), halt
    cli
.hang:
    hlt
    jmp .hang