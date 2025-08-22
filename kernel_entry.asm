; kernel_entry.asm - Entry point for the kernel
[BITS 64]
[SECTION .text]

global _start
extern kernel_main

_start:
    ; Set up stack
    mov rsp, stack_top
    
    ; Clear registers
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    ; Call kernel main
    call kernel_main
    
    ; Halt if kernel returns
.halt:
    cli
    hlt
    jmp .halt

[SECTION .bss]
resb 8192               ; 8KB stack
stack_top:
