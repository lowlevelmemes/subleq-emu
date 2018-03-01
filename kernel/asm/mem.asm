extern real_routine

global detect_mem

section .data

%define detect_mem_size         detect_mem_end - detect_mem_bin
detect_mem_bin:                 incbin "blobs/detect_mem.bin"
detect_mem_end:

align 4
mem_size        dd  0

section .text

bits 64

detect_mem:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rsi, detect_mem_bin
    mov rcx, detect_mem_size
    mov rbx, mem_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    mov eax, dword [mem_size]
    xor rdx, rdx
    ret
