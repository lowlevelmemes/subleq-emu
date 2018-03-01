#ifndef __PAGING_H__
#define __PAGING_H__

#include <stdint.h>
#include <stddef.h>

/* location of the kernel's page directory */
#define KERNEL_PAGE             kernel_pagemap
#define KERNEL_BASE             0x100000
#define KERNEL_TOP              0x70000000
#define MEMORY_BASE             0x70000000

/* arch specific values */
#define PAGE_SIZE               4096


void init_paging(void);

void full_identity_map(void);
void *kmalloc(size_t);
void kmfree(void *, size_t);


#endif
