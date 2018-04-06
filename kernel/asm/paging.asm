global full_identity_map
global kernel_pagemap

extern initramfs_end

section .data

section .text

bits 64

full_identity_map:

    ; Identity map all 4GB of memory for the kernel

    push rbx
    push rbp

    ; zero out subleq mem
    mov rdi, initramfs_end
    mov rcx, 0x1a000000
    sub rcx, initramfs_end
    mov rax, 0
    rep stosb

    pop rbp
    pop rbx
    ret
