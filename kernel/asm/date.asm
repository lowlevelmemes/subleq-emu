extern real_routine

global get_time

%define kernel_phys_offset 0xffffffffc0000000

section .data

%define get_time_size           get_time_end - get_time_bin
get_time_bin:                   incbin "blobs/get_time.bin"
get_time_end:

date_and_time:
    .seconds db 0
    .minutes db 0
    .hours db 0
    .days db 0
    .months db 0
    .years db 0
    .centuries db 0

section .text

bcd_to_int:
    ; in: RBX = address of byte to convert
    push rax
    push rcx

    mov al, byte [rbx]      ; seconds
    and al, 00001111b
    mov cl, al
    mov al, byte [rbx]
    and al, 11110000b
    shr al, 4
    mov ch, 10
    mul ch
    add cl, al

    mov byte [rbx], cl

    pop rcx
    pop rax
    ret

get_time:
    ; void get_time(int *seconds, int *minutes, int *hours,
    ;               int *days, int *months, int *years);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    push rdi
    push rsi
    push rdx
    push rcx
    push r8
    push r9

    mov rbx, date_and_time
    mov rdi, kernel_phys_offset
    sub rbx, rdi
    mov rsi, get_time_bin
    mov rcx, get_time_size
    call real_routine

    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rsi
    pop rdi

    xor eax, eax
    mov r12, date_and_time

    mov rbx, date_and_time.seconds
    call bcd_to_int
    mov al, byte [r12+0]
    mov dword [rdi], eax

    mov rbx, date_and_time.minutes
    call bcd_to_int
    mov al, byte [r12+1]
    mov dword [rsi], eax

    mov rbx, date_and_time.hours
    call bcd_to_int
    mov al, byte [r12+2]
    mov dword [rdx], eax

    mov rbx, date_and_time.days
    call bcd_to_int
    mov al, byte [r12+3]
    mov dword [rcx], eax

    mov rbx, date_and_time.months
    call bcd_to_int
    mov al, byte [r12+4]
    mov dword [r8], eax

    mov rbx, date_and_time.years
    call bcd_to_int
    mov al, byte [r12+5]
    mov dword [r9], eax

    mov rbx, date_and_time.centuries
    call bcd_to_int
    mov al, byte [r12+6]
    mov cx, 100
    mul cx
    add dword [r9], eax

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
