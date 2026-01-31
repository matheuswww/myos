BITS 16
ORG 0x7E00

  cli
  mov si, msg_stage2
  call print_string

  mov ah, 0x02        ; AH = 0x02 -> BIOS function: Read Sectors From Drive
  mov al, 70          ; AL = 70 -> number of sectors to read
  mov ch, 0           ; CH = 0 -> cylinder number (track)
  mov cl, 10          ; CL = 10 -> sector number (starts at 1)
  mov dh, 0           ; DH = 0 -> head number

  mov bx, 0x1000      ; BX = 0x1000 -> memory offset to load the kernel (ES:BX)
  mov es, bx          ; ES = 0x1000 -> set Extra Segment to 0x1000
  xor bx, bx          ; ES:BX = 0x10000
  int 0x13            ; call BIOS interrupt 0x13 to read sectors
  jc disk_error_stage2 ; jump to error handler if carry flag is set (read failed)

  call enable_a20  ; Enable A20 line to access memory above 1MB
  lgdt [gdt_descriptor] ; Load the Global Descriptor Table
  
  mov ax, 800
  mov bx, 600
  mov cl, 32
  call vbe_set_mode ; set VESA video mode (e.g., 800x600x32)

  cli
  mov eax, cr0  ; Get the current value of CR0
  or eax, 1     ; Set the PE (Protection Enable) bit0x0D, 0x0A,
  mov cr0, eax  ; Update CR0 to enable protected mode
  jmp 0x08:protected_mode ; Far jump to flush the prefetch queue and enter protected mode





; a20
enable_a20:
  in al, 0x92 ; Read port 0x92
  or al, 2    ; Set bit 1 to enable A20
  out 0x92, al ; Write back to port 0x92
  jmp $+2 ; delay
  jmp $+2 ; delay
  ret





; print_string, disk_error_stage2

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





; GDT setup
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





; ******** VBE ********
vbe_info_block:
    .signature          db 'VBE2'               ; +00h
    .version            dw 0                    ; +04h
    .oem_string_ptr     dd 0                    ; +06h
    .capabilities       dd 0                    ; +0Ah
    .video_modes        dd 0                    ; +0Eh ; pointer to supported video modes
    .total_memory       dw 0                    ; +12h  (64KB blocks)
    .oem_software_rev   dw 0                    ; +14h
    .oem_vendor_name    dd 0                    ; +16h
    .oem_product_name   dd 0                    ; +1Ah
    .oem_product_rev    dd 0                    ; +1Eh
                        times 492 db 0          ;  rest of the 512-byte block

; Mode Info Block (256 bytes)
mode_info_block:
    .attributes         dw 0                    ; +00h
    .window_a           db 0                    ; +02h
    .window_b           db 0                    ; +03h
    .window_granularity dw 0                    ; +04h
    .window_size        dw 0                    ; +06h
    .window_a_segment   dw 0                    ; +08h
    .window_b_segment   dw 0                    ; +0Ah
    .window_function    dd 0                    ; +0Ch
    .bytes_per_scanline dw 0                    ; +10h  ← pitch
    .width              dw 0                    ; +12h
    .height             dw 0                    ; +14h
    .char_width         db 0                    ; +16h
    .char_height        db 0                    ; +17h
    .planes             db 0                    ; +18h
    .bpp                db 0                    ; +19h  ← bits per pixel
    .banks              db 0                    ; +1Ah
    .memory_model       db 0                    ; +1Bh
    .bank_size          db 0                    ; +1Ch
    .image_pages        db 0                    ; +1Dh
    .reserved0          db 0                    ; +1Eh
    .red_mask_size      db 0                    ; +1Fh
    .red_field_pos      db 0                    ; +20h
    .green_mask_size    db 0                    ; +21h
    .green_field_pos    db 0                    ; +22h
    .blue_mask_size     db 0                    ; +23h
    .blue_field_pos     db 0                    ; +24h
    .rsvd_mask_size     db 0                    ; +25h
    .rsvd_field_pos     db 0                    ; +26h
    .directcolor_mode   db 0                    ; +27h
    .framebuffer        dd 0                    ; +28h  ← endereço físico do LFB !!!
    .offscreen_mem_ofs  dd 0                    ; +2Ch
    .offscreen_mem_size dd 0                    ; +30h
                        times 256-0x34 db 0     ; resto até 256 bytes

; vbe_set_mode:
; Sets a VESA mode
; In\	AX = Width
; In\	BX = Height
; In\	CL = Bits per pixel
; Out\	FLAGS = Carry clear on success
; Out\	Width, height, bpp, physical buffer, all set in vbe_screen structure

width_     dw 0
height_    dw 0
bpp_       db 0
offset_    dw 0
segment_   dw 0
mode_      dw 0

vbe_set_mode:
  mov [width_], ax  ; store width
  mov [height_], bx ; store height
  mov [bpp_], cl    ; store bits per pixel

  xor ax, ax
  mov ds, ax
  mov es, ax

  push es
  mov ax, 0x4F00 ; get VBE BIOS info
  mov di, vbe_info_block ; ES:DI points to VBE info block
  int 0x10
  pop es

  cmp ax, 0x4F ; check if VBE function succeeded
  jne .error

  mov ax, word[vbe_info_block.video_modes] ; get pointer to video modes list
  mov [offset_], ax ; store offset
  mov ax, word[vbe_info_block.video_modes + 2] ; get segment of video modes list
  mov [segment_], ax ; store segment

  mov ax, [segment_] ; load segment of video modes list
  mov fs, ax         ; set FS to segment of video modes list
  mov si, [offset_]  ; load offset of video modes list

  call vbe_set_mode.find_mode
  ret

.find_mode:
  mov dx, [fs:si] ; get mode number
  add si, 2       ; move to next mode
  mov [offset_], si ; update offset
  mov [mode_], dx   ; store current mode

  cmp word[mode_], 0xFFFF ; check for end of list
  je .error           ; if end of list, mode not found

  push es
  mov ax, 0x4F01 ; set VBE mode
  mov cx, [mode_] ; mode number
  mov di, mode_info_block ; ES:DI points to mode info block
  int 0x10 ; call VBE function
  pop es

  cmp ax, 0x4F ; check if VBE function succeeded
  jne .error

  mov ax, [width_] ; verify width
  cmp ax, [mode_info_block.width]
  jne .next_mode

  mov ax, [height_] ; verify height
  cmp ax, [mode_info_block.height]
  jne .next_mode

  mov al, [bpp_] ; verify bits per pixel
  cmp al, [mode_info_block.bpp]
  jne .next_mode

  ; Set the mode
  push es
  mov ax, 0x4F02  ; set VBE mode with linear framebuffer
  mov bx, [mode_] ; mode number
  or  bx, 0x4000   ; set bit 14 for linear framebuffer
  mov di, 0       ; not sure if some BIOSes need this
  int 0x10
  pop es

  cmp ax, 0x4F ; check if VBE function succeeded
  jne .error

  clc
  ret

.next_mode:
  mov si, [offset_] ; load updated offset
  jmp .find_mode

.error:
	stc
	ret





; ******** protected_mode ********
bits 32
protected_mode:
  ; Set up segment registers
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  mov esi, 0x10000  ; Set ESI to the kernel load address
  mov edi, KERNEL_PHYS ; Set EDI to the destination address)
  mov ecx, 70 * 512 / 4  ; Number of bytes to copy (70 sectors)
  rep movsd         ; Copy the kernel to 1MB

  call enable_paging ; Enable paging
  call set_vbe_info

  ; direct far jump to kernel virtual address
  jmp 0x08:KERNEL_VIRT
 




; ******** pagging ********
ENTRIES_4MB     equ 1024

; Page Directory physical address (4 KB)
PAGE_DIR_PHYS     equ 0x10000
; Page Table for identity mapping + VRAM
PT_IDENTITY_PHYS  equ 0x11000
; Page Table for kernel mapping at 5 MB
PT_KERNEL_PHYS    equ 0x12000

; Kernel loaded at this physical address
KERNEL_PHYS       equ 0x105000

; Identity mapping (low memory)
IDENTITY_VIRT   equ 0x00000000
; VRAM mapped at 1 MB virtual
VRAM_VIRT       equ 0x00101000
; Kernel mapped at 4 MB virtual
KERNEL_VIRT     equ 0x00400000

; Page flags
P_PRESENT       equ 0x001
RW              equ 0x002

enable_paging:
  ; init identity PT + VRAM mapping
  call init_identity_pt
  ; init kernel PT
  call init_pt_kernel
  ; setup Page Directory
  call init_page_directory

  ; load Page Directory address into CR3
  mov eax, PAGE_DIR_PHYS
  mov cr3, eax

  ; enable paging (set PG bit)
  mov eax, cr0
  or  eax, 0x80000000
  mov cr0, eax

  ret

; init page table for identity (0–1 MB) + VRAM at 1 MB virtual
init_identity_pt:
  mov edi, PT_IDENTITY_PHYS
  xor ebx, ebx                    ; ebx = current virtual address
  mov ecx, ENTRIES_4MB            ; 1024 entries (covers 4 MB total)
  
  ; Load the physical address of the Framebuffer into EDX
  mov edx, [mode_info_block.framebuffer] 

.fill_identity:
  cmp ebx, VRAM_VIRT
  jae .is_vram  ; If EBX >= 1.004MB, jump to map VRAM

  ; Identity Mapping (Virtual Address = Physical Address)
  mov eax, ebx
  or  eax, P_PRESENT | RW
  jmp .write_entry

.is_vram:
  ; VRAM Mapping (Virtual Address EBX -> Physical Address EDX)
  mov eax, edx
  or  eax, P_PRESENT | RW
  add edx, 0x1000                 ; Advance physical address to the next 4KB page

.write_entry:
  mov [edi], eax
  add ebx, 0x1000                 ; Advance virtual address
  add edi, 4                      ; Move to the next entry in the Page Table
  loop .fill_identity

  ret

; init kernel page table (maps KERNEL_PHYS to 5 MB virtual)
init_pt_kernel:
  mov edi, PT_KERNEL_PHYS
  mov ebx, KERNEL_PHYS
  mov ecx, ENTRIES_4MB            ; cover 4 MB (enough for most kernels)

.fill_kernel:
  mov eax, ebx
  or  eax, P_PRESENT | RW
  mov [edi], eax
  add ebx, 0x1000
  add edi, 4
  loop .fill_kernel

  ret

; setup Page Directory entries
init_page_directory:
  ; PD[0] → identity + VRAM (0–4 MB virtual)
  mov eax, PT_IDENTITY_PHYS
  or  eax, P_PRESENT | RW
  mov [PAGE_DIR_PHYS + 0*4], eax

  ; PD[1] → kernel (4–8 MB virtual, covers 5 MB)
  mov eax, PT_KERNEL_PHYS
  or  eax, P_PRESENT | RW
  mov [PAGE_DIR_PHYS + 1*4], eax

  ; clear remaining Page Directory entries
  xor eax, eax
  mov edi, PAGE_DIR_PHYS + 8
  mov ecx, 1022
  rep stosd

  ret





; ******** set vbe info ********
set_vbe_info:
; vbe_screen:
  ;     .width              dw 0      ; Screen width in pixels (e.g. 1024)
  ;     .height             dw 0      ; Screen height in pixels (e.g. 768)
  ;
  ;     .bits_per_pixel     db 0      ; Bits per pixel (e.g. 32 for 32bpp)
  ;
  ;     .bytes_per_pixel    dd 0      ; Bytes per pixel (e.g. 4 for 32bpp)
  ;                                   ; Stored as 32-bit for easy use in C
  ;
  ;     .bytes_per_scanline dw 0      ; Bytes per scanline (pitch)
  ;                                   ; Includes possible padding added by VBE
  ;
  ;     .physical_buffer    dd 0      ; Physical address of the Linear Frame Buffer (LFB)
  ;
  ;     .x_cur_max          dw 0      ; Maximum text cursor X position (zero-based)
  ;                                   ; Usually derived from width / character width
  ;
  ;     .y_cur_max          dw 0      ; Maximum text cursor Y position (zero-based)
  ;                                   ; Usually derived from height / character height
  ;
  ;     ; Optional fields you may add later if needed:
  ;     ; .virtual_buffer   dd 0      ; Virtual address after paging is enabled
  ;     ; .cursor_x         dw 0      ; Current cursor X position
  ;     ; .cursor_y         dw 0      ; Current cursor Y position
  ;     ; .text_color       dd 0      ; Current text color (RGBA or similar)

  mov edi, VRAM_VIRT - 0x00001000

  movzx eax, word [width_]            ; eax = width (16-bit → 32-bit)
  mov [edi + 0], ax                   ; .width (dw)

  movzx eax, word [height_]           ; eax = height
  mov [edi + 2], ax                   ; .height (dw)

  movzx eax, byte [bpp_]              ; eax = bits per pixel
  mov [edi + 4], al                   ; .bits_per_pixel (db)

  movzx eax, word [mode_info_block.bytes_per_scanline]
  mov [edi + 5], eax                  ; .bytes_per_scanline (dw → dd no seu comentário)

  mov eax, [mode_info_block.framebuffer]
  mov [edi + 9], eax                  ; .physical_buffer (dd)

  movzx eax, word [width_]
  shr eax, 3                          ; divide por 8 (pixels → caracteres)
  dec eax
  mov [edi + 15], ax                  ; .x_cur_max (dw)

  movzx eax, word [height_]
  shr eax, 4                          ; divide por 16 (pixels → linhas)
  dec eax
  mov [edi + 17], ax                  ; .y_cur_max (dw)

  ret