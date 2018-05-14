OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0xffffffff00000000 + 0x100000;

    .text ALIGN(4K) :
    {
        sections_text = . - 0xffffffff00000000;
        *(.multiboot)
        *(.startup)
        *(.text)
        sections_text_end = . - 0xffffffff00000000;
    }

    .data ALIGN(4K) :
    {
        sections_data = . - 0xffffffff00000000;
        *(.data)
        *(.rodata)
        *(COMMON)
        *(.bss)
    }

    .initramfs ALIGN(2M) :
    {
        initramfs = . - 0xffffffff00000000;
        *(.initramfs)
        initramfs_end = . - 0xffffffff00000000;
        sections_data_end = . - 0xffffffff00000000;
    }
}
