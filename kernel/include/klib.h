#ifndef __KLIB_H__
#define __KLIB_H__

#include <stdint.h>
#include <stddef.h>
#include <kernel.h>

static inline size_t kmemcpy(char *dest, const char *source, size_t count) {
    size_t i;

    for (i = 0; i < count; i++)
        dest[i] = source[i];

    return i;
}

static inline size_t memcpy(char *dest, const char *source, size_t count) {
    return kmemcpy(dest, source, count);
}

static inline size_t kstrcpy(char *dest, const char *source) {
    size_t i;

    for (i = 0; source[i]; i++)
        dest[i] = source[i];

    dest[i] = 0;

    return i;
}

static inline int kstrcmp(const char *dest, const char *source) {
    for (size_t i = 0; dest[i] == source[i]; i++)
        if ((!dest[i]) && (!source[i])) return 0;

    return 1;
}

static inline int kstrncmp(const char *dest, const char *source, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (dest[i] != source[i]) return 1;

    return 0;
}

static inline size_t kstrlen(const char *str) {
    size_t len;
    for (len = 0; str[len]; len++);

    return len;
}

void *kalloc(size_t);
void kfree(void *);
void *krealloc(void *, size_t);
void kputs(const char *);
void knputs(const char *, size_t);
void kprn_ui(uint64_t);
void kprn_x(uint64_t);

typedef enum {
    KPRN_INFO,
    KPRN_WARN,
    KPRN_ERR,
    KPRN_DBG,
} kprn_type_t;
void kprint(kprn_type_t type, const char *fmt, ...);



#endif
