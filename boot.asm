; boot.asm - First stage bootloader
; Loads at 0x7C00, switches to long mode, loads kernel

[BITS 16]
[ORG 0x7C00]

start:
    ; Clear segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    ; Clear screen
    mov ax, 0x0003
    int 0x10
    
    ; Enable A20 line
    in al, 0x92
    or al, 2
    out 0x92, al
    
    ; Load kernel from disk (sectors 2-64) to 0x8000
    mov ah, 0x02        ; Read sectors
    mov al, 63          ; Number of sectors (increase to load larger kernel)
    mov ch, 0           ; Cylinder 0
    mov cl, 2           ; Start from sector 2
    mov dh, 0           ; Head 0
    mov bx, 0x8000      ; Load to 0x8000
    int 0x13
    jc disk_error
    
    ; Check for long mode support
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb no_long_mode
    
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz no_long_mode
    
    ; Set up page tables at 0x1000
    mov edi, 0x1000
    mov cr3, edi
    xor eax, eax
    mov ecx, 4096
    rep stosd
    mov edi, cr3
    
    ; PML4[0] -> PDP
    mov dword [edi], 0x2003
    add edi, 0x1000
    
    ; PDP[0] -> PD
    mov dword [edi], 0x3003
    add edi, 0x1000
    
    ; PD[0] -> PT
    mov dword [edi], 0x4003
    add edi, 0x1000
    
    ; Identity map first 2MB
    mov ebx, 0x00000003
    mov ecx, 512
.set_entry:
    mov dword [edi], ebx
    add ebx, 0x1000
    add edi, 8
    loop .set_entry
    
    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    ; Set long mode bit
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Jump to long mode
    jmp 0x08:long_mode_start
    
disk_error:
    mov si, disk_err_msg
    call print_string_16
    jmp $
    
no_long_mode:
    mov si, no_lm_msg
    call print_string_16
    jmp $
    
print_string_16:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string_16
.done:
    ret
    
disk_err_msg db 'Disk read error!', 0
no_lm_msg db 'Long mode not supported!', 0

[BITS 64]
long_mode_start:
    ; Set up segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov rsp, 0x7C00
    
    ; Jump to kernel at 0x8000
    mov rax, 0x8000
    jmp rax

; GDT
gdt_start:
    dq 0x0000000000000000  ; Null descriptor
    dq 0x00209A0000000000  ; Code segment
    dq 0x0000920000000000  ; Data segment
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; Boot signature
times 510-($-$$) db 0
dw 0xAA55
