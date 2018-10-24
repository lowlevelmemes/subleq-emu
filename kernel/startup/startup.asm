; This file contains the code that is gonna be linked at the beginning of
; the kernel binary.
; It should contain core CPU initialisation routines such as entering
; long mode, then it should call 'kernel_init'.

extern kernel_init
extern initramfs
global startup
global kernel_pagemap
global subleq_pagemap
global load_tss

%define kernel_phys_offset 0xffffffff00000000

section .bss

align 16
kstack:
    resb 0x10000
.top:

align 4096

kernel_pagemap equ kernel_pagemap_t - kernel_phys_offset
kernel_pagemap_t:
.pml4:
    resq 512

.pdpt_low:
    resq 512

.pdpt_hi:
    resq 512

.pd:
    .pd1:
    resq 512
    .pd2:
    resq 512
    .pd3:
    resq 512
    .pd4:
    resq 512

align 4096

subleq_pagemap equ subleq_pagemap_t - kernel_phys_offset
subleq_pagemap_t:
.pml4:
    resq 512

.pdpt_low:
    resq 512

.pd:
    .pd1:
    resq 512
    .pd2:
    resq 512
    .pd3:
    resq 512
    .pd4:
    resq 512

section .data

align 16
GDT:

dw .GDTEnd - .GDTStart - 1	; GDT size
dq .GDTStart				; GDT start

align 16
.GDT_ptrlow:

dw .GDTEnd - .GDTStart - 1	; GDT size
dd .GDTStart - kernel_phys_offset	; GDT start

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

; 64 bit mode

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

; tss
.tss:
    dw 104              ; tss length
  .tss_low:
    dw 0
  .tss_mid:
    db 0
  .tss_flags1:
    db 10001001b
  .tss_flags2:
    db 00000000b
  .tss_high:
    db 0
  .tss_upper32:
    dd 0
  .tss_reserved:
    dd 0

.GDTEnd:

section .text

bits 64

load_tss:
    ; addr in RDI
    push rbx
    mov eax, edi
    mov rbx, GDT.tss_low
    mov word [rbx], ax
    mov eax, edi
    and eax, 0xff0000
    shr eax, 16
    mov rbx, GDT.tss_mid
    mov byte [rbx], al
    mov eax, edi
    and eax, 0xff000000
    shr eax, 24
    mov rbx, GDT.tss_high
    mov byte [rbx], al
    mov rax, rdi
    shr rax, 32
    mov rbx, GDT.tss_upper32
    mov dword [rbx], eax
    mov rbx, GDT.tss_flags1
    mov byte [rbx], 10001001b
    mov rbx, GDT.tss_flags2
    mov byte [rbx], 0
    pop rbx
    ret

bits 32

nolongmode:
    call clearscreen
    mov esi, .msg - kernel_phys_offset
    call textmodeprint
    .halt:
        cli
        hlt
        jmp .halt

section .data

.msg    db  "This CPU does not support long mode.", 0

section .text

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
    mov esp, kstack.top - kernel_phys_offset

    ; check if long mode is present
    mov eax, 0x80000001
    xor edx, edx
    cpuid
    and edx, 1 << 29
    test edx, edx
    jz nolongmode

    ; load the GDT
    mov ebx, GDT.GDT_ptrlow - kernel_phys_offset
    lgdt [ebx]

    mov edi, subleq_pagemap_t.pd - kernel_phys_offset
    mov eax, initramfs
    or eax, 0x03 | (1 << 7)
    mov ecx, 512 * 4
    .loop0:
        stosd
        add eax, 0x200000
        mov dword [edi], 0
        add edi, 4
        loop .loop0

    mov edi, subleq_pagemap_t.pdpt_low - kernel_phys_offset
    mov eax, subleq_pagemap_t.pd1 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, subleq_pagemap_t.pd2 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, subleq_pagemap_t.pd3 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, subleq_pagemap_t.pd4 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, subleq_pagemap_t.pml4 - kernel_phys_offset
    mov eax, subleq_pagemap_t.pdpt_low - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, subleq_pagemap_t.pml4+(511*8) - kernel_phys_offset
    mov eax, kernel_pagemap_t.pdpt_hi - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pagemap_t.pd - kernel_phys_offset
    mov eax, 0x03 | (1 << 7)
    mov ecx, 512 * 4
    .loop1:
        stosd
        add eax, 0x200000
        mov dword [edi], 0
        add edi, 4
        loop .loop1

    mov edi, kernel_pagemap_t.pdpt_low - kernel_phys_offset
    mov eax, kernel_pagemap_t.pd1 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd2 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd3 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd4 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pagemap_t.pdpt_hi+(508*8) - kernel_phys_offset
    mov eax, kernel_pagemap_t.pd1 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd2 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd3 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd4 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pagemap_t.pml4 - kernel_phys_offset
    mov eax, kernel_pagemap_t.pdpt_low - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pagemap_t.pml4+(511*8) - kernel_phys_offset
    mov eax, kernel_pagemap_t.pdpt_hi - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edx, kernel_pagemap
    mov cr3, edx

    mov eax, 00100000b
    mov cr4, eax

    mov ecx, 0xc0000080
    rdmsr

    or eax, 0x00000100
    wrmsr

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    jmp 0x08:.mode64 - kernel_phys_offset
  .mode64:
    bits 64
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rax, .higher_half
    jmp rax
  .higher_half:
    mov rsp, kstack.top

    mov rbx, GDT
    lgdt [rbx]

    mov rax, kernel_init
    call rax
