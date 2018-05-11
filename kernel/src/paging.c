#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <paging.h>
#include <system.h>
#include <klib.h>
#include <e820.h>

#define BITMAP_MAX (memory_size / PAGE_SIZE)
#define MBITMAP_FULL ((0x100000000 / PAGE_SIZE) / 32)
#define BITMAP_BASE (MEMORY_BASE / PAGE_SIZE)

static volatile size_t bitmap_full = MBITMAP_FULL;
static volatile uint32_t *mem_bitmap;
static volatile uint32_t initial_bitmap[MBITMAP_FULL];
static volatile uint32_t *tmp_bitmap;

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

static void bm_realloc(void) {
    if (!(tmp_bitmap = kalloc((bitmap_full + 2048) * sizeof(uint32_t)))) {
        kprint(KPRN_ERR, "kalloc failure in bm_realloc(). Halted.");
        for (;;);
    }

    kmemcpy((void *)tmp_bitmap, (void *)mem_bitmap, bitmap_full * sizeof(uint32_t));
    for (size_t i = bitmap_full; i < bitmap_full + 2048; i++) {
        tmp_bitmap[i] = 0xffffffff;
    }
    
    bitmap_full += 2048;

    volatile uint32_t *tmp = tmp_bitmap;
    tmp_bitmap = mem_bitmap;
    mem_bitmap = tmp;

    kfree((void *)tmp_bitmap);

    return;
}

/* Populate bitmap using e820 data. */
void init_pmm(void) {
    for (size_t i = 0; i < bitmap_full; i++) {
        initial_bitmap[i] = 0;
    }

    mem_bitmap = initial_bitmap;
    if (!(tmp_bitmap = kalloc(bitmap_full * sizeof(uint32_t)))) {
        kprint(KPRN_ERR, "kalloc failure in init_pmm(). Halted.");
        for (;;);
    }

    for (size_t i = 0; i < bitmap_full; i++)
        tmp_bitmap[i] = initial_bitmap[i];
    mem_bitmap = tmp_bitmap;

    /* For each region specified by the e820, iterate over each page which
       fits in that region and if the region type indicates the area itself
       is usable, write that page as free in the bitmap. Otherwise, mark the page as used. */
    for (size_t i = 0; e820_map[i].type; i++) {
        for (size_t j = 0; j * PAGE_SIZE < e820_map[i].length; j++) {
            size_t addr = e820_map[i].base + j * PAGE_SIZE;

            if (addr < KERNEL_TOP + 0x1000000)
                continue;

            size_t page = addr / PAGE_SIZE;

            while (page >= bitmap_full * 32)
                bm_realloc();
            if (e820_map[i].type == 1)
                wr_bitmap(page, 0);
            else
                wr_bitmap(page, 1);
        }
    }

    return;
}

void init_paging(void) {
    init_pmm();

    return;
}
