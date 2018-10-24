extern startup
extern sections_text
extern sections_data_end
extern sections_bss_end
global _start

%define kernel_phys_offset 0xffffffff00000000

bits 32

section .multiboot
legacy_skip_header:
    mov ecx, sections_bss_end
    sub ecx, sections_data_end
    mov edi, sections_data_end
    xor eax, eax
    rep stosb

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
    .bss_end_addr dd sections_bss_end
    .entry_addr dd _start - kernel_phys_offset

section .text
_start:
    mov eax, startup - kernel_phys_offset
    jmp eax
