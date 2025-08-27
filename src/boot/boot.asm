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
    
    ; Load kernel using extended BIOS functions (LBA mode)
    ; Load first 32KB (64 sectors) to 0x8000
    mov si, dap1        ; Point to first Disk Address Packet
    mov ah, 0x42        ; Extended Read
    mov dl, 0x80        ; Drive 0x80
    int 0x13
    jc error
    
    ; Load next 32KB (64 sectors) to 0x10000 (segment 0x1000:0x0000)
    mov si, dap2        ; Point to second Disk Address Packet
    mov ah, 0x42        ; Extended Read
    mov dl, 0x80        ; Drive 0x80
    int 0x13
    jc error
    
    ; Load final 24KB (48 sectors) to 0x18000 (segment 0x1800:0x0000)
    mov si, dap3        ; Point to third Disk Address Packet
    mov ah, 0x42        ; Extended Read
    mov dl, 0x80        ; Drive 0x80
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
    
    ; Enable A20 line for access to memory above 1MB
    ; Using Fast A20 Gate method (port 0x92)
    in al, 0x92
    or al, 2        ; Set bit 1 (A20 enable)
    out 0x92, al
    
    ; Set stack pointer to 2MB (in extended memory)
    ; Stack grows downward from here, giving us nearly 2MB of stack space
    mov esp, 0x200000
    
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

; Disk Address Packets for LBA read
align 4
dap1:
    db 0x10             ; Size of packet (16 bytes)
    db 0                ; Reserved (0)
    dw 64               ; Number of sectors to read (32KB)
    dw 0x8000           ; Offset to load to
    dw 0x0000           ; Segment to load to
    dd 1                ; Starting LBA (sector 1, after boot sector)
    dd 0                ; Upper 32-bits of LBA (0 for disks < 2TB)

dap2:
    db 0x10             ; Size of packet (16 bytes)
    db 0                ; Reserved (0)
    dw 64               ; Number of sectors to read (32KB)
    dw 0x0000           ; Offset to load to
    dw 0x1000           ; Segment to load to (0x1000:0x0000 = physical 0x10000)
    dd 65               ; Starting LBA (sector 65, after first 64 sectors)
    dd 0                ; Upper 32-bits of LBA (0 for disks < 2TB)

dap3:
    db 0x10             ; Size of packet (16 bytes)
    db 0                ; Reserved (0)
    dw 48               ; Number of sectors to read (24KB)
    dw 0x0000           ; Offset to load to
    dw 0x1800           ; Segment to load to (0x1800:0x0000 = physical 0x18000)
    dd 129              ; Starting LBA (sector 129, after first 128 sectors)
    dd 0                ; Upper 32-bits of LBA (0 for disks < 2TB)

times 510-($-$$) db 0
dw 0xAA55