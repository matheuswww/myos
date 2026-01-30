ORG 0x7c00
bits 16

start:
  cli
  xor ax, ax
  mov ds, ax
  mov es, ax
  mov ss, ax
  mov sp, 0x7c00

  mov ah, 0x02        ; AH = 0x02 -> BIOS function: Read Sectors From Drive
  mov al, 8           ; AL = 8 -> number of sectors to read
  mov ch, 0           ; CH = 0 -> cylinder number (track)
  mov cl, 2           ; CL = 2 -> sector number (starts at 1)
  mov dh, 0           ; DH = 0 -> head number
  
  mov bx, 0x7E00      ; BX = 0x7E00 -> memory offset to load the sectors (ES:BX)
  int 0x13            ; call BIOS interrupt 0x13 to read sectors
  jc disk_error       ; jump to error handler if carry flag is set (read failed)

  jmp 0x0000:0x7E00   ; jump to the loaded stage 2 bootloader in memory

disk_error:
  mov si, err_msg
  call print_string
  jmp $

print_string:
  lodsb
  or al, al
  jz .done
  mov ah, 0x0E
  int 0x10
  jmp print_string
.done:
  ret

err_msg db 'Error to read from stage 2', 0

times 510-($-$$) db 0
db 0x55
db 0xAA