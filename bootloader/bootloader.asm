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

mov eax, 0						; Start from LBA sector 0
push 0x1000
pop es
mov ebx, 0x0					; Load to offset 0x1000:0x0000
mov ecx, 128						; Load 128 sectors (64k)
call read_sectors

jc err							; Catch any error

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

times 510-($-$$)			db 0x00				; Fill rest with 0x00
bios_signature				dw 0xAA55			; BIOS signature

incbin 'stage2.bin'

times (128*512)-($-$$)			db 0x00				; Padding

; bootloader is 64kb / 128 sectors

incbin '../Dawn/disk0.bin'
