; Entry and exit of code translated by jit.c
;
; Translated code runs with:
;   r15 = 0x80000000, Dawn address X being [r15 + (X - 0x80000000)]
;   r14 = static field bitmap, one bit per 8-byte word
;   r12 = jit_exit, which every way out jumps to
;   rbp = instruction budget
; and leaves with r13 = next eip, rbx = exit info.

bits 64

section .text

global jit_enter
global jit_exit

; void jit_enter(uint8_t *code, jit_state_t *st)
; st: eip at 0, budget at 8, exit at 16, bitmap at 24
jit_enter:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    push rsi
    mov r15, 0x80000000
    mov r14, [rsi + 24]
    mov rbp, [rsi + 8]
    lea r12, [rel jit_exit]
    jmp rdi

jit_exit:
    pop rsi
    mov [rsi], r13
    mov [rsi + 8], rbp
    mov [rsi + 16], rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
