org 0x200           ; 1000:0200
bits 16

; ************************* STAGE 2 ************************
stage2:

mov ax, 0x1000
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax

; ***** Memory check *****

mem_check:

push edx

xor ebx, ebx
.loop:
mov eax, 0xe820
mov ecx, 24
mov edx, 0x534d4150
mov edi, .e820_entry
int 0x15
jc .nomem
test ebx, ebx
jz .nomem
cmp dword [.e820_entry + 16], 1
jne .loop
cmp dword [.e820_entry + 8], ((256+18)*1024*1024)
jb .loop
jmp .out

align 16
.e820_entry:
    times 32 db 0

.nomem:
mov si, NoMemMsg
call simple_print
jmp err

.out:
mov ebp, dword [.e820_entry]
add ebp, 0x1200000
and ebp, ~(0x200000-1)
pop edx

; ***** A20 *****

mov si, A20Msg					; Display A20 message
call simple_print

call enable_a20					; Enable the A20 address line to access the full memory
jc err							; If it fails, print an error and halt

mov si, DoneMsg
call simple_print				; Display done message

; ***** Unreal Mode *****

mov si, UnrealMsg				; Display unreal message
call simple_print

lgdt [GDT]						; Load the GDT

; enter unreal mode

cli						; Disable interrupts

mov eax, cr0			; Enable bit 0 of cr0 and enter protected mode
or eax, 00000001b
mov cr0, eax

mov ax, 0x10
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax

mov eax, cr0			; Exit protected mode
and eax, 11111110b
mov cr0, eax

mov ax, 0x1000
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax

sti						; Enable interrupts

mov si, DoneMsg
call simple_print				; Display done message

; ***** Kernel *****

; Load the kernel to 0x100000 (1 MiB)

mov si, KernelMsg				; Show loading kernel message
call simple_print

mov ecx, kernel_size
push 0
pop es
mov edi, 0x100000
mov esi, kernel
a32 o32 rep movsb

mov si, DoneMsg
call simple_print				; Display done message

; ****************** Load Dawn ******************

mov si, DawnMsg				; Print loading Dawn message
call simple_print

mov ax, 2048					; Start from LBA sector 2048
push 0x0
pop es
mov ebx, ebp
mov ecx, ((256*1024*1024)/512)
call read_sectors

jc err							; Catch any error

mov si, DoneMsg
call simple_print				; Display done message

; enter pmode

cli						; Disable interrupts

push 0
pop es
mov esi, .trampoline
mov ecx, .trampoline_end - .trampoline
mov edi, 0x8000
rep movsb

mov eax, cr0			; Enable bit 0 of cr0 and enter protected mode
or eax, 00000001b
mov cr0, eax

jmp 0x18:0x8000

bits 32

.trampoline:

mov ax, 0x20
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax

; *** Setup registers ***

mov esp, 0xEFFFF0

xor eax, eax
xor ebx, ebx
xor ecx, ecx
xor edx, edx
xor esi, esi
xor edi, edi

jmp 0x18:0x100000					; Jump to the newly loaded kernel

.trampoline_end:

bits 16
err:
mov si, ErrMsg
call simple_print
.halt:
hlt
jmp .halt

;Data

A20Msg			db 'Enabling A20 line...', 0x00
UnrealMsg		db 'Entering Unreal Mode...', 0x00
KernelMsg		db 'Loading kernel...', 0x00
DawnMsg         db 'Loading Dawn (please be patient)...', 0x00
NoMemMsg        db 0x0D, 0x0A, 'Not enough memory to run subleq-emu.', 0x0d, 0x0a, 0
ErrMsg			db 0x0D, 0x0A, 'Error, system halted.', 0x00
DoneMsg			db '  DONE', 0x0D, 0x0A, 0x00

;Includes

bits 16
%include 'includes/simple_print.inc'
%include 'includes/disk.inc'
%include 'includes/a20_enabler.inc'
%include 'includes/gdt.inc'

align 16
kernel:
incbin '../kernel/subleq.bin'
.end:
kernel_size equ (kernel.end - kernel)
