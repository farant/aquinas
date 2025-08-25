; Interrupt handler stubs
; Saves registers, calls C handler, restores registers

[bits 32]
section .text

global timer_interrupt_stub
global default_interrupt_stub
extern timer_handler
extern default_handler

timer_interrupt_stub:
    ; Save all registers
    pushad
    
    ; Save segment registers
    push ds
    push es
    push fs
    push gs
    
    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call C handler
    call timer_handler
    
    ; Restore segment registers
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Restore all registers
    popad
    
    ; Return from interrupt
    iret

; We need individual stubs for each interrupt to know which one fired
; For now, use a simple version that doesn't track interrupt number
default_interrupt_stub:
    ; Save all registers
    pushad
    
    ; Save segment registers
    push ds
    push es
    push fs
    push gs
    
    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Pass interrupt number (0xFF = unknown for now)
    push dword 0xFF
    call default_handler
    add esp, 4
    
    ; Restore segment registers
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Restore all registers
    popad
    
    ; Return from interrupt
    iret