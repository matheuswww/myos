%macro x86_EnterRealMode 0
  [bits 32]
  jmp word 18h:.pmode16            ; 1 - jump to 16-bit protected mode segment
  
.pmode16:
  [bits 16]
  ; 2 - disble protected mode bit in cr0
  mov eax, cr0
  and al, ~1
  mov cr0, eax

  ; 3 = jump to real mode
  jmp word 00h:.rmode

.rmode:
  ; 4 - setup segments
  mov ax, 0
  mov ds, ax
  mov ss, ax

  ; 5 - enable interrupts
  sti

%endmacro

%macro x86_EnterProtectedMode 0
  cli

  ; 4 - set protection enable flag in CR0
  mov eax, cr0
  or al, 1
  mov cr0, eax

  ; 5 - far jump into protected
  jmp dword 08h:.pmode

  .pmode:
    ; we are now in protected mode!
    [bits 32]

    ; 6 - setup segment registers
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
%endmacro

; Convert linear address to segment:offset addresss
; Args:
; 1 - linear address
; 2 - (out) target segment (e.g es)
; 3 - target 32-bit register to use (e.g eax)
; 4 - target lower 16-bit half of #3 (e.g ax)

%macro LinearToSegOffset 4
  mov %3, %1            ; linear address to eax
  shr %3, 4
  mov %2, %4
  mov %3, %1
  and %3, 0xf

%endmacro

global x86_outb
x86_outb:
  [bits 32]
  mov dx, [esp + 4]
  mov al, [esp + 8]
  out dx, al
  ret

global x86_inb
x86_inb:
  [bits 32]
  mov dx, [esp + 4]
  xor eax, eax
  in al, dx
  ret


global x86_Disk_GetDriveParams
x86_Disk_GetDriveParams:
  [bits 32]

  ; make new call frame
  push ebp               ; Save the base pointer (ebp) to preserve the caller's stack frame
  mov ebp, esp            ; Set up a new stack frame by copying the stack pointer (esp) to esp

  x86_EnterRealMode

  [bits 16]

  ; save regs
  push es
  push bx
  push esi
  push di

  ; call int13h
  mov dl, [bp + 8]      ; Load the disk drive number from the stack (parameter at bp + 8) into DL
  mov ah, 08h           ; Set AH to 08h, the BIOS function code for "Get Drive Parameters"
  mov di, 0             ; Set DI to 0 (part of ES:DI = 0000:0000 for BIOS call)
  mov es, di            ; Set ES to 0 (segment for ES:DI = 0000:0000)
  stc                   ; Set the carry flag (some BIOSes require this before INT 13h)
  int 13h               ; Call BIOS interrupt 13h to get disk drive parameters

  ; return
  mov eax, 1             ; Set EAX to 1 (assume success for return value)
  sbb eax, 0             ; Subtract carry flag from EAX (EAX = 1 if CF=0, EAX = 0 if CF=1, indicating error)

  ; out params
  mov eax, 1
  sbb eax, 0

  ; drive type from bl
  LinearToSegOffset [bp + 12], es, esi, si
  mov [es:si], bl       ; Store the drive type (BL) at the address in ES:SI

  ; cylinders
  mov bl, ch            ; Move lower 8 bits of cylinders (from CH) to BL
  mov bh, cl            ; Move cylinder bits (from CL) to BH
  shr bh, 6             ; Shift BH right by 6 to get the upper 2 bits of cylinders (bits 6-7 of CL)
  inc bx
  
  LinearToSegOffset [bp + 16], es, esi, si
  mov [es:si], bx       ; Store the cylinder value (BX) at the address in ES:SI

  ; sectors
  xor ch, ch            ; Clear CH to zero (prepare for sectors calculation)
  and cl, 3Fh           ; Mask CL to keep only the lower 6 bits (sectors, bits 0-5 of CL)

  LinearToSegOffset [bp + 20], es, esi, si
  mov [es:si], cx

  ; heads
  mov cl, dh            ; Move the number of heads (from DH) to CL
  inc cx

  LinearToSegOffset [bp + 24], es, esi, si
  mov [es:si], cx          ; Store the number of heads (CX) at the address in SI

  ; restore regs
  pop di
  pop esi
  pop bx
  pop es

 ; return
  push eax

  x86_EnterProtectedMode

  [bits 32]

  pop eax

  ; restore old call frame
  mov esp, esp            ; Restore the stack pointer to the base pointer, discarding the stack frame
  pop ebp                ; Restore the caller's base pointer
  ret                    ; Return to the caller

global x86_Disk_Reset
x86_Disk_Reset:
  [bits 32]

  ; make new call frame
  push ebp               ; save old call frame
  mov ebp, esp            ; initialize new call frame

  x86_EnterRealMode

  mov ah, 0
  mov dl, [bp + 8] ; dl - drive
  stc
  int 13h
  
  mov eax, 1
  sbb eax, 0              ; 1 on success, 0 on fail

  push eax

  x86_EnterProtectedMode

  pop eax

  ; restore old call frame
  mov esp, ebp
  pop ebp
  ret


global x86_Disk_Read
x86_Disk_Read:
  ; make new call frame
  push ebp               ; save old call frame
  mov ebp, esp            ; initialize new call frame

  x86_EnterRealMode

  ; save modified regs
  push ebx
  push es

  ; setup args
  mov dl, [bp + 8]   ; dl - drive

  mov ch, [bp + 12]   ; ch - cylinder (lower 8 bits)
  mov cl, [bp + 13]   ; cl - cylinder to bits 6-7
  shl cl, 6

  mov al, [bp + 16]  ; cl - sector to bits 0-5
  and al, 3Fh
  or cl, al       

  mov dh, [bp + 20]  ; dh - head

  mov al, [bp + 24]   ; al - count

  LinearToSegOffset [bp + 28], es, ebx, bx
   
  ; call int13h
  mov ah, 02h
  stc
  int 13h

  mov eax, 1
  sbb eax, 0          ; 1 on success, 0 on fail

  ; set return value
  pop es
  pop bx

  push eax

  x86_EnterProtectedMode

  pop eax

  ; restore old call frame
  mov esp, ebp
  pop ebp
  ret
