OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0xffffffff00000000 + 0x100000;

    initramfs = 0x200000;
    initramfs_end = (0x200000 + 256*1024*1024);

    .text ALIGN(4K) :
    {
        sections_text = . - 0xffffffff00000000;
        KEEP(*(.multiboot))
        KEEP(*(.startup))
        KEEP(*(.text))
        sections_text_end = . - 0xffffffff00000000;
    }

    .data ALIGN(4K) :
    {
        sections_data = . - 0xffffffff00000000;
        KEEP(*(.data))
        KEEP(*(.rodata))
        sections_data_end = . - 0xffffffff00000000;
    }

    .bss ALIGN(4K) :
    {
        KEEP(*(.bss))
        KEEP(*(COMMON))
        sections_bss_end = . - 0xffffffff00000000;
    }
}
