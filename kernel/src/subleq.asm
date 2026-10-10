global screen_redraw_sse2
global screen_redraw_avx2
global screen_redraw_avx512

section .data
    align 64
pixel_shuffle_mask:
    ; Transform big-endian ARGB to framebuffer format
    ; Equivalent to: ROL 8, BSWAP (maps byte positions: 0->2, 1->1, 2->0, 3->3)
    ; 64 bytes for AVX-512 (16 pixels), also works for AVX2 (first 32 bytes)
    db 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15
    db 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15
    db 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15
    db 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15
    ; For SSE2, which has no byte shuffle: bytes 1 and 3 stay, 0 and 2 swap
pixel_keep_mask:
    times 4 dd 0xff00ff00
pixel_low_mask:
    times 4 dd 0x000000ff

section .text

bits 64

; One instruction, with eip in r12, taking the branch when B <= A as signed
; numbers
%macro interp_cycle 0
    mov rsi, [r12]
    mov rbx, [r12+8]
    mov rdi, [r12+16]
    lea r12, [r12+24]
    bswap rsi
    bswap rbx
    mov rax, [rsi]
    mov rdx, [rbx]
    bswap rax
    bswap rdx
    sub rdx, rax
    bswap rdx
    mov [rbx], rdx
    jg %%no_branch
    bswap rdi
    mov r12, rdi
%%no_branch:
%endmacro

; uint64_t subleq_interp(uint64_t eip, uint64_t count), count a multiple of 4
global subleq_interp
subleq_interp:
    push rbx
    push r12
    mov r12, rdi
    mov rcx, rsi

    align 32
.loop:
    interp_cycle
    interp_cycle
    interp_cycle
    interp_cycle
    sub rcx, 4
    jnz .loop

    mov rax, r12
    pop r12
    pop rbx
    ret

; ============================================================================
; SSE2 screen redraw - processes 4 pixels at a time, swapping bytes 0 and 2
; of each with masks and shifts
; void screen_redraw_sse2(volatile uint32_t *dst, uint32_t *src,
;                         int width, int height, int dst_pitch, int src_pitch)
; rdi = dst, rsi = src, edx = width, ecx = height, r8d = dst_pitch, r9d = src_pitch
; ============================================================================
screen_redraw_sse2:
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save all parameters
    mov r10d, edx           ; width
    mov r11d, ecx           ; height
    mov r12d, r8d           ; dst_pitch (in pixels)
    mov r13d, r9d           ; src_pitch (in pixels)

    ; Load masks
    movdqa xmm14, [rel pixel_keep_mask]
    movdqa xmm15, [rel pixel_low_mask]

    xor r14d, r14d          ; y = 0

.sse2_row_loop:
    cmp r14d, r11d
    jge .sse2_done

    ; Calculate src_row = src + y * src_pitch (in pixels, then scale by 4)
    mov eax, r14d
    imul eax, r13d          ; y * src_pitch (pixels)
    lea rcx, [rsi + rax*4]  ; src + y * src_pitch * 4

    ; Calculate dst_row = dst + y * dst_pitch (in pixels, then scale by 4)
    mov eax, r14d
    imul eax, r12d          ; y * dst_pitch (pixels)
    lea rbx, [rdi + rax*4]  ; dst + y * dst_pitch * 4

    xor r15d, r15d          ; x = 0

.sse2_col_loop:
    mov eax, r10d
    sub eax, r15d           ; remaining = width - x
    cmp eax, 4
    jl .sse2_scalar_tail

    ; Process 4 pixels: (p & 0xff00ff00) | ((p >> 16) & 0xff) | ((p << 24) >> 8)
    movdqu xmm0, [rcx + r15*4]
    movdqa xmm1, xmm0
    movdqa xmm2, xmm0
    pand xmm0, xmm14
    psrld xmm1, 16
    pand xmm1, xmm15
    pslld xmm2, 24
    psrld xmm2, 8
    por xmm0, xmm1
    por xmm0, xmm2
    movdqu [rbx + r15*4], xmm0

    add r15d, 4
    jmp .sse2_col_loop

.sse2_scalar_tail:
    cmp r15d, r10d
    jge .sse2_next_row

    ; Process one pixel with scalar code
    mov eax, [rcx + r15*4]
    rol eax, 8
    bswap eax
    mov [rbx + r15*4], eax

    inc r15d
    jmp .sse2_scalar_tail

.sse2_next_row:
    inc r14d
    jmp .sse2_row_loop

.sse2_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; ============================================================================
; AVX2 screen redraw - processes 8 pixels at a time using VPSHUFB
; void screen_redraw_avx2(volatile uint32_t *dst, uint32_t *src,
;                         int width, int height, int dst_pitch, int src_pitch)
; rdi = dst, rsi = src, edx = width, ecx = height, r8d = dst_pitch, r9d = src_pitch
; ============================================================================
screen_redraw_avx2:
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save all parameters
    mov r10d, edx           ; width
    mov r11d, ecx           ; height
    mov r12d, r8d           ; dst_pitch (in pixels)
    mov r13d, r9d           ; src_pitch (in pixels)

    ; Load shuffle mask
    vmovdqu ymm15, [rel pixel_shuffle_mask]

    xor r14d, r14d          ; y = 0

.avx2_row_loop:
    cmp r14d, r11d
    jge .avx2_done

    ; Calculate src_row = src + y * src_pitch (in pixels, then scale by 4)
    mov eax, r14d
    imul eax, r13d          ; y * src_pitch (pixels)
    lea rcx, [rsi + rax*4]  ; src + y * src_pitch * 4

    ; Calculate dst_row = dst + y * dst_pitch (in pixels, then scale by 4)
    mov eax, r14d
    imul eax, r12d          ; y * dst_pitch (pixels)
    lea rbx, [rdi + rax*4]  ; dst + y * dst_pitch * 4

    xor r15d, r15d          ; x = 0

.avx2_col_loop:
    mov eax, r10d
    sub eax, r15d           ; remaining = width - x
    cmp eax, 8
    jl .avx2_scalar_tail

    ; Process 8 pixels with AVX2
    vmovdqu ymm0, [rcx + r15*4]
    vpshufb ymm0, ymm0, ymm15
    vmovdqu [rbx + r15*4], ymm0

    add r15d, 8
    jmp .avx2_col_loop

.avx2_scalar_tail:
    cmp r15d, r10d
    jge .avx2_next_row

    ; Process one pixel with scalar code
    mov eax, [rcx + r15*4]
    rol eax, 8
    bswap eax
    mov [rbx + r15*4], eax

    inc r15d
    jmp .avx2_scalar_tail

.avx2_next_row:
    inc r14d
    jmp .avx2_row_loop

.avx2_done:
    vzeroupper              ; Clear upper YMM bits
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; ============================================================================
; AVX-512 screen redraw - processes 16 pixels at a time using VPSHUFB
; void screen_redraw_avx512(volatile uint32_t *dst, uint32_t *src,
;                           int width, int height, int dst_pitch, int src_pitch)
; rdi = dst, rsi = src, edx = width, ecx = height, r8d = dst_pitch, r9d = src_pitch
; ============================================================================
screen_redraw_avx512:
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save all parameters
    mov r10d, edx           ; width
    mov r11d, ecx           ; height
    mov r12d, r8d           ; dst_pitch (in pixels)
    mov r13d, r9d           ; src_pitch (in pixels)

    ; Load 512-bit shuffle mask into zmm31
    vmovdqu64 zmm31, [rel pixel_shuffle_mask]

    xor r14d, r14d          ; y = 0

.avx512_row_loop:
    cmp r14d, r11d
    jge .avx512_done

    ; Calculate src_row = src + y * src_pitch (in pixels, then scale by 4)
    mov eax, r14d
    imul eax, r13d          ; y * src_pitch (pixels)
    lea rcx, [rsi + rax*4]  ; src + y * src_pitch * 4

    ; Calculate dst_row = dst + y * dst_pitch (in pixels, then scale by 4)
    mov eax, r14d
    imul eax, r12d          ; y * dst_pitch (pixels)
    lea rbx, [rdi + rax*4]  ; dst + y * dst_pitch * 4

    xor r15d, r15d          ; x = 0

.avx512_col_loop:
    mov eax, r10d
    sub eax, r15d           ; remaining = width - x
    cmp eax, 16
    jl .avx512_avx2_tail

    ; Process 16 pixels with AVX-512
    vmovdqu64 zmm0, [rcx + r15*4]
    vpshufb zmm0, zmm0, zmm31
    vmovdqu64 [rbx + r15*4], zmm0

    add r15d, 16
    jmp .avx512_col_loop

.avx512_avx2_tail:
    ; Fall back to AVX2 for 8-15 remaining pixels
    cmp eax, 8
    jl .avx512_scalar_tail

    vmovdqu ymm0, [rcx + r15*4]
    vpshufb ymm0, ymm0, ymm31   ; Uses lower 256 bits of zmm31
    vmovdqu [rbx + r15*4], ymm0

    add r15d, 8
    jmp .avx512_col_loop

.avx512_scalar_tail:
    cmp r15d, r10d
    jge .avx512_next_row

    ; Process one pixel with scalar code
    mov eax, [rcx + r15*4]
    rol eax, 8
    bswap eax
    mov [rbx + r15*4], eax

    inc r15d
    jmp .avx512_scalar_tail

.avx512_next_row:
    inc r14d
    jmp .avx512_row_loop

.avx512_done:
    vzeroupper              ; Clear upper bits of YMM/ZMM registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
