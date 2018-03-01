%macro subleq_loop 1
    lodsq
    bswap rax
    mov r8, rax
    lodsq
    bswap rax
    mov r9, rax
    push rsi
    mov rsi, r9
    add rsi, rdx
    lodsq
    bswap rax
    mov r10, rax
    mov rsi, r8
    add rsi, rdx
    lodsq
    bswap rax
    sub r10, rax
    pop rsi
    cmp r10, 0
    jle .%1a
    add rsi, 8
    jmp .%1b
  .%1a:
    lodsq
    bswap rax
    mov rsi, rax
    add rsi, rdx
  .%1b:
    mov rdi, r9
    add rdi, rdx
    mov rax, r10
    bswap rax
    stosq
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

global subleq_cycle
subleq_cycle:
    ; RDI = ESP
    ; return
    ; RAX = ESP

    mov rcx, 4096 / 4
    mov rdx, initramfs
    mov rsi, rdi
    add rsi, rdx

  .main_loop:
    subleq_loop 1
    subleq_loop 2
    subleq_loop 3
    subleq_loop 4

    dec rcx
    test rcx, rcx
    jz .out
    mov rax, .main_loop
    jmp rax

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
