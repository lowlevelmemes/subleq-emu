#ifndef __PAGING_H__
#define __PAGING_H__

#include <stdint.h>
#include <stddef.h>

/* location of the kernel's page directory */
#define KERNEL_PAGE             kernel_pagemap
#define KERNEL_BASE             0x100000
#define KERNEL_TOP              0x1a000000
#define MEMORY_BASE             KERNEL_TOP

/* arch specific values */
#define PAGE_SIZE               0x200000
#define PAGE_TABLE_ENTRIES      512


void init_paging(void);

void full_identity_map(void);
void *kmalloc(size_t);
void kmfree(void *, size_t);

typedef uint64_t pt_entry_t;

void map_page(pt_entry_t *, size_t, size_t);

extern void *kernel_pagemap[];
extern void *subleq_pagemap[];


#endif
