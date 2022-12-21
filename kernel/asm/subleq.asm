global subleq.reentry

section .text

%macro loop_cycle 0
    pop rsi
    pop rbx
    bswap rsi
    bswap rbx
    mov rax, qword [rsi]
    mov rdx, qword [rbx]

    pop rdi

    cmp rdx, rax
    je %%eq

    bswap rax
    bswap rdx
    sub rdx, rax
    bswap rdx
    mov qword [rbx], rdx

    jg %%end
    jmp %%branch

  %%eq:
    mov qword [rbx], rcx

  %%branch:
    bswap rdi
    mov rsp, rdi

  %%end:
%endmacro

subleq_loop:
    mov qword [fs:18], 0

    xor rcx, rcx

  .start:
    loop_cycle
    loop_cycle
    loop_cycle

    cmp qword [fs:18], 0
    je .start
    jmp subleq.reentry

%macro pusham 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popam 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

extern shutdown
extern reboot
extern pm_sleep

global _readram
global _writeram

section .text

bits 64

global subleq
subleq:
    mov r11, qword [fs:0000]        ; uint64_t cpu_number;

    xor rsp, rsp       ; uint64_t eip = 0;
    mov r9, 1           ; int is_halted = 1;

    mov r10, 334364672  ; uint64_t cpu_bank;
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
        dq .shutdown
        times 7 dq .execute_cycle
        dq .reboot
        times 15 dq .execute_cycle
        dq .sleep

    .execute_cycle:
        ; eip = subleq_cycle(eip);

        jmp subleq_loop

    .reentry:
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
        hlt                     ; halt
        jmp .loop_allcpu               ; continue;

        .shutdown:
        cli
        mov rbp, rsp
        mov rsp, qword [fs:0008]
        sti
        mov rax, shutdown
        pusham
        call rax
        popam
        cli
        mov rsp, rbp
        sti
        jmp .execute_cycle

        .reboot:
        cli
        mov rbp, rsp
        mov rsp, qword [fs:0008]
        sti
        mov rax, reboot
        pusham
        call rax
        popam
        cli
        mov rsp, rbp
        sti
        jmp .execute_cycle

        .sleep:
        cli
        mov rbp, rsp
        mov rsp, qword [fs:0008]
        sti
        mov rax, pm_sleep
        pusham
        call rax
        popam
        cli
        mov rsp, rbp
        sti
        jmp .execute_cycle

_readram:
    mov rax, qword [rdi]
    bswap rax
    ret

_writeram:
    bswap rsi
    mov qword [rdi], rsi
    ret
