; This file contains the code that is gonna be linked at the beginning of
; the kernel binary.
; It should contain core CPU initialisation routines such as entering
; long mode, then it should call 'kernel_init'.

extern kernel_init
extern initramfs
global startup
global kernel_pagemap
global kernel_pagemap_tables
global subleq_pagemap

section .data

align 4096

kernel_pagemap_tables:
kernel_pml4:
    times 512 dq 0

kernel_pdpt_low:
    times 512 dq 0

kernel_pdpt_hi:
    times 512 dq 0

kernel_pd:
    .1:
    times 512 dq 0
    .2:
    times 512 dq 0
    .3:
    times 512 dq 0
    .4:
    times 512 dq 0

subleq_pagemap:
.pml4:
    times 512 dq 0

.pdpt_low:
    times 512 dq 0

.pd:
    .pd1:
    times 512 dq 0
    .pd2:
    times 512 dq 0
    .pd3:
    times 512 dq 0
    .pd4:
    times 512 dq 0

kernel_pagemap dq kernel_pagemap_tables

align 16
GDT:

dw .GDTEnd - .GDTStart - 1	; GDT size
dd .GDTStart				; GDT start

align 16
.GDTStart:

; Null descriptor (required)

.NullDescriptor:

dw 0x0000			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 00000000b		; Access
db 00000000b		; Granularity
db 0x00				; Base (high 8 bits)

; Protected mode

.KernelCode64:

dw 0x0000			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10011010b		; Access
db 00100000b		; Granularity
db 0x00				; Base (high 8 bits)

.KernelData64:

dw 0x0000			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10010010b		; Access
db 00000000b		; Granularity
db 0x00				; Base (high 8 bits)

; Protected mode

.UserCode64:

dw 0x0000			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 11111010b		; Access
db 00100000b		; Granularity
db 0x00				; Base (high 8 bits)

.UserData64:

dw 0x0000			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 11110010b		; Access
db 00000000b		; Granularity
db 0x00				; Base (high 8 bits)

; Unreal mode

.UnrealCode:

dw 0xFFFF			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10011010b		; Access
db 10001111b		; Granularity
db 0x00				; Base (high 8 bits)

.UnrealData:

dw 0xFFFF			; Limit
dw 0x0000			; Base (low 16 bits)
db 0x00				; Base (mid 8 bits)
db 10010010b		; Access
db 10001111b		; Granularity
db 0x00				; Base (high 8 bits)

.GDTEnd:

section .startup

bits 32

nolongmode:
    call clearscreen
    mov esi, .msg
    call textmodeprint
    .halt:
        cli
        hlt
        jmp .halt

.msg    db  "This CPU does not support long mode.", 0

textmodeprint:
    pusha
    mov edi, 0xb8000
    .loop:
        lodsb
        test al, al
        jz .out
        stosb
        inc edi
        jmp .loop
    .out:
    popa
    ret

clearscreen:
    ; clear screen
    pusha
    mov edi, 0xb8000
    mov ecx, 80*25
    mov al, ' '
    mov ah, 0x17
    rep stosw
    popa
    ret

startup:
    ; check if long mode is present
    mov eax, 0x80000001
    xor edx, edx
    cpuid
    and edx, 1 << 29
    test edx, edx
    jz nolongmode

    ; load the GDT
    lgdt [GDT]

    mov edi, subleq_pagemap.pd
    mov eax, initramfs
    or eax, 0x03 | (1 << 7)
    mov ecx, 512 * 4
    .loop0:
        stosd
        add eax, 0x200000
        mov dword [edi], 0
        add edi, 4
        loop .loop0

    mov edi, subleq_pagemap.pdpt_low
    mov eax, subleq_pagemap.pd1
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, subleq_pagemap.pd2
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, subleq_pagemap.pd3
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, subleq_pagemap.pd4
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, subleq_pagemap.pml4
    mov eax, subleq_pagemap.pdpt_low
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, subleq_pagemap.pml4+(511*8)
    mov eax, kernel_pdpt_hi
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pd
    mov eax, 0x03 | (1 << 7)
    mov ecx, 512 * 4
    .loop1:
        stosd
        add eax, 0x200000
        mov dword [edi], 0
        add edi, 4
        loop .loop1

    mov edi, kernel_pdpt_low
    mov eax, kernel_pd.1
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pd.2
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pd.3
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pd.4
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pdpt_hi+(508*8)
    mov eax, kernel_pd.1
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pd.2
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pd.3
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pd.4
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pml4
    mov eax, kernel_pdpt_low
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pml4+(511*8)
    mov eax, kernel_pdpt_hi
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edx, kernel_pml4
    mov cr3, edx

    mov eax, 10100000b
    mov cr4, eax

    mov ecx, 0xc0000080
    rdmsr

    or eax, 0x00000100
    wrmsr

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    jmp 0x08:.mode64
    .mode64:
    bits 64
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; enable SSE
    mov rax, cr0
    and al, 0xfb
    or al, 0x02
    mov cr0, rax
    mov rax, cr4
    or ax, 3 << 9
    mov cr4, rax

    call kernel_init
