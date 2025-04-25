org 0x0 ; boot sector
bits 16 ; 16-bit code

%define ENDL 0x0D, 0x0A ; define end of line

start:
  jmp main


main:
  ; print message
  mov si, msg_hello ; load address of string into si
  call puts ; call puts to print the string

.halt:
  cli
  hlt ; hold the cpu

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

msg_hello: db 'Hello world!', ENDL, 0 ; null-terminated string