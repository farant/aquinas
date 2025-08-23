; Fixed bootloader that loads kernel to a simpler location
[BITS 16]
[ORG 0x7C00]

start:
    ; Setup segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    ; Load kernel from sector 2 to 0x8000 (simpler address)
    mov ax, 0x020E      ; Read 14 sectors (7KB)
    mov cx, 0x0002      ; Cylinder 0, Sector 2
    mov dx, 0x0000      ; Head 0, Drive 0
    mov bx, 0x8000      ; Load to 0x8000
    int 0x13
    jc error
    
    ; Switch to protected mode
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode

error:
    mov si, err_msg
.print:
    lodsb
    or al, al
    jz .halt
    mov ah, 0x0E
    int 0x10
    jmp .print
.halt:
    cli
    hlt
    
err_msg db 'Disk error!', 0

[BITS 32]
protected_mode:
    ; Setup segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    
    ; Jump to kernel at 0x8000
    jmp 0x8000

; GDT
align 8
gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF  ; Code segment
    dq 0x00CF92000000FFFF  ; Data segment
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510-($-$$) db 0
dw 0xAA55