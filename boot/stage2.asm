BITS 16
ORG 0x7E00


stage2_start:
  cli

  mov si, msg_stage2
  call print_string

  mov ah, 0x02        ; AH = 0x02 -> BIOS function: Read Sectors From Drive
  mov al, 100         ; AL = 100 -> number of sectors to read
  mov ch, 0           ; CH = 0 -> cylinder number (track)
  mov cl, 10          ; CL = 10 -> sector number (starts at 1)
  mov dh, 0           ; DH = 0 -> head number
  mov dl, 0           ; DL = 0 -> drive number (0 = first floppy/hard disk)
  mov bx, 0x1000      ; BX = 0x1000 -> memory offset to load the kernel (ES:BX)
  mov es, bx          ; ES = 0x1000 -> set Extra Segment to 0x1000
  xor bx, bx          ; ES:BX = 0x10000
  int 0x13            ; call BIOS interrupt 0x13 to read sectors
  jc disk_error_stage2 ; jump to error handler if carry flag is set (read failed)

  call enable_a20  ; Enable A20 line to access memory above 1MB
  lgdt [gdt_descriptor] ; Load the Global Descriptor Table
  mov eax, cr0  ; Get the current value of CR0
  or eax, 1     ; Set the PE (Protection Enable) bit0x0D, 0x0A,
  mov cr0, eax  ; Update CR0 to enable protected mode
  jmp 0x08:protected_mode ; Far jump to flush the prefetch queue and enter protected mode

enable_a20:
  in al, 0x92 ; Read port 0x92
  or al, 2    ; Set bit 1 to enable A20
  out 0x92, al ; Write back to port 0x92
  jmp $+2
  jmp $+2
  ret

print_string:
  lodsb
  or al, al
  jz .done
  mov ah, 0x0E
  int 0x10
  jmp print_string
.done:
  ret

err_msg_stage2 db 'Error reading kernel from disk', 0x0D, 0x0A, 0

msg_stage2 db "Loading kernel...", 0x0D, 0x0A, 0

disk_error_stage2:
  mov si, err_msg_stage2
  call print_string
  jmp $

gdt_null:
  dq 0 ; Null descriptor

gdt_code:
  dw 0xFFFF ; Limit low (0xFFFF = 4GB)
  dw 0      ; Base low
  db 0      ; Base middle
  db 0x9A   ; Access byte: present, ring 0, code segment, executable, readable
  db 0xCF   ; Flags and limit high: granularity, 32-bit
  db 0      ; Base high

gdt_data:
  dw 0xFFFF ; Limit low (0xFFFF = 4GB)
  dw 0      ; Base low
  db 0      ; Base middle
  db 0x92   ; Access byte: present, ring 0, data segment, writable
  db 0xCF   ; Flags and limit high: granularity, 32-bit
  db 0      ; Base high

gdt_end:
gdt_descriptor:
  dw gdt_end - gdt_null - 1 ; Size of GDT
  dd gdt_null               ; Address of GDT

[bits 32]
protected_mode:
  ; Set up segment registers
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  
  mov esi, 0x10000  ; Set ESI to the kernel load address
  mov edi, 0x100000 ; Set EDI to the destination address (1MB)
  mov ecx, 100 * 512 / 4  ; Number of bytes to copy (100 sectors)
  rep movsd         ; Copy the kernel to 1MB

  jmp 0x08:0x100000 ; Jump to the kernel entry point at 1MB