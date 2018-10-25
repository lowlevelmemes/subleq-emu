#ifndef __PAGING_H__
#define __PAGING_H__

#include <stdint.h>
#include <stddef.h>

/* arch specific values */
#define PAGE_SIZE               0x200000
#define PAGE_TABLE_ENTRIES      512

void init_paging(void);

void *kmalloc(size_t);
void kmfree(void *, size_t);

typedef uint64_t pt_entry_t;

int map_page(pt_entry_t *, size_t, size_t);

extern pt_entry_t kernel_pagemap[];
extern pt_entry_t *subleq_pagemap;

#endif
