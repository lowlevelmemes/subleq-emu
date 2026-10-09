#ifndef __KLIB_H__
#define __KLIB_H__

#include <stdint.h>
#include <stddef.h>
#include <memory.h>

/* Standard C library string functions */
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *restrict dest, const char *restrict src);

/* Kernel memory allocation */
void *kalloc(size_t);
void kfree(void *);
void *krealloc(void *, size_t);

/* Kernel printing */
void kputs(const char *);
void knputs(const char *, size_t);
void kprn_ui(uint64_t);
void kprn_x(uint64_t);
void kprint(int type, const char *fmt, ...);

#define KPRN_INFO   0
#define KPRN_WARN   1
#define KPRN_ERR    2
#define KPRN_DBG    3

#endif
