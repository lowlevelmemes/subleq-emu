%macro subleq_loop 1
    lodsq
    bswap rax
    mov rbx, qword [rax+rdx]
    bswap rbx
    lodsq
    bswap rax
    mov rdi, qword [rax+rdx]
    bswap rdi
    sub rdi, rbx
    bswap rdi
    mov qword [rax+rdx], rdi
    js .%1a
    jz .%1a
    lodsq
    jmp .%1b
  .%1a:
    lodsq
    bswap rax
    lea rsi, [rax+rdx]
  .%1b:
%endmacro

extern initramfs

global _readram
global _writeram

global initramfs_addr

section .data
initramfs_addr:
    dq initramfs

section .text

bits 64

global subleq
subleq:
    xor r8, r8          ; uint64_t eip = 0;
    mov r9, 1           ; int is_halted = 1;

    mov r10, initramfs + 334364672  ; uint64_t cpu_bank;
    mov r11, qword [fs:0000]        ; uint64_t cpu_number;
    mov rax, r11
    shl rax, 4
    add r10, rax

    mov rax, 4              ; _writeram(cpu_bank + 0, 4);        // status
    bswap rax
    mov qword [r10], rax

    mov qword [r10 + 8], 0  ; _writeram(cpu_bank + 8, 0);        // EIP

    .loop:
        test r11, r11
        jz .execute_cycle

        ; check status
        mov rax, qword [r10]
        bswap rax

        test rax, rax
        jz .loop

        cmp rax, 1
        je .case1

        cmp rax, 2
        je .case2

        cmp rax, 4
        je .case4

        jmp .loop

        .execute_cycle:
        mov rdi, r8         ; eip = subleq_cycle(eip);
        call subleq_cycle
        mov r8, rax

        bswap rax
        mov qword [r10 + 8], rax ; _writeram(cpu_bank + 8, eip);
        jmp .loop

        .case1:
        ; active
        test r9, r9             ; if (is_halted) {
        jz .execute_cycle
        xor r9, r9              ; is_halted = 0;
        mov r8, qword [r10 + 8] ; eip = _readram(cpu_bank + 8);
        bswap r8
        jmp .execute_cycle

        .case2:
        ; stop requested
        mov rax, 4
        bswap rax
        mov qword [r10], rax      ; _writeram(cpu_bank + 0, 4);

        .case4:
        ; halted
        mov r9, 1               ; is_halted = 1;
        jmp .loop               ; continue;

subleq_cycle:
    ; RDI = ESP
    ; return
    ; RAX = ESP

    mov rcx, 4096 / 4
    mov rdx, initramfs
    lea rsi, [rdi+rdx]

  .main_loop:
    subleq_loop 1
    subleq_loop 2
    subleq_loop 3
    subleq_loop 4

    dec rcx
    jnz .main_loop

  .out:
    sub rsi, rdx
    mov rax, rsi
    ret

_readram:
    add edi, initramfs
    mov rax, qword [rdi]
    bswap rax
    ret

_writeram:
    bswap rsi
    add edi, initramfs
    mov qword [rdi], rsi
    ret
