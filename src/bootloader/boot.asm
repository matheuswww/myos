org 0x7C00 ; boot sector
bits 16 ; 16-bit code

%define ENDL 0x0D, 0x0A ; define end of line

;
; FAT12 HEADER
;
jmp short start ; jump to start of code
nop

bdb_oem:                   db 'M5WIN4.1' ; OEM identifier, an 8-byte string identifying the system (here, a custom 'M5WIN4.1')
bdb_bytes_per_sector:      dw 512        ; Bytes per sector, set to 512 (standard sector size for floppy disks)
bdb_sectors_per_cluster:   db 1          ; Sectors per cluster, set to 1 (each cluster is one sector, 512 bytes)
bdb_reserved_sectors:      dw 1          ; Reserved sectors, set to 1
bdb_fat_count:             db 2          ; Number of FATs, set to 2 (two File Allocation Tables for redundancy)
bdb_dir_entries:           dw 0E0h       ; Root directory entries, set to 224
bdb_total_sectors:         dw 2880       ; Total sectors on the disk, set to 2880 (1.44 MB floppy: 2880 * 512 bytes)
bdb_media_descriptor_type: db 0F0h       ; Media descriptor, set to 0xF0 (indicates a 3.5-inch, 1.44 MB floppy disk)
bdb_sector_per_fat:        dw 9          ; Sectors per FAT, set to 9 (each FAT occupies 9 sectors, 4608 bytes)
bdb_sectors_per_track:     dw 18         ; Sectors per track, set to 18 (standard for 1.44 MB floppy disks)
bdb_heads:                 dw 2          ; Number of heads, set to 2 (one head per side of the floppy disk)
bdb_hidden_sectors:        dd 0          ; Hidden sectors, set to 0 (no hidden sectors on a floppy disk)
bdb_large_sector_count:    dd 0          ; Large sector count, set to 0 (used for disks > 65,536 sectors; not needed here)

; Extended Boot Record (EBR) fields, extending the BPB for FAT12
ebr_drive_number:          db 0          ; Drive number, set to 0 (typically the boot floppy drive)
                           db 0          ; Reserved byte, set to 0 (unused, for alignment or future use)
ebr_signature:             db 29h        ; Volume signature, set to 0x29 (indicates an extended BPB with volume info)
ebr_volume_id:             dd 12h, 34h, 56h, 78h ; Volume ID, a 4-byte unique identifier (here, 0x12345678)
ebr_volume_label:          db 'MYOS       ' ; Volume label, an 11-byte string naming the disk ('MYOS' with padding)
ebr_system_id:             db 'FAT12   '  ; File system type, an 8-byte string identifying the FS ('FAT12' with padding)

start:
  jmp main

; Prints a string to the screen
; Params:
; - ds:si points to string
puts:
  ; save registers we will modifiy
  push si
  push ax

.loop:
  lodsb ; loads next character in al
  or al, al ; verify if next character is null?
  jz .done ; if null, we are done

  mov ah, 0x0e ; call bios interrupt
  mov bh, 0 ; page number 
  int 0x10 ; print character in al

  jmp .loop

.done:
  pop ax
  pop si
  ret

main:
;setup data segments
  mov ax, 0 ; can't write to ds/es directly
  mov ds, ax ; data segment
  mov es, ax ; extra segment

  ;setup stack
  mov ss, ax ; stack segment
  mov sp, 0x7C00 ; stack grows downwards from where we are loaded in memory

  ; read something from floppy disk
  ; BIOS should set DL to drive number
  mov [ebr_drive_number], dl

  mov ax, 1        ; LBA=1, second sector from disk
  mov cl, 1        ; 1 sector to read
  mov bx, 0x7E00   ; data should be after the bootloader
  call disk_read

  ; print message
  mov si, msg_hello ; load address of string into si
  call puts ; BIOS should set DL to drive number

  cli           ; disable interrupts, this way CPU can't get out of "halt" state
  hlt

floppy_error:
  mov si, msg_read_failed
  call puts
  jmp wait_key_and_reboot

wait_key_and_reboot:
  mov ah, 0
  int 16h       ; wait for keypress
  jmp 0FFFFh:0  ; jump to beginning of BIOS, should reboot

.halt 
  cli           ; disable interrupts, this way CPU can't get out of "halt" state
  hlt

;
; Disk Routines
;

;
; Converts an LBA address to a CHS address
; sector = (LBA % sectors per track) + 1
; head = (LBA / sectors per track) % heads
; cylinder = (LBA / sectors per track) / heads
; Parameters:
; - ax: LBA address
; Returns:
; - cx [bits 0-5]: sector number
; - cx [bits 6-15]: cylinder
; - dh: head

lba_to_chs:
  push ax
  push dx

  xor dx, dx
  div word [bdb_sectors_per_track] ; divide LBA by sectors per track

  inc dx ; dx = (LBA % Sectors per track) + 1
  mov cx, dx ; cx = sector

  xor dx, dx

  div word [bdb_heads] ; ax = (LBA / SectorsPerTrack) / Heads = head
                      ; dx = (LBA / SectorsPerTrack) % Heads
  mov dh, dl ; dh = head
  mov ch, al ; ch = cylinder
  shl ah, 6 ; shift left 6 bits to get the upper part of the cylinder
  or cl, ah ; put upper 2 bits of cylinder in cl

  pop ax
  mov dl, al
  pop ax
  ret

;
; Reads sectors from a disk
; Parameters:
; - ax: LBA address
; - cl: number of sectors to read (up to 128)
; - dl: drive number
; - es:bx memory address where to store read data

disk_read:
  push ax         ; save registers we will modify
  push bx
  push cx
  push dx
  push di

  push cx         ; temporalily save CL (number of sectors to read)
  call lba_to_chs ; compute CHS
  pop ax          ; AL = number of sectors to read

  mov ah, 02h
  mov di, 3       ; retry count

.retry:
  pusha           ; save all registers, we don't know what bios modifies
  stc             ; set carry flag, some BIOS'es don't set it
  int 13h         ; carry flag cleared = success
  jnc .done       ; jump if carry not set

  ; read failed
  popa
  call disk_reset

  dec di
  test di, di
  jnz .retry

.fail:
  ; all attempts are exhausted
  jmp floppy_error

.done:
  popa

  pop di
  pop dx
  pop cx
  pop bx
  pop ax     ; restore registers modified
  ret

;
; Resets disk controller
; Parameters:
; d1: drive number
;
disk_reset:
  pusha
  mov ah, 0
  stc
  int 13h
  jc floppy_error
  popa
  ret

msg_hello:        db 'Hello world!', ENDL, 0 ; null-terminated string
msg_read_failed:  db 'Read from disk failed!', ENDL, 0 

times 510-($-$$) db 0 ; fill the rest of the sector with zeros
db 0x55, 0xAA ; boot sector signature