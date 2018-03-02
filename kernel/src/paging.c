#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <paging.h>
#include <system.h>
#include <klib.h>

#define BITMAP_MAX (memory_size / PAGE_SIZE)
#define BITMAP_FULL ((0x100000000 / PAGE_SIZE) / 32)
#define BITMAP_BASE (MEMORY_BASE / PAGE_SIZE)

static uint32_t mem_bitmap[BITMAP_FULL] = {0};

void init_paging(void) {
    full_identity_map();

    for (size_t i = 0; i < BITMAP_FULL; i++)
        mem_bitmap[i] = 0;

    return;
}

static int rd_bitmap(size_t i) {
    size_t entry = i / 32;
    size_t offset = i % 32;

    return (int)((mem_bitmap[entry] >> offset) & 1);
}

static void wr_bitmap(size_t i, int val) {
    size_t entry = i / 32;
    size_t offset = i % 32;

    if (val)
        mem_bitmap[entry] |= (1 << offset);
    else
        mem_bitmap[entry] &= ~(1 << offset);

    return;
}

void *kmalloc(size_t pages) {
    /* allocate memory pages using a bitmap to track free and used pages */

    /* find contiguous free pages */
    size_t pg_counter = 0;
    size_t i;
    size_t strt_page;
    for (i = BITMAP_BASE; i < BITMAP_MAX; i++) {
        if (!rd_bitmap(i))
            pg_counter++;
        else
            pg_counter = 0;
        if (pg_counter == pages)
            goto found;
    }

    return (void *)0;

found:
    strt_page = i - (pages - 1);

    for (i = strt_page; i < (strt_page + pages); i++)
        wr_bitmap(i, 1);

    return (void *)(strt_page * PAGE_SIZE);

}

void kmfree(void *ptr, size_t pages) {

    size_t strt_page = (size_t)ptr / PAGE_SIZE;

    for (size_t i = strt_page; i < (strt_page + pages); i++)
        wr_bitmap(i, 0);

    return;

}
