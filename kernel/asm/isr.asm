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

; IDT hooks
; ... CPU exceptions
global handler_irq_apic
global handler_irq_pic0
global handler_irq_pic1
global handler_div0
global handler_debug
global handler_nmi
global handler_breakpoint
global handler_overflow
global handler_bound_range_exceeded
global handler_invalid_opcode
global handler_device_not_available
global handler_double_fault
global handler_coprocessor_segment_overrun
global handler_invalid_tss
global handler_segment_not_present
global handler_stack_segment_fault
global handler_gpf
global handler_pf
global handler_x87_exception
global handler_alignment_check
global handler_machine_check
global handler_simd_exception
global handler_virtualisation_exception
global handler_security_exception
; ... misc
global irq0_handler
global keyboard_isr

; CPU exception handlers
extern except_div0
extern except_debug
extern except_nmi
extern except_breakpoint
extern except_overflow
extern except_bound_range_exceeded
extern except_invalid_opcode
extern except_device_not_available
extern except_double_fault
extern except_coprocessor_segment_overrun
extern except_invalid_tss
extern except_segment_not_present
extern except_stack_segment_fault
extern except_gen_prot_fault
extern except_page_fault
extern except_x87_exception
extern except_alignment_check
extern except_machine_check
extern except_simd_exception
extern except_virtualisation_exception
extern except_security_exception

; misc external references
extern kernel_pagemap
extern eoi
extern timer_interrupt
extern keyboard_handler

section .text

bits 64

handler_irq_apic:
        pusham
        call eoi
        popam
        iretq

handler_irq_pic0:
        push rax
        mov al, 0x20    ; acknowledge interrupt to PIC0
        out 0x20, al
        pop rax
        iretq

handler_irq_pic1:
        push rax
        mov al, 0x20    ; acknowledge interrupt to both PICs
        out 0xA0, al
        out 0x20, al
        pop rax
        iretq

handler_div0:
        pop rdi
        pop rsi
        call except_div0

handler_debug:
        pop rdi
        pop rsi
        call except_debug

handler_nmi:
        pop rdi
        pop rsi
        call except_nmi

handler_breakpoint:
        pop rdi
        pop rsi
        call except_breakpoint

handler_overflow:
        pop rdi
        pop rsi
        call except_overflow

handler_bound_range_exceeded:
        pop rdi
        pop rsi
        call except_bound_range_exceeded

handler_invalid_opcode:
        pop rdi
        pop rsi
        call except_invalid_opcode

handler_device_not_available:
        pop rdi
        pop rsi
        call except_device_not_available

handler_double_fault:
        pop rdi
        pop rsi
        pop rdx
        call except_double_fault

handler_coprocessor_segment_overrun:
        pop rdi
        pop rsi
        call except_coprocessor_segment_overrun

handler_invalid_tss:
        pop rdi
        pop rsi
        pop rdx
        call except_invalid_tss

handler_segment_not_present:
        pop rdi
        pop rsi
        pop rdx
        call except_segment_not_present

handler_stack_segment_fault:
        pop rdi
        pop rsi
        pop rdx
        call except_stack_segment_fault

handler_gpf:
        pop rdi
        pop rsi
        pop rdx
        call except_gen_prot_fault

handler_pf:
        pop rdi
        pop rsi
        pop rdx
        call except_page_fault

handler_x87_exception:
        pop rdi
        pop rsi
        call except_x87_exception

handler_alignment_check:
        pop rdi
        pop rsi
        pop rdx
        call except_alignment_check

handler_machine_check:
        pop rdi
        pop rsi
        call except_machine_check

handler_simd_exception:
        pop rdi
        pop rsi
        call except_simd_exception

handler_virtualisation_exception:
        pop rdi
        pop rsi
        call except_virtualisation_exception

handler_security_exception:
        pop rdi
        pop rsi
        call except_security_exception

extern get_cpu_number

local_irq0_handler:
        call eoi
        popam
        iretq

irq0_handler:
        ; first execute all the time-based routines (tty refresh...)
        pusham
        call get_cpu_number
        test rax, rax
        jnz local_irq0_handler
        call timer_interrupt
        call eoi
        popam
        iretq

keyboard_isr:
        pusham
        xor rax, rax
        in al, 0x60     ; read from keyboard
        mov rdi, rax
        call keyboard_handler
        call eoi
        popam
        iretq
