#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <pmm.h>
#include <klib.h>
#include <panic.h>

/* Memory map request */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

/* Higher Half Direct Map request */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

/* Executable address request - tells us kernel physical/virtual base */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

/* Executable file request - gives us the kernel ELF file for PHDR parsing */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_file_request executable_file_request = {
    .id = LIMINE_EXECUTABLE_FILE_REQUEST_ID,
    .revision = 0
};

/* Limine globals - exported via pmm.h */
uint64_t hhdm_offset = 0;
uint64_t kernel_phys_base = 0;
uint64_t kernel_virt_base = 0;
struct limine_memmap_response *limine_memmap = NULL;
struct limine_file *kernel_file = NULL;

#define MEMORY_BASE 0x1000000
#define BITMAP_BASE (MEMORY_BASE / PAGE_SIZE)

static volatile uint32_t *mem_bitmap;
static volatile uint32_t initial_bitmap[] = { 0xfffffffe };
static volatile uint32_t *tmp_bitmap;
static size_t bitmap_pages = 1;  /* Number of pages in the bitmap */
static size_t max_page = BITMAP_BASE + 32768;  /* Maximum page number tracked */
static size_t last_alloc_page = BITMAP_BASE;  /* Hint for next allocation */

static inline int rd_bitmap(size_t i) {
    i -= BITMAP_BASE;

    size_t entry = i / 32;
    size_t offset = i % 32;

    return (int)((mem_bitmap[entry] >> offset) & 1);
}

static inline void wr_bitmap(size_t i, int val) {
    i -= BITMAP_BASE;

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

    /* find contiguous free pages - start from hint for O(n) sequential allocs */
    size_t pg_counter = 0;
    size_t i;
    size_t strt_page;
    size_t start = last_alloc_page;

    /* First pass: from hint to end */
    for (i = start; i < max_page; i++) {
        if (!rd_bitmap(i))
            pg_counter++;
        else
            pg_counter = 0;
        if (pg_counter == pages)
            goto found;
    }

    /* Second pass: from beginning to hint (wrap around) */
    pg_counter = 0;
    for (i = BITMAP_BASE; i < start; i++) {
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
    last_alloc_page = i + 1;  /* Update hint for next allocation */

    for (i = strt_page; i < (strt_page + pages); i++)
        wr_bitmap(i, 1);

    /* zero out the pages */
    uint64_t *phys_pages = (uint64_t *)((strt_page * PAGE_SIZE) + PHYS_MEM_OFFSET);

    for (size_t j = 0; j < (pages * PAGE_SIZE) / sizeof(uint64_t); j++)
        phys_pages[j] = 0;

    return (void *)(strt_page * PAGE_SIZE);
}

void kmfree(void *ptr, size_t pages) {
    size_t strt_page = (size_t)ptr / PAGE_SIZE;

    for (size_t i = strt_page; i < (strt_page + pages); i++)
        wr_bitmap(i, 0);

    return;
}

/* Initialize PMM - validates Limine requests and populates bitmap */
void pmm_init(void) {
    /* Validate and extract Limine responses */
    if (hhdm_request.response == NULL) {
        panic("PMM: HHDM not provided by bootloader", 0);
    }
    hhdm_offset = hhdm_request.response->offset;

    if (executable_address_request.response == NULL) {
        panic("PMM: Executable address not provided by bootloader", 0);
    }
    kernel_phys_base = executable_address_request.response->physical_base;
    kernel_virt_base = executable_address_request.response->virtual_base;

    if (executable_file_request.response == NULL ||
        executable_file_request.response->executable_file == NULL) {
        panic("PMM: Executable file not provided by bootloader", 0);
    }
    kernel_file = executable_file_request.response->executable_file;

    if (memmap_request.response == NULL) {
        panic("PMM: Memory map not provided by bootloader", 0);
    }
    limine_memmap = memmap_request.response;

    /* First pass: find the highest usable address to size the bitmap */
    size_t highest_addr = 0;
    for (uint64_t i = 0; i < limine_memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            size_t end = entry->base + entry->length;
            if (end > highest_addr)
                highest_addr = end;
        }
    }

    /* Calculate bitmap size needed */
    size_t new_max_page = highest_addr / PAGE_SIZE;
    size_t bitmap_bits = new_max_page - BITMAP_BASE;
    size_t bitmap_bytes = (bitmap_bits + 7) / 8;
    bitmap_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    kprint(KPRN_INFO, "pmm: Highest address: %X, max_page: %U, bitmap_pages: %U",
           highest_addr, new_max_page, bitmap_pages);

    /* Bootstrap: use initial_bitmap with limited max_page to allocate the real bitmap */
    mem_bitmap = initial_bitmap;
    /* Keep max_page small for bootstrap allocation */
    if (!(tmp_bitmap = kmalloc(bitmap_pages))) {
        kprint(KPRN_ERR, "kalloc failure in pmm_init(). Halted.");
        for (;;);
    }

    /* Now we can use the full range */
    max_page = new_max_page;

    tmp_bitmap = (uint32_t *)((size_t)tmp_bitmap + PHYS_MEM_OFFSET);

    /* Initialize bitmap to all 1s (all pages used) */
    for (size_t i = 0; i < (bitmap_pages * PAGE_SIZE) / sizeof(uint32_t); i++)
        tmp_bitmap[i] = 0xffffffff;

    mem_bitmap = tmp_bitmap;

    kprint(KPRN_INFO, "pmm: Mapping memory from Limine memory map...");

    /* Second pass: mark usable regions as free in the bitmap */
    for (uint64_t i = 0; i < limine_memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_memmap->entries[i];

        size_t aligned_base;
        if (entry->base % PAGE_SIZE)
            aligned_base = entry->base + (PAGE_SIZE - (entry->base % PAGE_SIZE));
        else
            aligned_base = entry->base;
        size_t aligned_length = (entry->length / PAGE_SIZE) * PAGE_SIZE;
        if ((entry->base % PAGE_SIZE) && aligned_length) aligned_length -= PAGE_SIZE;

        /* Check if this region is usable - only USABLE, not BOOTLOADER_RECLAIMABLE
         * (bootloader reclaimable contains Limine's page tables we may still need) */
        int is_usable = (entry->type == LIMINE_MEMMAP_USABLE);

        if (!is_usable)
            continue;  /* Already marked as used */

        for (size_t j = 0; j * PAGE_SIZE < aligned_length; j++) {
            size_t addr = aligned_base + j * PAGE_SIZE;
            size_t page = addr / PAGE_SIZE;

            if (page < BITMAP_BASE)
                continue;

            if (page >= max_page)
                continue;

            /* skip bitmap pages */
            size_t bitmap_phys = (size_t)tmp_bitmap - PHYS_MEM_OFFSET;
            if (addr >= bitmap_phys && addr < bitmap_phys + (bitmap_pages * PAGE_SIZE))
                continue;

            wr_bitmap(page, 0);  /* Mark as free */
        }
    }

    return;
}
