global subleq.reentry
global screen_redraw_avx2
global screen_redraw_avx512

; CPU feature flags (defined in dawn.c)
extern has_movbe
extern has_avx2
extern has_avx512

; Dawn OS memory-mapped I/O locations
%define DAWN_CPU_BANK_BASE  334364672   ; 0x13EE0000 - per-CPU status/EIP

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

section .text

; NOTE: We use R12 as the SUBLEQ instruction pointer (not RSP)
; This allows normal interrupt handling without needing IST

; Legacy loop cycle (no MOVBE) - for older CPUs
%macro loop_cycle 0
    ; Load operands in parallel (vs sequential pops)
    mov rsi, [r12]
    mov rbx, [r12+8]
    mov rdi, [r12+16]
    lea r12, [r12+24]

    ; Convert addresses to little-endian
    bswap rsi
    bswap rbx

    ; Load values
    mov rax, [rsi]
    mov rdx, [rbx]

    ; Convert to little-endian, compute, convert back
    bswap rax
    bswap rdx
    sub rdx, rax
    bswap rdx
    mov [rbx], rdx

    ; Branch if result <= 0
    jg %%no_branch
    bswap rdi
    mov r12, rdi
%%no_branch:
%endmacro

; Ultra-fast loop cycle with speculative execution and prefetching
; Key optimizations:
; 1. Speculative bswap of branch target (executes parallel to data loads)
; 2. Software prefetch for data 4 instructions ahead
; 3. Instruction reordering for maximum ILP
%macro loop_cycle_ultra 0
    ; === PHASE 1: Load instruction operands ===
    movbe rsi, [r12]         ; A address (swapped)
    movbe rbx, [r12+8]       ; B address (swapped)
    mov rdi, [r12+16]        ; C (branch target, raw)

    ; === PHASE 2: Speculative work while waiting for addresses ===
    ; Swap branch target NOW - will be ready if we need it
    ; This executes in parallel with the data loads below
    bswap rdi

    ; Prefetch data for instruction N+4 (hide memory latency)
    ; Load addresses for future instruction (96 bytes = 4 instructions ahead)
    movbe r13, [r12+96]
    movbe r14, [r12+104]

    ; Advance instruction pointer
    lea r12, [r12+24]

    ; Issue prefetches (non-blocking, hint to cache)
    prefetcht0 [r13]
    prefetcht0 [r14]

    ; === PHASE 3: Load data and compute ===
    movbe rax, [rsi]         ; Load A value
    movbe rdx, [rbx]         ; Load B value

    sub rdx, rax             ; B = B - A (sets flags)
    movbe [rbx], rdx         ; Store result

    ; === PHASE 4: Conditional branch ===
    jg %%no_branch           ; If result > 0, no branch
    mov r12, rdi             ; Branch taken (rdi already swapped)
%%no_branch:
%endmacro

; Standard fast cycle (no prefetch) - used when near potential branch targets
%macro loop_cycle_fast 0
    movbe rsi, [r12]
    movbe rbx, [r12+8]
    mov rdi, [r12+16]
    bswap rdi                ; Speculative swap
    lea r12, [r12+24]

    movbe rax, [rsi]
    movbe rdx, [rbx]

    sub rdx, rax
    movbe [rbx], rdx

    jg %%no_branch
    mov r12, rdi
%%no_branch:
%endmacro

subleq_loop:
    mov qword [fs:16], 0

    cmp qword [rel has_movbe], 0
    jne .start_fast

    ; Legacy path for CPUs without MOVBE (4x unroll)
    align 32
  .start:
    loop_cycle
    loop_cycle
    loop_cycle
    loop_cycle

    cmp qword [fs:16], 0
    je .start
    jmp subleq.reentry

    ; Ultra-fast path with MOVBE + prefetching
    ; 16x unroll: each group = 1 ultra + 3 fast = 4 instructions
    align 64
  .start_fast:
    loop_cycle_ultra        ; 1 instruction + prefetch
    loop_cycle_fast
    loop_cycle_fast
    loop_cycle_fast

    loop_cycle_ultra
    loop_cycle_fast
    loop_cycle_fast
    loop_cycle_fast

    loop_cycle_ultra
    loop_cycle_fast
    loop_cycle_fast
    loop_cycle_fast

    loop_cycle_ultra
    loop_cycle_fast
    loop_cycle_fast
    loop_cycle_fast

    cmp qword [fs:16], 0
    je .start_fast
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

section .text

bits 64

global subleq
subleq:
    mov r11, qword [fs:0000]        ; uint64_t cpu_number;

    xor r12, r12       ; uint64_t eip = 0; (now in r12, not rsp)
    mov r9, 1           ; int is_halted = 1;

    mov r10, DAWN_CPU_BANK_BASE  ; uint64_t cpu_bank;
    mov rax, r11
    shl rax, 4
    add r10, rax

    mov rax, 4              ; _writeram(cpu_bank + 0, 4);        // status
    bswap rax
    mov qword [r10], rax

    mov qword [r10 + 8], r12  ; _writeram(cpu_bank + 8, 0);        // EIP

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
        jmp subleq_loop

    .reentry:
        test r11, r11
        jz .loop_cpu0

        mov r8, r12
        bswap r8
        mov qword [r10 + 8], r8 ; _writeram(cpu_bank + 8, eip);
        jmp .loop_allcpu

        .case1:
        ; active
        test r9, r9             ; if (is_halted) {
        jz .execute_cycle
        xor r9, r9              ; is_halted = 0;
        mov r12, qword [r10 + 8] ; eip = _readram(cpu_bank + 8);
        bswap r12
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
        pusham
        call shutdown
        popam
        jmp .execute_cycle

        .reboot:
        pusham
        call reboot
        popam
        jmp .execute_cycle

        .sleep:
        pusham
        call pm_sleep
        popam
        jmp .execute_cycle

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
