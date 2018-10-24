global load_IDT

extern handler_simple
extern handler_code
extern handler_irq_apic
extern handler_irq_pic0
extern handler_irq_pic1
extern handler_div0
extern handler_debug
extern handler_nmi
extern handler_breakpoint
extern handler_overflow
extern handler_bound_range_exceeded
extern handler_invalid_opcode
extern handler_device_not_available
extern handler_double_fault
extern handler_coprocessor_segment_overrun
extern handler_invalid_tss
extern handler_segment_not_present
extern handler_stack_segment_fault
extern handler_gpf
extern handler_pf
extern handler_x87_exception
extern handler_alignment_check
extern handler_machine_check
extern handler_simd_exception
extern handler_virtualisation_exception
extern handler_security_exception
extern irq0_handler
extern keyboard_isr
extern mouse_isr

extern handler_wakeup

section .data

align 4
IDT:
    dw .IDTEnd - .IDTStart - 1	; IDT size
    dq .IDTStart				; IDT start

    align 4
    .IDTStart:
        times 0x100 dq 0,0 ; 64 bit IDT entries are 128 bit
    .IDTEnd:

section .text

bits 64

make_entry:
; RBX = address
; CX = selector
; DL = type
; DH = IST
; DI = vector

    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push r8

    push rdx

    mov rax, 16
    and rdi, 0x0000FFFF
    mul rdi
    mov r8, IDT.IDTStart
    add rax, r8
    mov rdi, rax

    mov ax, bx
    stosw
    mov ax, cx
    stosw
    pop rdx
    mov al, dh
    stosb
    mov al, dl
    stosb
    shr rbx, 16
    mov ax, bx
    stosw
    shr rbx, 16
    mov eax, ebx
    stosd
    xor eax, eax
    stosd

    pop r8
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    ret

load_IDT:
    push rbx
    push rcx
    push rdx
    push rdi

    xor di, di
    mov dl, 10001110b
    mov dh, 1
    mov cx, 0x08
    mov rbx, handler_div0
    call make_entry                 ; int 0x00, divide by 0

    inc di
    mov rbx, handler_debug
    call make_entry                 ; int 0x01, debug

    inc di
    mov rbx, handler_nmi
    call make_entry                 ; int 0x02, NMI

    inc di
    mov rbx, handler_breakpoint
    call make_entry                 ; int 0x03, breakpoint

    inc di
    mov rbx, handler_overflow
    call make_entry                 ; int 0x04, overflow

    inc di
    mov rbx, handler_bound_range_exceeded
    call make_entry                 ; int 0x05, bound range exceeded

    inc di
    mov rbx, handler_invalid_opcode
    call make_entry                 ; int 0x06, invalid opcode

    inc di
    mov rbx, handler_device_not_available
    call make_entry                 ; int 0x07, device not available

    inc di
    mov rbx, handler_double_fault
    call make_entry                 ; int 0x08, double fault

    inc di
    mov rbx, handler_coprocessor_segment_overrun
    call make_entry                 ; int 0x09, coprocessor segment overrun

    inc di
    mov rbx, handler_invalid_tss
    call make_entry                 ; int 0x0A, invalid TSS

    inc di
    mov rbx, handler_segment_not_present
    call make_entry                 ; int 0x0B, segment not present

    inc di
    mov rbx, handler_stack_segment_fault
    call make_entry                 ; int 0x0C, stack-segment fault

    inc di
    mov rbx, handler_gpf
    call make_entry                 ; int 0x0D, general protection fault

    inc di
    mov rbx, handler_pf
    call make_entry                 ; int 0x0E, page fault

    add di, 2
    mov rbx, handler_x87_exception
    call make_entry                 ; int 0x10, x87 floating point exception

    inc di
    mov rbx, handler_alignment_check
    call make_entry                 ; int 0x11, alignment check

    inc di
    mov rbx, handler_machine_check
    call make_entry                 ; int 0x12, machine check

    inc di
    mov rbx, handler_simd_exception
    call make_entry                 ; int 0x13, SIMD floating point exception

    mov di, 0x1d
    mov rbx, handler_virtualisation_exception
    call make_entry                 ; int 0x14, virtualisation exception

    inc di
    mov rbx, handler_security_exception
    call make_entry                 ; int 0x1E, security exception

    add di, 2
    mov rbx, irq0_handler
    call make_entry

    inc di
    mov rbx, keyboard_isr
    call make_entry

    inc di
    mov rbx, handler_irq_apic
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    mov rbx, mouse_isr
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    mov di, 0x80
    mov rbx, handler_wakeup
    call make_entry

    mov di, 0x81
    mov rbx, handler_abort
    call make_entry

    mov di, 0xa0
    mov rbx, handler_irq_pic0
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    mov rbx, handler_irq_pic1
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    mov di, 0x90
    mov rbx, handler_irq_apic
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    inc di
    call make_entry

    mov di, 0xff
    call make_entry

    mov rbx, IDT
    lidt [rbx]

    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

handler_abort:
    cli
    hlt
    jmp handler_abort
