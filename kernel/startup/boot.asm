extern startup
extern sections_text
extern sections_data_end
extern sections_bss_end
global _start
global kstack.top

%define kernel_phys_offset 0xffffffff00000000

bits 32

section .multiboot
legacy_skip_header:
    mov eax, _start - kernel_phys_offset
    jmp eax

align 4
multiboot_header:
    .magic dd 0x1BADB002
    .flags dd 0x00010000
    .checksum dd -(0x1BADB002 + 0x00010000)
    .header_addr dd multiboot_header - kernel_phys_offset
    .load_addr dd sections_text
    .load_end_addr dd sections_data_end
    .bss_end_addr dd sections_data_end
    .entry_addr dd _start - kernel_phys_offset

section .data

align 16
kstack:
    times 0x10000 db 0
.top:

section .text
_start:
    mov esp, (kstack.top - 0x10) - kernel_phys_offset

    mov eax, startup - kernel_phys_offset
    jmp eax
