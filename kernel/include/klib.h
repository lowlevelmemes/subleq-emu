#ifndef __KLIB_H__
#define __KLIB_H__

#include <stdint.h>
#include <stddef.h>
#include <kernel.h>


size_t kmemcpy(char *, const char *, size_t);
size_t kstrcpy(char *, const char *);
int kstrcmp(const char *, const char *);
int kstrncmp(const char *, const char *, size_t);
size_t kstrlen(const char *);
void *kalloc(size_t);
void kfree(void *);
void *krealloc(void *, size_t);
uint64_t power(uint64_t, uint64_t);
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
