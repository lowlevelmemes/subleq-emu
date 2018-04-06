OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text ALIGN(4K) :
    {
        sections_text = .;
        *(.multiboot)
        *(.startup)
        *(.text)
        sections_text_end = .;
    }

    .data ALIGN(4K) :
    {
        sections_data = .;
        *(.data)
        *(.rodata)
        *(COMMON)
        *(.bss)
    }

    .initramfs ALIGN(2M) :
    {
        initramfs = .;
        *(.initramfs)
        initramfs_end = .;
        sections_data_end = .;
    }
}
