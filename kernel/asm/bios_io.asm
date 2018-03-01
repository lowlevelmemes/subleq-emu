extern real_routine

global bios_print

section .data

%define bios_print_size           bios_print_end - bios_print_bin
bios_print_bin:                   incbin "blobs/bios_print.bin"
bios_print_end:

section .text

bits 64

bios_print:
    ; void bios_print(const char *msg);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rbx, rdi
    mov rsi, bios_print_bin
    mov rcx, bios_print_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
