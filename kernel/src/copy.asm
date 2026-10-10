; Forward copies of n bytes for the translator's word copy loop, picked at
; boot by speed: SSE2, AVX (whose 256-bit moves need no AVX2), and AVX-512F.
; They use SIMD registers, which the screen redraw, the only other user,
; saves around itself. Each block is loaded whole before it is stored, so
; they also copy down over their source.
;
; void copy_xxx(void *d, const void *s, size_t n)
; rdi = d, rsi = s, rdx = n

global copy_sse2
global copy_avx
global copy_avx512

bits 64

section .text

; the last n bytes, under a block: words, then bytes
%macro copy_tail 0
    cmp rdx, 8
    jb %%bytes
%%words:
    mov rax, [rsi]
    mov [rdi], rax
    add rsi, 8
    add rdi, 8
    sub rdx, 8
    cmp rdx, 8
    jae %%words
%%bytes:
    test rdx, rdx
    jz %%done
%%byte:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    dec rdx
    jnz %%byte
%%done:
%endmacro

copy_sse2:
    cmp rdx, 64
    jb .tail
.block:
    movdqu xmm0, [rsi]
    movdqu xmm1, [rsi + 16]
    movdqu xmm2, [rsi + 32]
    movdqu xmm3, [rsi + 48]
    movdqu [rdi], xmm0
    movdqu [rdi + 16], xmm1
    movdqu [rdi + 32], xmm2
    movdqu [rdi + 48], xmm3
    add rsi, 64
    add rdi, 64
    sub rdx, 64
    cmp rdx, 64
    jae .block
.tail:
    copy_tail
    ret

copy_avx:
    cmp rdx, 128
    jb .tail
.block:
    vmovdqu ymm0, [rsi]
    vmovdqu ymm1, [rsi + 32]
    vmovdqu ymm2, [rsi + 64]
    vmovdqu ymm3, [rsi + 96]
    vmovdqu [rdi], ymm0
    vmovdqu [rdi + 32], ymm1
    vmovdqu [rdi + 64], ymm2
    vmovdqu [rdi + 96], ymm3
    add rsi, 128
    add rdi, 128
    sub rdx, 128
    cmp rdx, 128
    jae .block
    vzeroupper
.tail:
    copy_tail
    ret

copy_avx512:
    cmp rdx, 256
    jb .tail
.block:
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 zmm1, [rsi + 64]
    vmovdqu64 zmm2, [rsi + 128]
    vmovdqu64 zmm3, [rsi + 192]
    vmovdqu64 [rdi], zmm0
    vmovdqu64 [rdi + 64], zmm1
    vmovdqu64 [rdi + 128], zmm2
    vmovdqu64 [rdi + 192], zmm3
    add rsi, 256
    add rdi, 256
    sub rdx, 256
    cmp rdx, 256
    jae .block
    vzeroupper
.tail:
    copy_tail
    ret
