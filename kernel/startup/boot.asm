extern startup
extern sections_text
extern sections_data_end
extern sections_bss_end
global _start

bits 32

section .startup
_start:
    mov ecx, sections_bss_end
    sub ecx, sections_data_end
    mov edi, sections_data_end
    xor eax, eax
    rep stosb

    jmp startup
