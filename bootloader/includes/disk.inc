read_sector:

; ***********************************************
;     Reads a disk sector with an LBA address
; ***********************************************

; IN:
; EAX = LBA sector to load
; DL = Drive number
; BP = Buffer segment
; BX = Buffer offset
; CX = Count

; OUT:
; Carry if error

push eax
push ebx
push ecx
push edx
push esi
push edi
push ebp

mov word [.target_segment], bp
mov word [.target_offset], bx
mov dword [.lba_address_low], eax
mov word [.count], cx

xor esi, esi
mov si, .da_struct
mov ah, 0x42

clc										; Clear carry for int 0x13 because some BIOSes may not clear it on success

int 0x13								; Call int 0x13

.done:

pop ebp
pop edi
pop esi
pop edx
pop ecx
pop ebx
pop eax
ret										; Exit routine

align 4
.da_struct:
    .packet_size        db  16
    .unused             db  0
    .count              dw  0
    .target_offset      dw  0
    .target_segment     dw  0
    .lba_address_low    dd  0
    .lba_address_high   dd  0


%define SECTORS_AT_ONCE 16

read_sectors:

; ********************************************
;     Reads multiple LBA addressed sectors
; ********************************************

; IN:
; EAX = LBA starting sector
; DL = Drive number
; ES = Buffer segment
; EBX = Buffer offset
; ECX = Sectors count

; OUT:
; Carry if error

push eax									; Save GPRs
push ebx
push ecx
push edx
push esi
push edi
push ebp
push fs

push 0
pop fs

.loop:

push ebx
push ecx

mov bp, 0x7000							; Load in a temp buffer
xor bx, bx
mov cx, SECTORS_AT_ONCE

call read_sector						; Read sector

pop ecx
pop ebx

jc .done								; If carry exit with flag

mov edi, (512*SECTORS_AT_ONCE)/4
mov esi, 0x70000
.copy_loop:
mov ebp, dword [fs:esi]
mov dword [es:ebx], ebp
add esi, 4
add ebx, 4
dec edi
jnz .copy_loop

add eax, SECTORS_AT_ONCE				; Increment sector
sub ecx, SECTORS_AT_ONCE
jnz .loop								; Loop!

.done:
pop fs
pop ebp
pop edi
pop esi
pop edx
pop ecx									; Restore GPRs
pop ebx
pop eax
ret										; Exit routine
