; Entry point for C kernel at 0x8000
[BITS 32]
[SECTION .text]

global _start
extern kernel_main
extern __bss_start
extern __bss_end

_start:
    ; Stack is already set up by bootloader at 0x90000
    
    ; Clear BSS section - zero out all uninitialized global variables
    ; This ensures all static/global variables start at 0
    mov edi, __bss_start    ; Destination = start of BSS
    mov ecx, __bss_end      ; Get end of BSS
    sub ecx, edi            ; Calculate size (end - start)
    xor eax, eax            ; Zero to write
    cld                     ; Clear direction flag (forward)
    rep stosb               ; Fill ECX bytes at [EDI] with AL (zero)
    
    ; Now call the C kernel with properly initialized BSS
    call kernel_main
    
    ; If kernel returns (it shouldn't), halt
    cli
.hang:
    hlt
    jmp .hang