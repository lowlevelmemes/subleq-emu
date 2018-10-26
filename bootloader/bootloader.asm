org 0x7C00						; BIOS loads us here (0000:7C00)
bits 16							; 16-bit real mode code

cli
jmp 0x0000:initialise_cs		; Initialise CS to 0x0000 with a long jump
initialise_cs:
xor ax, ax
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax
mov sp, 0x7BF0
sti

mov si, LoadingMsg				; Print loading message using simple print (BIOS)
call simple_print

; ****************** Load stage 2 ******************

mov si, Stage2Msg				; Print loading stage 2 message
call simple_print

mov si, 128
mov bp, 0x1000
mov bx, 0x0					; Load to offset 0x1000:0x0000
mov eax, 0						; Start from LBA sector 0
mov cx, 16						; Load 128 sectors (64k)
load_stage2:
call read_sector
jc err							; Catch any error
add bx, 16*512
add eax, 16
sub si, 16
jnz load_stage2

mov si, DoneMsg
call simple_print				; Display done message

jmp 0x1000:0x0200						; Jump to stage 2

err:
mov si, ErrMsg
call simple_print

halt:
hlt
jmp halt

;Data

LoadingMsg		db 0x0D, 0x0A, 'Loading subleq-emu...', 0x0D, 0x0A, 0x0A, 0x00
Stage2Msg		db 'Loading emulator...', 0x00
ErrMsg			db 0x0D, 0x0A, 'Error, system halted.', 0x00
DoneMsg			db '  DONE', 0x0D, 0x0A, 0x00

;Includes

%include 'includes/simple_print.inc'
%include 'includes/disk.inc'

; Add a fake MBR because some motherboards won't boot otherwise

times 0x1b8-($-$$) db 0
mbr:
    .signature: dd 0xdeadbeef
    times 2 db 0
    .p1:
        db 0x80         ; status (active)
        db 0x20, 0x21, 0x00    ; CHS start
        db 0x83         ; partition type (Linux)
        db 0xb6, 0x25, 0x51    ; CHS end
        dd disk_start/512   ; LBA start
        dd disk_size/512    ; size in sectors
    .p2:
        db 0x00         ; status (invalid)
        times 3 db 0    ; CHS start
        db 0x00         ; partition type
        times 3 db 0    ; CHS end
        dd 00           ; LBA start
        dd 00           ; size in sectors
    .p3:
        db 0x00         ; status (invalid)
        times 3 db 0    ; CHS start
        db 0x00         ; partition type
        times 3 db 0    ; CHS end
        dd 00           ; LBA start
        dd 00           ; size in sectors
    .p4:
        db 0x00         ; status (invalid)
        times 3 db 0    ; CHS start
        db 0x00         ; partition type
        times 3 db 0    ; CHS end
        dd 00           ; LBA start
        dd 00           ; size in sectors


times 510-($-$$)			db 0x00				; Fill rest with 0x00
bios_signature				dw 0xAA55			; BIOS signature

incbin 'stage2.bin'

; bootloader is 64kb / 128 sectors
times (128*512)-($-$$)			db 0x00				; Padding

; padding for partition alignment
times (2048*512)-($-$$)			db 0x00				; Padding

disk_start equ ($ - $$)
disk_size equ (disk_end - disk_start)
incbin '../Dawn/disk0.bin'
disk_end equ ($ - $$)
