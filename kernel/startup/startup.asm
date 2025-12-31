; This file contains the code that is gonna be linked at the beginning of
; the kernel binary.
; It should contain core CPU initialisation routines such as entering
; long mode, then it should call 'kernel_init'.

extern kernel_init
global startup
global kernel_pagemap
global load_tss

%define kernel_phys_offset 0xffffffffc0000000

section .bss

align 16
kstack:
    resb 0x10000
.top:

align 4096

kernel_pagemap equ kernel_pagemap_t
kernel_pagemap_t:
.pml4:
    resq 512

.pdpt_phys:
    resq 512

.pd_phys:
    .pd_phys1:
    resq 512
    .pd_phys2:
    resq 512
    .pd_phys3:
    resq 512
    .pd_phys4:
    resq 512

.pdpt_low:
    resq 512

.pd_low:
    .pd_low1:
    resq 512
    .pd_low2:
    resq 512
    .pd_low3:
    resq 512
    .pd_low4:
    resq 512

.pdpt_kern:
    resq 512

.pd_kern:
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

    mov edi, kernel_pagemap_t.pd_low - kernel_phys_offset
    mov eax, 0x03 | (1 << 7)
    mov ecx, 512 * 4
    .loop1:
        stosd
        add eax, 0x200000
        mov dword [edi], 0
        add edi, 4
        loop .loop1

    mov edi, kernel_pagemap_t.pdpt_low - kernel_phys_offset
    mov eax, kernel_pagemap_t.pd_low1 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd_low2 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd_low3 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd_low4 - kernel_phys_offset
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

    mov edi, kernel_pagemap_t.pd_kern - kernel_phys_offset
    mov eax, 0x03 | (1 << 7)
    mov ecx, 512
    .loop2:
        stosd
        add eax, 0x200000
        mov dword [edi], 0
        add edi, 4
        loop .loop2

    mov edi, kernel_pagemap_t.pdpt_kern+(511*8) - kernel_phys_offset
    mov eax, kernel_pagemap_t.pd_kern - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pagemap_t.pml4+(511*8) - kernel_phys_offset
    mov eax, kernel_pagemap_t.pdpt_kern - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pagemap_t.pd_phys - kernel_phys_offset
    mov eax, 0x03 | (1 << 7)
    mov ecx, 512 * 4
    .loop3:
        stosd
        add eax, 0x200000
        mov dword [edi], 0
        add edi, 4
        loop .loop3

    mov edi, kernel_pagemap_t.pdpt_phys - kernel_phys_offset
    mov eax, kernel_pagemap_t.pd_phys1 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd_phys2 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd_phys3 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd
    mov eax, kernel_pagemap_t.pd_phys4 - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edi, kernel_pagemap_t.pml4+(256*8) - kernel_phys_offset
    mov eax, kernel_pagemap_t.pdpt_phys - kernel_phys_offset
    or eax, 0x03
    stosd
    xor eax, eax
    stosd

    mov edx, kernel_pagemap_t - kernel_phys_offset
    mov cr3, edx

    mov eax, cr4
    or eax, 1 << 5
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

    ; Enable SSE
    mov rax, cr0
    and ax, 0xFFFB          ; Clear CR0.EM (bit 2)
    or ax, 0x2              ; Set CR0.MP (bit 1)
    mov cr0, rax

    mov rax, cr4
    or ax, (1 << 9) | (1 << 10)  ; Set CR4.OSFXSR and CR4.OSXMMEXCPT
    mov cr4, rax

    ; Check for XSAVE (bit 26) and AVX (bit 28) support via CPUID
    mov eax, 1
    xor ecx, ecx
    cpuid
    mov eax, ecx            ; Save feature flags
    and eax, (1 << 26) | (1 << 28)
    cmp eax, (1 << 26) | (1 << 28)
    jne .no_avx             ; Need both XSAVE and AVX

    ; Enable OSXSAVE in CR4
    mov rax, cr4
    bts rax, 18             ; Set CR4.OSXSAVE (bit 18) without clobbering other bits
    mov cr4, rax

    ; Check for AVX-512F support (CPUID.07H:EBX bit 16)
    mov eax, 7
    xor ecx, ecx
    cpuid
    test ebx, (1 << 16)
    jz .avx_only

    ; Enable AVX-512 in XCR0 (bits 0,1,2,5,6,7)
    xor ecx, ecx            ; XCR0
    xgetbv
    or eax, 0xE7            ; Enable x87, SSE, AVX, opmask, ZMM_Hi256, Hi16_ZMM
    xsetbv
    jmp .no_avx

  .avx_only:
    ; Enable AVX in XCR0 (bits 0,1,2)
    xor ecx, ecx            ; XCR0
    xgetbv
    or eax, 0x7             ; Enable x87, SSE, AVX state
    xsetbv

  .no_avx:

    mov rdi, rbp
    mov rax, kernel_init
    call rax
