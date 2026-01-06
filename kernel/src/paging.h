#ifndef __PAGING_H__
#define __PAGING_H__

#include <stdint.h>
#include <stddef.h>

#define PAGE_TABLE_ENTRIES      512

typedef uint64_t pt_entry_t;

/* Map a 4KB page */
int map_page(pt_entry_t *pml4, size_t phys, size_t virt);

/* Map a 2MB huge page */
int map_page_2m(pt_entry_t *pml4, size_t phys, size_t virt);

/* Dawn PML4 page table (the only pagemap, used for entire runtime) */
extern pt_entry_t *dawn_pagemap;

#endif
