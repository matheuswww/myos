.intel_syntax noprefix

.equ dd_tf,      0        # offset: task file base I/O port (ex: 0x1F0)
.equ dd_dcr,     2        # offset: device control / alternate status port
.equ dd_stLBA,   4        # offset: partition starting LBA
.equ dd_prtlen,  8        # offset: partition length
.equ dd_sbits,   12       # offset: drive select + LBA mode bits (0xE0/0xF0)

max_sectors:
    .long 256

set_lba:
  add  eax, [esi + dd_stLBA]   # convert relative LBA → absolute LBA
  push eax                     # save absolute LBA on stack

  # --- select drive and send LBA bits 24–27 ---

  mov  dx, [esi + dd_tf]       # DX = base I/O port (ex: 0x1F0)
  add  dl, 6                   # DX = base+6 (drive/head register = 1F6)

  mov  ecx, eax                # copy LBA
  shr  ecx, 24                 # move bits 24–31 down
  and  cl, 0x0F                # keep only LBA[24–27]
  or   cl, [esi + dd_sbits]    # add master/slave + LBA mode bits
  mov  al, cl
  out  dx, al                  # write drive/head register

  # --- send sector count ---

  sub  dl, 4                   # back to base+2 (1F2 sector count register)
  mov  al, bl                  # number of sectors to read
  out  dx, al

  # --- send LBA low/mid/high bytes ---

  mov  eax, [esp]              # reload absolute LBA from stack

  inc  edx                     # base+3 (1F3) LBA bits 0–7
  out  dx, al

  inc  edx                     # base+4 (1F4) LBA bits 8–15
  mov  al, ah
  out  dx, al

  inc  edx                     # base+5 (1F5) LBA bits 16–23
  shr  eax, 16
  out  dx, al

  # --- send READ SECTORS command ---

  add  dl, 2                   # base+7 (1F7 command/status register)

  cmp edi, 1

  je .write_cmd

.read_cmd:
  mov  al, 0x20                # ATA READ SECTORS command
  jmp .send_cmd
  
.write_cmd:
  mov  al, 0x30                # ATA READ SECTORS command

.send_cmd:
  out  dx, al
  pop eax

  ret

/* -------------- READ -------------- */

.global read_ata_st_c

read_ata_st_c:
  push ebp                # standard C function prologue
  mov  ebp, esp

  push ebx                # save callee-saved registers
  push esi
  push edi

  mov  eax, [ebp + 8]     # arg1: relative LBA
  mov  edi, [ebp + 12]    # arg2: destination buffer pointer
  mov  ebx, [ebp + 16]    # arg3: sector count
  mov  esi, [ebp + 20]    # arg4: drive descriptor pointer

  call .read_ata_st       # call ASM ATA  read routine

  pop  edi                # restore registers
  pop  esi
  pop  ebx
  pop  ebp
  ret

.read_ata_st:
  push edi
  mov edi, 0
  call set_lba
  pop edi

  push eax
  mov DWORD PTR [max_sectors], 256

.read:
  in   al, dx                  # read status register
  test al, 0x80                # check BSY bit
  jne  .read                   # if busy, keep waiting
  test al, 0x08                # check DRQ bit
  je   .read                   # if data not ready, keep waiting

  sub  dl, 7                   # DX = base port (1F0 data register)
  cld
  mov  ecx, 256                # 256 words = 512 bytes (1 sector)
  rep  insw                    # read sector data → ES:EDI buffer

  add  dl, 7                   # restore DX to base+7 for next command status access

  dec  ebx
  dec DWORD PTR [max_sectors]
  cmp  ebx, 0
  je   .done
  cmp DWORD PTR [max_sectors], 0
  jne  .read

.read_next_sectors:
  pop  eax                     # restore absolute LBA
  add  eax, 256                # move to next LBA
  jmp  .read_ata_st

.done:
  pop  eax                     # clean up stack (absolute LBA)
  ret

/* -------------- WRITE -------------- */

.global write_ata_st_c

write_ata_st_c:
  push ebp
  mov  ebp, esp
  push ebx
  push esi
  push edi

  mov  eax, [ebp + 8]      # arg1: relative LBA
  mov  edi, [ebp + 12]     # arg2: source buffer pointer
  mov  ebx, [ebp + 16]     # arg3: sector count
  mov  esi, [ebp + 20]     # arg4: drive descriptor pointer

  call .write_ata_st

  pop  edi
  pop  esi
  pop  ebx
  pop  ebp
  ret

.write_ata_st:
  push edi
  mov edi, 1
  call set_lba
  pop edi
  
  push esi
  mov esi, edi

  push eax
  mov DWORD PTR [max_sectors], 256

.write:
  in   al, dx                  # read status register
  test al, 0x80                # check BSY bit
  jne  .write                  # if busy, keep waiting
  test al, 0x08                # check DRQ bit
  je   .write                  # if data not ready, keep waiting

  sub  dl, 7                   # DX = base port (1F0 data register)
  cld
  mov  ecx, 256                # 256 words = 512 bytes (1 sector)
  rep  outsw                   # write sector data

  add  dl, 7                   # restore DX to base+7 for next command status access

  dec  ebx
  dec DWORD PTR [max_sectors]
  cmp  ebx, 0
  je   .wait_flush
  cmp DWORD PTR [max_sectors], 0
  jne  .write

.write_next_sectors:
  pop esi
  pop eax
  add eax, 256
  add edi, 512
  jmp .write_ata_st


.wait_flush:
  in   al, dx
  test al, 0x80
  jne  .wait_flush

  pop esi
  pop eax
  ret