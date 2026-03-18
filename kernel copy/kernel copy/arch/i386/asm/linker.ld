ENTRY(_start)

SECTIONS
{
  . = 0x00400000;

  .text ALIGN(4K) : {
      *(.text*)
  }

  .rodata ALIGN(4K) : {
      *(.rodata*)
  }

  .data ALIGN(4K) : {
      *(.data*)
  }

  .bss ALIGN(4K) : {
      *(COMMON)
      *(.bss*)
  }

  . = ALIGN(4K);
  heap_start = .;
  . += 1M;
  heap_end = .;

  . = ALIGN(16);
  stack_bottom = .;
  . += 16K;
  stack_top = .;
}