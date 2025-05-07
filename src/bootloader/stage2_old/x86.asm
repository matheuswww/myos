bits 16

section _TEXT class=CODE

;
; U4D
;
; Operation:            unsigned 4 bytes divide
; Inputs:               DX;AX Dividend
;
; Outputs:              CX;BX Divisor
;
; Volatile:             none

global __U4D
__U4D:
  shl dx, 16            ; dx to upper half of edx
  mov dx, ax            ; ecx - divisor
  mov eax, edx          ; eax - dividend
  xor edx, edx
  
  shl ecx, 16         ; cx to upper half of ecx
  mov cx, bx          ; ecx - divisor

  div ecx             ; eax - quot, edx - remainder
  mov ebx, edx
  mov ecx, edx
  shr ecx, 16

  mov edx, eax
  shr edx, 16

  ret

;
; U4M
; Operation:          integer four byte multiply
; Inputs:             DX;AX integer M1
;                     CX;BX integer M2
; Outputs:            DX;AX product
; Volatilite:         CX, BX destroyted
;
global __U4M
__U4M:
  shl edx, 16         ; dx to upper half of edx
  mov dx, ax          ; m1 in edx
  mov eax, edx        ; m1 in eax

  shl ecx, 16         ; cx to upper half of ecx
  mov cx, bx          ; m2 in ecx

  mul ecx             ; result in edx:eax (we only need eax)
  mov edx, eax        ; move upper half to dx
  shr edx, 16

  ret
;
; void _cdecl x86_div64_32(uint64_t dividend, uint32_t divisor, uint32_t *quotient, uint32_t *remainder);

global _x86_div64_32
_x86_div64_32:

  ; make new call frame
  push bp              ; save old call frmae 
  mov bp, sp          ; initialize new call frame

  push bx

  ; divide upper 32 btis
  mov eax, [bp + 8]  ; eax <- upper 32 bits
  mov ecx, [bp + 12] ; ecx <- divisor
  xor edx, edx
  div ecx            ; eax - quot, edx - remainder

  ; store upper 32 bits of quotient
  mov ebx, [bp + 16]
  mov [bx + 4], eax

  ; divide lower 32 bits
  mov eax, [bp + 4] ; eax <- lower 32 bits of dividend
                    ; edx <- old remainder
  div ecx

  ; store results
  mov [bx], eax
  mov bx, [bp + 18]
  mov [bx], edx

  pop bx

  ; restore old call frame
  mov sp, bp
  pop bp
  ret

; int 10h ah=0Eh
; args: character, page
;

global _x86_Video_WriteCharTeletype
_x86_Video_WriteCharTeletype:

  ; make new call frame
  push bp               ; save old call frame
  mov bp, sp            ; initialize new call frame

  ;save bx
  push bx

  ; [bp + 0] - old call frame
  ; [bp + 2] - return address (small memory model >= 2 bytes)
  ; [bp + 4] - first argument (character); bytes are converted to words (you cant push a single byte on the stack)
  ; note: [bp + 4] - second argument (page)
  ; [bp + 6] - second argument (page)
  ; note: bytes are converted to words (you cant push a snigle byte on the stack)
  mov ah, 0Eh
  mov al, [bp + 4]
  mov bh, [bp + 6]

  int 10h

  ; restore bx
  pop bx

  ; restore old call frame
  mov sp, bp
  pop bp
  ret

;
; void _cdecl x86_Disk_Reset(uint8_t drive);
;
global _x86_Disk_Reset
_x86_Disk_Reset:
  ; make new call frame
  push bp               ; save old call frame
  mov bp, sp            ; initialize new call frame

  mov ah, 0
  mov dl, [bp + 4] ; dl - drive
  stc
  int 13h
  
  mov ax, 1
  sbb ax, 0              ; 1 on success, 0 on fail

  ; restore old call frame
  mov sp, bp
  pop bp
  ret

; bool _cdecl x86_Disk_Read(uint8_t drive,
;                          uint16_t cylinder,
;                          uint16_t sector,
;                          uint16_t head,
;                          uint8_t count,
;                          uint8_t far* bufferOut);
;

global _x86_Disk_Read
_x86_Disk_Read:
  ; make new call frame
  push bp               ; save old call frame
  mov bp, sp            ; initialize new call frame

  ; save modified regs
  push bx
  push es

  ; setup args
  mov dl, [bp + 4]   ; dl - drive

  mov ch, [bp + 6]   ; ch - cylinder (lower 8 bits)
  mov cl, [bp + 7]   ; cl - cylinder to bits 6-7
  shl cl, 6

  mov al, [bp + 8]  ; cl - sector to bits 0-5
  and al, 3Fh
  or cl, al       

  mov dh, [bp + 10]  ; al - head

  mov al, [bp + 12]   ; al - count

  mov bx, [bp + 16]   ; es:bx - far pointer to data out
  mov es, bx
  mov bx, [bp + 14]

  ; call int13h
  mov ah, 02h
  stc
  int 13h

  mov ax, 1
  sbb ax, 0          ; 1 on success, 0 on fail

  ; set return value
  pop es
  pop bx

  ; restore old call frame
  mov sp, bp
  pop bp
  ret


; bool _cdecl x86_Disk_GetDriveParams(uint8_t drive,
;                          uint8_t* driveTypeOut,
;                          uint8_t* cylindersOut,
;                          uint16_t* sectorsOut,
;                          uint16_t* headsOut);


global _x86_Disk_GetDriveParams
_x86_Disk_GetDriveParams:
  ; make new call frame
  push bp               ; Save the base pointer (bp) to preserve the caller's stack frame
  mov bp, sp            ; Set up a new stack frame by copying the stack pointer (sp) to bp

  ; save regs
  push es
  push bx
  push si
  push di

  ; call int13h
  mov dl, [bp + 4]      ; Load the disk drive number from the stack (parameter at bp + 4) into DL
  mov ah, 08h           ; Set AH to 08h, the BIOS function code for "Get Drive Parameters"
  mov di, 0             ; Set DI to 0 (part of ES:DI = 0000:0000 for BIOS call)
  mov es, di            ; Set ES to 0 (segment for ES:DI = 0000:0000)
  stc                   ; Set the carry flag (some BIOSes require this before INT 13h)
  int 13h               ; Call BIOS interrupt 13h to get disk drive parameters

  ; return
  mov ax, 1             ; Set AX to 1 (assume success for return value)
  sbb ax, 0             ; Subtract carry flag from AX (AX = 1 if CF=0, AX = 0 if CF=1, indicating error)

  ; out params
  mov si, [bp + 6]      ; Load the address of the drive type output parameter from the stack
  mov [si], bl          ; Store the drive type (returned in BL by BIOS) at the address in SI

  mov bl, ch            ; Move lower 8 bits of cylinders (from CH) to BL
  mov bh, cl            ; Move cylinder bits (from CL) to BH
  shr bh, 6             ; Shift BH right by 6 to get the upper 2 bits of cylinders (bits 6-7 of CL)
  mov si, [bp + 8]      ; Load the address of the cylinders output parameter from the stack
  mov [si], bx          ; Store the number of cylinders (BX) at the address in SI

  xor ch, ch            ; Clear CH to zero (prepare for sectors calculation)
  and cl, 3Fh           ; Mask CL to keep only the lower 6 bits (sectors, bits 0-5 of CL)
  mov si, [bp + 10]     ; Load the address of the sectors output parameter from the stack
  mov [si], cx          ; Store the number of sectors (CX) at the address in SI

  mov cl, dh            ; Move the number of heads (from DH) to CL
  mov si, [bp + 12]     ; Load the address of the heads output parameter from the stack
  mov [si], cx          ; Store the number of heads (CX) at the address in SI

  ; restore regs
  pop di
  pop si
  pop es
  pop es

  ; restore old call frame
  mov sp, bp            ; Restore the stack pointer to the base pointer, discarding the stack frame
  pop bp                ; Restore the caller's base pointer
  ret                   ; Return to the caller