#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <klib.h>
#include <pmm.h>
#include <time.h>
#include <cpu.h>

/*
 * Standard C library memory functions
 */

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (size_t i = 0; i < n; i++)
        d[i] = s[i];

    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s) {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    unsigned char v = (unsigned char)c;

    for (size_t i = 0; i < n; i++)
        p[i] = v;

    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i])
            return (int)p1[i] - (int)p2[i];
    }

    return 0;
}

/*
 * Standard C library string functions
 */

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return (int)(unsigned char)s1[i] - (int)(unsigned char)s2[i];
        if (s1[i] == '\0')
            return 0;
    }
    return 0;
}

char *strcpy(char *restrict dest, const char *restrict src) {
    char *ret = dest;
    while ((*dest++ = *src++))
        ;
    return ret;
}

/*
 * Kernel memory allocation
 */

typedef struct {
    size_t pages;
    size_t size;
} kalloc_metadata_t;

void *kalloc(size_t size) {
    size_t pages = size / PAGE_SIZE;
    if (size % PAGE_SIZE) pages++;

    /* allocate the size in pages + an additional page for metadata */
    char *ptr = kmalloc(pages + 1);
    if (!ptr)
        return (void *)0;

    ptr += PHYS_MEM_OFFSET;

    kalloc_metadata_t *metadata = (kalloc_metadata_t *)ptr;
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
        return (void *)0;

    if (metadata->size > new_size)
        memcpy(new_ptr, addr, new_size);
    else
        memcpy(new_ptr, addr, metadata->size);

    kfree(addr);

    return new_ptr;
}

/*
 * Kernel printing
 */

void kputs(const char *string) {
    for (size_t i = 0; string[i]; i++)
        port_out_b(0xe9, string[i]);
}

void knputs(const char *string, size_t len) {
    for (size_t i = 0; i < len; i++)
        port_out_b(0xe9, string[i]);
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

void kprint(int type, const char *fmt, ...) {
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
