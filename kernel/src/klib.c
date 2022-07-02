#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <kernel.h>
#include <klib.h>
#include <paging.h>
#include <system.h>
#include <cio.h>
#include <tty.h>

typedef struct {
    size_t pages;
    size_t size;
} kalloc_metadata_t;

void *kalloc(size_t size) {
    size_t pages = size / PAGE_SIZE;
    if (size % PAGE_SIZE) pages++;

    // allocate the size in page + allocate an additional page for metadata
    char *ptr = kmalloc(pages + 1);
    if (!ptr)
        return (void*)0;

    ptr += PHYS_MEM_OFFSET;

    kalloc_metadata_t* metadata = (kalloc_metadata_t*)ptr;
    ptr += PAGE_SIZE;

    metadata->pages = pages;
    metadata->size = size;

    return (void *)ptr;
}

void kfree(void *addr) {
    kalloc_metadata_t *metadata = (kalloc_metadata_t *)((size_t)addr - PAGE_SIZE);

    kmfree((void *)(metadata - PHYS_MEM_OFFSET), metadata->pages + 1);

    return;
}

void *krealloc(void *addr, size_t new_size) {
    if (!addr) return kalloc(new_size);
    if (!new_size) {
        kfree(addr);
        return (void *)0;
    }

    kalloc_metadata_t *metadata = (kalloc_metadata_t *)((size_t)addr - PAGE_SIZE);

    char *new_ptr;
    if ((new_ptr = kalloc(new_size)) == 0)
        return (void*)0;

    if (metadata->size > new_size)
        kmemcpy(new_ptr, (char *)addr, new_size);
    else
        kmemcpy(new_ptr, (char *)addr, metadata->size);

    kfree(addr);

    return new_ptr;
}

void kputs(const char *string) {

    for (size_t i = 0; string[i]; i++) {
      #ifdef _KERNEL_QEMU_OUTPUT_
        port_out_b(0xe9, string[i]);
      #endif
      #ifdef _KERNEL_VGA_OUTPUT_
        tty_putchar(string[i]);
      #endif
    }

    return;
}

void knputs(const char *string, size_t len) {

    for (size_t i = 0; i < len; i++) {
      #ifdef _KERNEL_QEMU_OUTPUT_
        port_out_b(0xe9, string[i]);
      #endif
      #ifdef _KERNEL_VGA_OUTPUT_
        tty_putchar(string[i]);
      #endif
    }

    return;
}

void kprn_ui(uint64_t x) {
    int i;
    char buf[21] = {0};

    if (!x) {
        kputs("0");
        return;
    }

    for (i = 19; x; i--) {
        buf[i] = (x % 10) + 0x30;
        x = x / 10;
    }

    i++;
    kputs(buf + i);

    return;
}

static const char hex_to_ascii_tab[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

void kprn_x(uint64_t x) {
    int i;
    char buf[17] = {0};

    if (!x) {
        kputs("0x0");
        return;
    }

    for (i = 15; x; i--) {
        buf[i] = hex_to_ascii_tab[(x % 16)];
        x = x / 16;
    }

    i++;
    kputs("0x");
    kputs(buf + i);

    return;
}

void kprint(kprn_type_t type, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    /* print timestamp */
    kputs("["); kprn_ui(uptime_sec); kputs(".");
    kprn_ui(uptime_raw); kputs("] ");

    switch (type) {
        case KPRN_INFO:
            kputs("\e[36minfo\e[37m: ");
            break;
        case KPRN_WARN:
            kputs("\e[33mwarning\e[37m: ");
            break;
        case KPRN_ERR:
            kputs("\e[31mERROR\e[37m: ");
            break;
        case KPRN_DBG:
            kputs("\e[36mDEBUG\e[37m: ");
            break;
        default:
            return;
    }

    char *str;

    for (;;) {
        char c;
        size_t len;
        while (*fmt && *fmt != '%') knputs(fmt++, 1);
        if (!*fmt++) {
            va_end(args);
            kputs("\n");
            return;
        }
        switch (*fmt++) {
            case 's':
                str = (char *)va_arg(args, const char *);
                if (!str)
                    kputs("(null)");
                else
                    kputs(str);
                break;
            case 'k':
                str = (char *)va_arg(args, const char *);
                len = va_arg(args, size_t);
                knputs(str, len);
                break;
            case 'u':
                kprn_ui((uint64_t)va_arg(args, unsigned int));
                break;
            case 'U':
                kprn_ui((uint64_t)va_arg(args, uint64_t));
                break;
            case 'x':
                kprn_x((uint64_t)va_arg(args, unsigned int));
                break;
            case 'X':
                kprn_x((uint64_t)va_arg(args, uint64_t));
                break;
            case 'c':
                c = (char)va_arg(args, int);
                knputs(&c, 1);
                break;
            default:
                kputs("?");
                break;
        }
    }
}
