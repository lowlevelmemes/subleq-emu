%macro subleq_loop 1
    pop rsi
    pop rbx
    bswap rsi
    bswap rbx
    mov rax, qword [rsi]
    mov rdx, qword [rbx]
    bswap rax
    bswap rdx
    sub rdx, rax
    bswap rdx
    pop rax
    mov qword [rbx], rdx
    jg .%1a
    bswap rax
    mov rsp, rax
  .%1a:
%endmacro

extern initramfs
extern initramfs_end
extern kernel_pagemap
extern subleq_pagemap

extern reboot
extern shutdown

global zero_subleq_memory
global _readram
global _writeram

global initramfs_addr

%define kernel_phys_offset 0xffffffff00000000

section .data
initramfs_addr:
    dq initramfs

section .text

bits 64

global subleq
subleq:
    cli

    mov rax, subleq_pagemap
    mov cr3, rax

    ; switch to ring 3
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    push 0x23
    push 0
    push 0x202
    push 0x1b
    mov rax, .ring3
    push rax
    iretq

  .ring3:
    ;xor rsp, rsp       ; uint64_t eip = 0;
    mov r9, 1           ; int is_halted = 1;

    mov r10, 334364672  ; uint64_t cpu_bank;
    mov r11, qword [fs:0000]        ; uint64_t cpu_number;
    mov rax, r11
    shl rax, 4
    add r10, rax

    mov rax, 4              ; _writeram(cpu_bank + 0, 4);        // status
    bswap rax
    mov qword [r10], rax

    mov qword [r10 + 8], rsp  ; _writeram(cpu_bank + 8, 0);        // EIP

    test r11, r11
    jz .loop_cpu0

    .loop_allcpu:
        ; check status
        mov rax, qword [r10]
        bswap rax

        mov rbx, .jump_table0
        jmp [rbx + rax * 8]
        align 16
      .jump_table0:
        dq .loop_allcpu
        dq .case1
        dq .case2
        dq .loop_allcpu
        dq .case4

    .loop_cpu0:
        ; check status
        mov rax, qword [r10]
        bswap rax

        mov rbx, .jump_table1
        jmp [rbx + rax * 8]
        align 16
      .jump_table1:
        times 8 dq .execute_cycle
        dq shutdown
        times 7 dq .execute_cycle
        dq reboot

    .execute_cycle:
        ; eip = subleq_cycle(eip);

        mov rcx, 8192 / 3

        .main_loop:
            subleq_loop 1
            subleq_loop 2
            subleq_loop 3

            dec rcx
            jnz .main_loop

        test r11, r11
        jz .loop_cpu0

        mov r8, rsp
        bswap r8
        mov qword [r10 + 8], r8 ; _writeram(cpu_bank + 8, eip);
        jmp .loop_allcpu

        .case1:
        ; active
        test r9, r9             ; if (is_halted) {
        jz .execute_cycle
        xor r9, r9              ; is_halted = 0;
        mov rsp, qword [r10 + 8] ; eip = _readram(cpu_bank + 8);
        bswap rsp
        jmp .execute_cycle

        .case2:
        ; stop requested
        mov rax, 4
        bswap rax
        mov qword [r10], rax      ; _writeram(cpu_bank + 0, 4);

        .case4:
        ; halted
        mov r9, 1               ; is_halted = 1;
        int 0x82                ; yield
        jmp .loop_allcpu               ; continue;

zero_subleq_memory:
    push rbx
    push rbp

    ; zero out subleq mem
    mov rdi, initramfs_end
    mov rcx, 0x1a000000
    sub rcx, initramfs_end
    mov rax, 0
    rep stosb

    pop rbp
    pop rbx
    ret

_readram:
    mov rax, initramfs + kernel_phys_offset
    add rdi, rax
    mov rax, qword [rdi]
    bswap rax
    ret

_writeram:
    bswap rsi
    mov rax, initramfs + kernel_phys_offset
    add rdi, rax
    mov qword [rdi], rsi
    ret
