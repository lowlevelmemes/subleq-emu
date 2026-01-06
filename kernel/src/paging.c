#include <stdint.h>
#include <stddef.h>
#include <paging.h>
#include <pmm.h>

/* Dawn PML4 page table - dynamically allocated, used for entire runtime */
pt_entry_t *dawn_pagemap;

/* Map a 4KB page: creates full 4-level page table structure */
int map_page(pt_entry_t *pml4, size_t phys_addr, size_t virt_addr) {
    /* Calculate the indices in the various tables using the virtual address */
    size_t pml4_idx = (virt_addr >> 39) & 0x1ff;
    size_t pdpt_idx = (virt_addr >> 30) & 0x1ff;
    size_t pd_idx = (virt_addr >> 21) & 0x1ff;
    size_t pt_idx = (virt_addr >> 12) & 0x1ff;

    pt_entry_t *pdpt, *pd, *pt;

    /* Get or allocate PDPT */
    if (pml4[pml4_idx] & 0x1) {
        pdpt = (pt_entry_t *)((pml4[pml4_idx] & 0xfffffffffffff000) + PHYS_MEM_OFFSET);
    } else {
        pdpt = kmalloc(1);
        if (!pdpt)
            return -1;
        pml4[pml4_idx] = (pt_entry_t)pdpt | 0x03;
        pdpt = (pt_entry_t *)((size_t)pdpt + PHYS_MEM_OFFSET);
    }

    /* Check if it's a 1GB huge page - can't map 4KB inside it */
    if ((pdpt[pdpt_idx] & 0x1) && (pdpt[pdpt_idx] & (1 << 7)))
        return -1;

    /* Get or allocate PD */
    if (pdpt[pdpt_idx] & 0x1) {
        pd = (pt_entry_t *)((pdpt[pdpt_idx] & 0xfffffffffffff000) + PHYS_MEM_OFFSET);
    } else {
        pd = kmalloc(1);
        if (!pd)
            return -1;
        pdpt[pdpt_idx] = (pt_entry_t)pd | 0x03;
        pd = (pt_entry_t *)((size_t)pd + PHYS_MEM_OFFSET);
    }

    /* Check if PD entry is a 2MB huge page - can't map 4KB inside it */
    if ((pd[pd_idx] & 0x1) && (pd[pd_idx] & (1 << 7)))
        return -1;

    /* Get or allocate PT */
    if (pd[pd_idx] & 0x1) {
        pt = (pt_entry_t *)((pd[pd_idx] & 0xfffffffffffff000) + PHYS_MEM_OFFSET);
    } else {
        pt = kmalloc(1);
        if (!pt)
            return -1;
        pd[pd_idx] = (pt_entry_t)pt | 0x03;
        pt = (pt_entry_t *)((size_t)pt + PHYS_MEM_OFFSET);
    }

    /* Set the 4KB page entry */
    pt[pt_idx] = (pt_entry_t)(phys_addr & 0xfffffffffffff000) | 0x03;

    return 0;
}

/* Map a 2MB huge page */
int map_page_2m(pt_entry_t *pml4, size_t phys_addr, size_t virt_addr) {
    /* Calculate the indices */
    size_t pml4_entry = (virt_addr >> 39) & 0x1ff;
    size_t pdpt_entry = (virt_addr >> 30) & 0x1ff;
    size_t pd_entry = (virt_addr >> 21) & 0x1ff;

    pt_entry_t *pdpt, *pd;

    /* Get or allocate PDPT */
    if (pml4[pml4_entry] & 0x1) {
        pdpt = (pt_entry_t *)((pml4[pml4_entry] & 0xfffffffffffff000) + PHYS_MEM_OFFSET);
    } else {
        pdpt = kmalloc(1);
        if (!pdpt)
            return -1;
        pml4[pml4_entry] = (pt_entry_t)pdpt | 0x03;
        pdpt = (pt_entry_t *)((size_t)pdpt + PHYS_MEM_OFFSET);
    }

    /* Get or allocate PD */
    if (pdpt[pdpt_entry] & 0x1) {
        pd = (pt_entry_t *)((pdpt[pdpt_entry] & 0xfffffffffffff000) + PHYS_MEM_OFFSET);
    } else {
        pd = kmalloc(1);
        if (!pd)
            return -1;
        pdpt[pdpt_entry] = (pt_entry_t)pd | 0x03;
        pd = (pt_entry_t *)((size_t)pd + PHYS_MEM_OFFSET);
    }

    /* Set the 2MB huge page entry (bit 7 = PS bit for huge page) */
    pd[pd_entry] = (pt_entry_t)(phys_addr & 0xffffffffffe00000) | 0x03 | (1 << 7);
    return 0;
}
