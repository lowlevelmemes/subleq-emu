; This file contains the code that is gonna be linked at the beginning of
; the kernel binary.
; It should contain core CPU initialisation routines such as entering
; long mode, then it should call 'kernel_init'.

extern kernel_init
global startup

section .data

align 4096
early_pagemap:
    times (512 * (8 + 1 + 1 + 1)) dq 0
    ; 8 page tables (16 M)
    ; 1 page directory
    ; 1 pdpt
    ; 1 pml4

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

    ; prepare the page directory and page table
    ; identity map the first 16 MiB of RAM

    ; build the 8 identity mapped page tables starting at 0x10000
    mov edi, early_pagemap
    mov eax, 0x03
    mov ecx, 512 * 8
    .loop:
        stosd
        add eax, 0x1000
        mov dword [edi], 0
        add edi, 4
        loop .loop
    ; build the page directory
    mov edx, edi ; save starting address of page directory
    mov eax, (early_pagemap + 0x03)
    stosd
    xor eax, eax
    stosd
    mov eax, (early_pagemap + 0x1003)
    stosd
    xor eax, eax
    stosd
    mov eax, (early_pagemap + 0x2003)
    stosd
    xor eax, eax
    stosd
    mov eax, (early_pagemap + 0x3003)
    stosd
    xor eax, eax
    stosd
    mov eax, (early_pagemap + 0x4003)
    stosd
    xor eax, eax
    stosd
    mov eax, (early_pagemap + 0x5003)
    stosd
    xor eax, eax
    stosd
    mov eax, (early_pagemap + 0x6003)
    stosd
    xor eax, eax
    stosd
    mov eax, (early_pagemap + 0x7003)
    stosd
    xor eax, eax
    stosd
    add edi, (4096 - (8 * 8))
    ; build the pdpt
    mov eax, edx
    mov edx, edi
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    add edi, (4096 - (1 * 8))
    ; build the pml4
    mov eax, edx
    mov edx, edi
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

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
