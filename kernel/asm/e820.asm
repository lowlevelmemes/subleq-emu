extern real_routine

global get_e820

section .data

%define e820_size           e820_end - e820_bin
e820_bin:                   incbin "blobs/e820.bin"
e820_end:

section .text

bits 64

get_e820:
    ; void get_e820(e820_entry_t *e820_map);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi
    mov rsi, e820_bin
    mov rcx, e820_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
