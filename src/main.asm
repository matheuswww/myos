org 0x7C00 ; boot sector
bits 16 ; 16-bit code

%define ENDL 0x0D, 0x0A ; define end of line

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

  ; print message
  mov si, msg_hello ; load address of string into si
  call puts ; call puts to print the string

  hlt ; hold the cpu
  .halt:
    jmp .halt ; infinite loop

msg_hello: db 'Hello world!', ENDL, 0 ; null-terminated string

times 510-($-$$) db 0 ; fill the rest of the sector with zeros
db 0x55, 0xAA ; boot sector signature