bits 16                     ; Specify 16-bit mode for the assembler (real mode in x86).
                            ; Ensures instructions are generated for a 16-bit processor.

section _ENTRY class=CODE   ; Define a section named _ENTRY, classified as executable code.
                            ; Organizes the code for the linker.

extern _cstart_             ; Declare _cstart_ as an external function (defined elsewhere, likely in C).
                            ; Allows linking with a C function.

global entry                ; Make the 'entry' label globally accessible.
                            ; Marks 'entry' as the program's entry point.

entry:                      ; Label defining the program's starting point.
  cli                       ; Clear Interrupt Flag: Disable interrupts.
                            ; Prevents interruptions during initialization.

  mov ax, ds                ; Copy the data segment register (ds) to ax.
                            ; Prepares to align the stack segment with the data segment.

  mov ss, ax                ; Set the stack segment register (ss) to the value of ds (via ax).
                            ; Aligns the stack segment with the data segment for consistency.

  mov sp, 0                 ; Set the stack pointer (sp) to 0.
                            ; Initializes the stack to grow downward from the top of the segment.

  mov bp, sp                ; Set the base pointer (bp) to the stack pointer (sp).
                            ; Initializes bp for use in functions (e.g., for stack frames).

  sti                       ; Set Interrupt Flag: Re-enable interrupts.
                            ; Allows interrupts now that initialization is complete.

  ; expect boot drive in dl, send it as argument to cstart function
  xor dh, dh                ; Zero out the high byte of dx (dh), keeping dl (boot drive number) intact.
                            ; Ensures only the boot drive number is passed as an argument.

  push dx                   ; Push dx (containing the boot drive number) onto the stack.
                            ; Passes the boot drive as an argument to _cstart_.

  call _cstart_             ; Call the external _cstart_ function (likely written in C).
                            ; Transfers control to the C code for further initialization.

  cli                       ; Clear Interrupt Flag: Disable interrupts again.
                            ; Ensures no interruptions after _cstart_ returns (if it does).

  hlt                       ; Halt the CPU.
                            ; Stops execution until an interrupt occurs (effectively ends the program).