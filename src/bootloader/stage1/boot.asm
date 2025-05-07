org 0x7C00 ; boot sector, org defines origin address to the asssembler
bits 16 ; informs to assembler to generate in 16 bits mode

%define ENDL 0x0D, 0x0A ; define end of line

;
; JUMPING FAT12 HEADER
;
jmp short start                                    ; jump 2 bytes after jump to start of code
nop                                                ; jump 1 byte

bdb_oem:                   db 'MSWIN4.1'          ; OEM identifier, an 8-byte string identifying the system (here, a custom 'M5WIN4.1')
bdb_bytes_per_sector:      dw 512                 ; Bytes per sector, set to 512 (standard sector size for floppy disks)
bdb_sectors_per_cluster:   db 1                   ; Sectors per cluster, set to 1 (each cluster is one sector, 512 bytes)
bdb_reserved_sectors:      dw 1                   ; Reserved sectors, set to 1
bdb_fat_count:             db 2                   ; Number of FATs, set to 2 (two File Allocation Tables for redundancy)
bdb_dir_entries_count:     dw 0E0h                ; Root directory entries, set to 224
bdb_total_sectors:         dw 2880                ; Total sectors on the disk, set to 2880 (1.44 MB floppy: 2880 * 512 bytes)
bdb_media_descriptor_type: db 0F0h                ; Media descriptor, set to 0xF0 (indicates a 3.5-inch, 1.44 MB floppy disk)
bdb_sectors_per_fat:        dw 9                  ; Sectors per FAT, set to 9 (each FAT occupies 9 sectors, 4608 bytes)
bdb_sectors_per_track:     dw 18                  ; Sectors per track, set to 18 (standard for 1.44 MB floppy disks)
bdb_heads:                 dw 2                   ; Number of heads, set to 2 (one head per side of the floppy disk)
bdb_hidden_sectors:        dd 0                   ; Hidden sectors, set to 0 (no hidden sectors on a floppy disk)
bdb_large_sector_count:    dd 0                   ; Large sector count, set to 0 (used for disks > 65,536 sectors; not needed here)

; Extended Boot Record (EBR) fields, extending the BPB for FAT12
ebr_drive_number:          db 0                   ; Drive number, set to 0 (typically the boot floppy drive)
                           db 0                   ; Reserved byte, set to 0 (unused, for alignment or future use)
ebr_signature:             db 29h                 ; Volume signature, set to 0x29 (indicates an extended BPB with volume info)
ebr_volume_id:             db 12h, 34h, 56h, 78h  ; Volume ID, a 4-byte unique identifier (here, 0x12345678)
ebr_volume_label:          db 'MYOS       '       ; Volume label, an 11-byte string naming the disk ('MYOS' with padding)
ebr_system_id:             db 'FAT12   '          ; File system type, an 8-byte string identifying the FS ('FAT12' with padding)

start:
  ; setup data segments
  mov ax, 0                                       ; can't write to ds/es directly
  mov ds, ax                                      ; data segment
  mov es, ax                                      ; extra segment

  ;setup stack
  mov ss, ax                                      ; stack segment
  mov sp, 0x7C00                                  ; stack grows downwards from where we are loaded in memory

  ; some BIOSes might start us at 07C0:0000 instead of 0000:7C00, make sure we are in the
  ; expected location
  push es
  push word .after
  retf
.after:
  ; read something from floppy disk
  ; BIOS should set DL to drive number
  mov [ebr_drive_number], dl

  ; show loading message
  mov si, msg_loading                               ; load address of string into si
  call puts                                         ; BIOS should set DL to drive number

  ; read drive parameters (sectors per track and head count).
  ; instead of relying on data on formatted disk
  push es                                         ; save ES register (extra segment) on the stack
  mov ah, 08h                                     ; BIOS function 08h = Get Drive Parameters
  int 13h                                         ; call BIOS interrupt to retrieve drive geometry info
  jc floppy_error                                 ; if Carry Flag is set, an error occurred → jump to floppy_error
  pop es                                          ; restore original ES register value
  
 ; cx = ch:cl
  and cl, 0x3F                                    ; remove top 2 bits
  xor ch, ch                                      ; clears the CH register
  mov [bdb_sectors_per_track], cx                 ; sector count

  inc dh                                          ; head count
  mov [bdb_heads], dh                             ; stores the number of sectors per track in bdb_sectors_per_track

  ; bx = bh:hl
  ; read FAT root directory
  mov ax, [bdb_sectors_per_fat]                   ; compute lba of root directory = reserved + fats * sectors_per_fat
  mov bl, [bdb_fat_count] 
  xor bh, bh
  mul bx
  add ax, [bdb_reserved_sectors]                  ; ax = (fats * sectors_per_fat)
  push ax                                         ; ax = root directory

  ; compute size of root directory = (32 * number_of_entries) / bytes_per_sector
  ; note: this section can be hardcoded
  mov ax, [bdb_dir_entries_count]
  shl ax, 5                                       ; ax *= 32
  xor dx, dx                                      ; dx = 0
  div word [bdb_bytes_per_sector]                 ; number of sectors we need to read

  test dx, dx                                     ; if dx != 0, add 1
  jz   .root_dir_after
  inc  ax                                         ; division remainder != 0, add 1
  ; this means we have a sector only partially filled with entries
; ax = al:ah
.root_dir_after:
  ; read root directory
  mov cl, al                                       ; cl = number of sectors to read = size of root directory
  pop ax                                           ; ax = LBA of root direcoty
  mov dl, [ebr_drive_number]                       ; dl = drive number (we saved it previously)
  mov bx, buffer                                   ; es:bx = buffer
  call disk_read

  ; search for kernel.bin
  xor bx, bx
  mov di, buffer

.search_kernel:
  mov si, file_stage2_bin
  mov cx, 11                                       ; compare up to 11 characters
  push di
  repe cmpsb
  pop di
  je .found_kernel

  add di, 32
  inc bx
  cmp bx, [bdb_dir_entries_count]
  jl .search_kernel

  ; kernel not found
  jmp kernel_not_found_error

.found_kernel:
  ; di should have the address to the entry
  mov ax, [di + 26]                                ; first logical cluster field (offset 26)
  mov [stage2_cluster], ax

  ; load FAT from disk into memory
  mov ax, [bdb_reserved_sectors]
  mov bx, buffer
  mov cl, [bdb_sectors_per_fat]
  mov dl, [ebr_drive_number]
  call disk_read

  ; read kernel and process FAT chain
  mov bx, STAGE2_LOAD_SEGMENT
  mov es, bx
  mov bx, STAGE2_LOAD_OFFSET

.load_kernel_loop:
  ; Read next cluster
  mov ax, [stage2_cluster]

  ; not nice :( hardcoded value
  add ax, 31                                       ; first cluster = (stage2_cluster - 2) * sectors_per_cluster * start_sector
                                                   ; start sector = reserved + fats + root director size = 1 + 18 + 134 = 33
  mov cl, 1
  mov dl, [ebr_drive_number]
  call disk_read

  add bx, [bdb_bytes_per_sector]

  ; compute location of next cluster
  mov ax, [stage2_cluster]
  mov cx, 3
  mul cx
  mov cx, 2
  div cx                                            ; ax = index of entry in FAT, dx = cluster mod 2

  mov si, buffer
  add si, ax
  mov ax, [ds:si]                                   ; read entry from FAT table at index ax

  or dx, dx
  jz .even

.odd:
  shr ax, 4
  jmp .next_cluster_after

.even:
  and ax, 0x0FFF

.next_cluster_after:
  cmp ax, 0x0FF8
  jae .read_finish                                   ; end of chain

  mov [stage2_cluster], ax
  jmp .load_kernel_loop

.read_finish:
  ; jump to our kernel
  mov dl, [ebr_drive_number]                         ; boot device in dl

  mov ax, STAGE2_LOAD_SEGMENT                        ; set segment registers
  mov ds, ax
  mov es, ax

  jmp STAGE2_LOAD_SEGMENT:STAGE2_LOAD_OFFSET

  jmp wait_key_and_reboot                             ; should never happen

  cli                                                 ; disable interrupts, this way CPU can't get out of "halt" state
  hlt

;
; Error handlers
;

floppy_error:
  mov si, msg_read_failed
  call puts
  jmp wait_key_and_reboot

kernel_not_found_error:
  mov si, msg_stage2_not_found
  call puts
  jmp wait_key_and_reboot

wait_key_and_reboot:
  mov ah, 0
  int 16h      ; wait for keypress
  jmp 0FFFFh:0 ; jump to beginning of BIOS, should reboot

.halt:
  cli          ; disable interrupts, this way CPU can't get out of "halt" state
  hlt

; Prints a string to the screen
; Params:
; - ds:si points to string
puts:
  ; save registers we will modifiy
  push si
  push ax
  push bx

.loop:
  lodsb         ; loads next character in al
  or al, al     ; verify if next character is null?
  jz .done      ; if null, we are done

  mov ah, 0x0e  ; call bios interrupt
  mov bh, 0     ; page number 
  int 0x10      ; print character in al

  jmp .loop

.done:
  pop bx
  pop ax
  pop si
  ret

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

  inc dx                           ; dx = (LBA % Sectors per track) + 1
  mov cx, dx                       ; cx = sector

  xor dx, dx

  div word [bdb_heads]             ; ax = (LBA / SectorsPerTrack) / Heads = head
                                   ; dx = (LBA / SectorsPerTrack) % Heads
  mov dh, dl                       ; dh = head
  mov ch, al                       ; ch = cylinder
  shl ah, 6                        ; shift left 6 bits to get the upper part of the cylinder
  or cl, ah                        ; put upper 2 bits of cylinder in cl

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
  pop ax  ; restore registers modified
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

msg_loading:           db 'Loading...', ENDL, 0 ; null-terminated string
msg_read_failed:       db 'Read from disk failed!', ENDL, 0
msg_stage2_not_found:  db 'STAGE2.BIN file not found!', ENDL, 0
file_stage2_bin:       db 'STAGE2  BIN'
stage2_cluster:        dw 0

STAGE2_LOAD_SEGMENT   equ 0x0
STAGE2_LOAD_OFFSET    equ 0x500

times 510-($-$$) db 0 ; fill the rest of the sector with zeros
dw 0AA55h
buffer: 