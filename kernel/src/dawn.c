#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <graphics.h>
#include <paging.h>
#include <pmm.h>
#include <klib.h>
#include <dawn.h>
#include <mouse.h>
#include <panic.h>
#include <smp.h>
#include <acpi.h>
#include <cpu.h>
#include <jit.h>

/* Memory for each CPU's translations: code, block records, and lookup */
#define JIT_CPU_BYTES (24 * 1024 * 1024)

/* Framebuffer request - sets up graphics globals */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

/* Module request (for Dawn disk image) */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

/* Date at boot request - for RTC initialization */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_date_at_boot_request date_request = {
    .id = LIMINE_DATE_AT_BOOT_REQUEST_ID,
    .revision = 0
};

int dawn_ready = 0;

/* CPU feature flags (written by detect_cpu_features) */
uint64_t has_movbe = 0;
uint64_t has_monitor = 0;
uint64_t has_avx2 = 0;
uint64_t has_avx512 = 0;

/* The redraw runs in interrupts and uses SIMD registers, so it saves those of
 * whatever it interrupts: with XSAVE if it is enabled and its area for what
 * is enabled fits, else with FXSAVE, which only covers the XMM registers the
 * SSE2 redraw uses */
static uint8_t simd_area[4096] __attribute__((aligned(64)));
static int use_xsave = 0;

static void simd_save(void) {
    if (use_xsave)
        asm volatile ("xsave64 %0" : "=m" (simd_area) : "a" (~0U), "d" (~0U));
    else
        asm volatile ("fxsave64 %0" : "=m" (simd_area));
}

static void simd_restore(void) {
    if (use_xsave)
        asm volatile ("xrstor64 %0" :: "m" (simd_area), "a" (~0U), "d" (~0U));
    else
        asm volatile ("fxrstor64 %0" :: "m" (simd_area));
}

/* Assembly functions for SIMD screen redraw */
extern void screen_redraw_sse2(volatile uint32_t *dst, uint32_t *src,
                               int width, int height, int dst_pitch, int src_pitch);
extern void screen_redraw_avx2(volatile uint32_t *dst, uint32_t *src,
                                int width, int height, int dst_pitch, int src_pitch);
extern void screen_redraw_avx512(volatile uint32_t *dst, uint32_t *src,
                                  int width, int height, int dst_pitch, int src_pitch);

/* Big-endian memory access */
uint64_t dawn_readram(uint64_t addr) {
    uint64_t val = *(volatile uint64_t *)addr;
    return __builtin_bswap64(val);
}

void dawn_writeram(uint64_t addr, uint64_t value) {
    *(volatile uint64_t *)addr = __builtin_bswap64(value);
}

/* I/O stack for deferred writes */
typedef struct {
    uint64_t io_loc;
    uint64_t value;
} io_stack_t;

#define IO_STACK_MAX 4096

static io_stack_t io_stack[IO_STACK_MAX];
static size_t io_stack_head = 0;
static size_t io_stack_tail = 0;

void dawn_io_write(uint64_t io_loc, uint64_t value) {
    size_t next_tail = (io_stack_tail + 1) % IO_STACK_MAX;
    if (next_tail == io_stack_head)
        panic("io_stack overflow", 0);

    io_stack[io_stack_tail].io_loc = io_loc;
    io_stack[io_stack_tail].value = value;
    io_stack_tail = next_tail;
}

void dawn_io_flush(void) {
    while (io_stack_head != io_stack_tail) {
        if (!dawn_readram(io_stack[io_stack_head].io_loc)) {
            dawn_writeram(io_stack[io_stack_head].io_loc, io_stack[io_stack_head].value);
            io_stack_head = (io_stack_head + 1) % IO_STACK_MAX;
        } else {
            break;
        }
    }
}

static volatile uint64_t last_frame_counter = 0;
static uint32_t *dawn_framebuffer;

/* Check if a Limine memory map type should be mapped to HHDM */
static int should_map_to_hhdm(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE:
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        case LIMINE_MEMMAP_FRAMEBUFFER:
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        case LIMINE_MEMMAP_ACPI_NVS:
        case LIMINE_MEMMAP_RESERVED_MAPPED:
            return 1;
        default:
            return 0;
    }
}

#define IA32_APIC_BASE_MSR 0x1B

/* Build the dawn_pagemap with all required mappings */
static void dawn_build_pagemap(uintptr_t ramdisk_loc) {
    kprint(KPRN_INFO, "dawn: Building page tables...");

    /* Allocate the pagemap */
    dawn_pagemap = kmalloc(1);
    if (!dawn_pagemap)
        for (;;);
    dawn_pagemap = (pt_entry_t *)((size_t)dawn_pagemap + PHYS_MEM_OFFSET);
    kprint(KPRN_INFO, "dawn: pagemap at %X", (size_t)dawn_pagemap);

    /* 1. Map the kernel executable by parsing ELF PHDRs */
    kprint(KPRN_INFO, "dawn: Mapping kernel from ELF PHDRs...");

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)kernel_file->address;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)((uint8_t *)kernel_file->address + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = &phdrs[i];

        if (phdr->p_type != PT_LOAD)
            continue;

        /* Calculate physical address from virtual address offset */
        size_t virt_base = phdr->p_vaddr;
        size_t phys_base = kernel_phys_base + (virt_base - kernel_virt_base);
        size_t memsz = phdr->p_memsz;

        /* Align to page boundaries */
        size_t page_offset = virt_base & (PAGE_SIZE_4K - 1);
        size_t aligned_virt = virt_base & ~(PAGE_SIZE_4K - 1);
        size_t aligned_phys = phys_base & ~(PAGE_SIZE_4K - 1);
        size_t aligned_size = (memsz + page_offset + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);

        kprint(KPRN_INFO, "dawn:   PHDR %u: virt %X phys %X size %X",
               i, aligned_virt, aligned_phys, aligned_size);

        for (size_t offset = 0; offset < aligned_size; offset += PAGE_SIZE_4K) {
            map_page(dawn_pagemap, aligned_phys + offset, aligned_virt + offset);
        }
    }

    /* 2. Map HHDM regions (use 2MB pages where possible) */
    kprint(KPRN_INFO, "dawn: Mapping HHDM regions...");
    for (uint64_t i = 0; i < limine_memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = limine_memmap->entries[i];

        if (!should_map_to_hhdm(entry->type))
            continue;

        size_t aligned_base = entry->base & ~(PAGE_SIZE_4K - 1);
        size_t aligned_end = (entry->base + entry->length + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);

        for (size_t addr = aligned_base; addr < aligned_end; ) {
            if ((addr & (PAGE_SIZE_2M - 1)) == 0 && (aligned_end - addr) >= PAGE_SIZE_2M) {
                map_page_2m(dawn_pagemap, addr, addr + hhdm_offset);
                addr += PAGE_SIZE_2M;
            } else {
                map_page(dawn_pagemap, addr, addr + hhdm_offset);
                addr += PAGE_SIZE_4K;
            }
        }
    }

    /* 3. Map APIC MMIO regions */
    kprint(KPRN_INFO, "dawn: Mapping APIC MMIO...");
    uint64_t lapic_base = rdmsr(IA32_APIC_BASE_MSR) & 0xFFFFFFFFFFFFF000ULL;
    kprint(KPRN_INFO, "dawn: Local APIC at %X", lapic_base);
    map_page(dawn_pagemap, lapic_base, lapic_base + hhdm_offset);

    for (size_t i = 0; i < io_apic_ptr; i++) {
        uint32_t ioapic_addr = io_apics[i]->addr;
        kprint(KPRN_INFO, "dawn: I/O APIC #%U at %X", i, ioapic_addr);
        map_page(dawn_pagemap, ioapic_addr, ioapic_addr + hhdm_offset);
    }

    /* 4. Map Dawn image using 4KB pages (module is only 4KB aligned per Limine spec) */
    kprint(KPRN_INFO, "dawn: Mapping Dawn image...");
    for (size_t i = 0; i < DAWN_IMAGE_SIZE / PAGE_SIZE; i++) {
        map_page(dawn_pagemap, ramdisk_loc + i * PAGE_SIZE, DAWN_IMAGE_BASE + i * PAGE_SIZE);
    }

    /* 5. Set up the translator with part of what is left, sized for all of
     * it going to Dawn, which gets the rest */
    uint64_t dawn_max = DAWN_IMAGE_SIZE + pmm_free_pages() * PAGE_SIZE;
    int ncpus = smp_cpu_total();
    size_t jit_max = 2 * (dawn_max >> 6) + (size_t)ncpus * JIT_CPU_BYTES;
    if (jit_max > (dawn_max - DAWN_IMAGE_SIZE) / 8)
        jit_max = (dawn_max - DAWN_IMAGE_SIZE) / 8;
    size_t jit_bytes = jit_init(dawn_max, ncpus, jit_max, has_movbe);
    if (jit_bytes)
        kprint(KPRN_INFO, "dawn: Translator for %u CPUs took %U MiB.", (uint32_t)ncpus, jit_bytes >> 20);
    else
        kprint(KPRN_WARN, "dawn: No memory for the translator, interpreting.");

    /* 6. Allocate additional memory for Dawn */
    size_t pg;
    uint64_t *lastptrs[4] = {0};

    for (pg = 0; ; pg++) {
        uint64_t *ptr = kmalloc(1);
        if (!ptr)
            break;
        size_t virt = DAWN_IMAGE_SIZE + pg * PAGE_SIZE;
        int map_ret = map_page(dawn_pagemap, (size_t)ptr, virt);
        if (map_ret)
            break;
        memmove(&lastptrs[1], &lastptrs[0], 3 * sizeof(uint64_t *));
        lastptrs[0] = (void *)((size_t)ptr + PHYS_MEM_OFFSET);
    }

    /* Fill last 4 pages with 0xff - Dawn expects this at end of memory */
    for (size_t i = 0; i < 4; i++) {
        if (lastptrs[i] == NULL) {
            break;
        }
        for (size_t j = 0; j < PAGE_SIZE / sizeof(uint64_t); j++) {
            lastptrs[i][j] = 0xffffffffffffffff;
        }
    }

    kprint(KPRN_INFO, "dawn: Acquired %U MiB total.",
           (DAWN_IMAGE_SIZE + pg * PAGE_SIZE) / (1024 * 1024));
    if (jit_bytes)
        jit_set_mem_size(DAWN_IMAGE_SIZE + pg * PAGE_SIZE);

    asm volatile (
        "movq %0, %%cr3"
        :
        : "r" ((size_t)dawn_pagemap - PHYS_MEM_OFFSET)
        : "memory"
    );
}

void dawn_redraw_screen(void) {
    if (last_frame_counter != dawn_readram(DAWN_FRAME_COUNTER)) {
        last_frame_counter = dawn_readram(DAWN_FRAME_COUNTER);
        volatile uint32_t *tmp = antibuffer0;
        antibuffer0 = antibuffer1;
        antibuffer1 = tmp;

        int pitch_pixels = vbe_pitch / sizeof(uint32_t);

        simd_save();
        if (use_xsave && has_avx512) {
            /* AVX-512 fast path - processes 16 pixels at a time with VPSHUFB */
            screen_redraw_avx512(antibuffer0, dawn_framebuffer,
                                 vbe_width, vbe_height, pitch_pixels, vbe_width);
        } else if (use_xsave && has_avx2) {
            /* AVX2 fast path - processes 8 pixels at a time with VPSHUFB */
            screen_redraw_avx2(antibuffer0, dawn_framebuffer,
                               vbe_width, vbe_height, pitch_pixels, vbe_width);
        } else {
            /* SSE2, which x86-64 always has - 4 pixels at a time */
            screen_redraw_sse2(antibuffer0, dawn_framebuffer,
                               vbe_width, vbe_height, pitch_pixels, vbe_width);
        }
        simd_restore();
        swap_vbufs();
        put_mouse_cursor(1);
    }
}

static void get_cpu_name(char *str) {
    asm volatile (
        "movl $0x80000002, %%eax;"
        "cpuid;"
        "stosl;"
        "movl %%ebx, %%eax;"
        "stosl;"
        "movl %%ecx, %%eax;"
        "stosl;"
        "movl %%edx, %%eax;"
        "stosl;"
        "movl $0x80000003, %%eax;"
        "cpuid;"
        "stosl;"
        "movl %%ebx, %%eax;"
        "stosl;"
        "movl %%ecx, %%eax;"
        "stosl;"
        "movl %%edx, %%eax;"
        "stosl;"
        "movl $0x80000004, %%eax;"
        "cpuid;"
        "stosl;"
        "movl %%ebx, %%eax;"
        "stosl;"
        : "+D" (str)
        :
        : "rax", "rbx", "rcx", "rdx", "memory"
    );
}

static void detect_cpu_features(void) {
    uint32_t ecx, ebx;

    /* CPUID.01H for MOVBE and AVX */
    asm volatile (
        "movl $1, %%eax;"
        "cpuid;"
        : "=c" (ecx)
        :
        : "eax", "ebx", "edx"
    );
    /* MOVBE is bit 22 of ECX, MONITOR (with MWAIT) bit 3 */
    has_movbe = (ecx >> 22) & 1;
    has_monitor = (ecx >> 3) & 1;

    /* Check if AVX is supported (bit 28) AND OSXSAVE is enabled (bit 27) */
    /* OSXSAVE (bit 27) indicates the OS has enabled XSAVE - required for AVX */
    if (!((ecx >> 27) & 1) || !((ecx >> 28) & 1)) {
        has_avx2 = 0;
        return;
    }

    /* Verify AVX is actually enabled in XCR0 (bit 2) */
    uint32_t xcr0_lo, xcr0_hi;
    asm volatile (
        "xorl %%ecx, %%ecx;"
        "xgetbv;"
        : "=a" (xcr0_lo), "=d" (xcr0_hi)
        :
        : "ecx"
    );
    if (!((xcr0_lo >> 2) & 1)) {
        has_avx2 = 0;
        return;
    }

    /* The size of the XSAVE area for the state enabled in XCR0 */
    uint32_t a = 0xd, b, c = 0, d;
    asm volatile ("cpuid" : "+a" (a), "=b" (b), "+c" (c), "=d" (d));
    use_xsave = b <= sizeof(simd_area);

    /* CPUID.07H for AVX2 and AVX-512 */
    asm volatile (
        "movl $7, %%eax;"
        "xorl %%ecx, %%ecx;"
        "cpuid;"
        : "=b" (ebx)
        :
        : "eax", "ecx", "edx"
    );
    /* AVX2 is bit 5 of EBX */
    has_avx2 = (ebx >> 5) & 1;

    /* The AVX-512 redraw needs AVX-512F (bit 16 of EBX), AVX-512BW (bit 30) for
     * VPSHUFB on ZMM registers and AVX-512VL (bit 31) for EVEX encoded YMM ones */
    if (!((ebx >> 16) & 1) || !((ebx >> 30) & 1) || !((ebx >> 31) & 1)) {
        has_avx512 = 0;
        return;
    }

    /* Check XCR0 bits 5,6,7 for AVX-512 state (opmask, ZMM_Hi256, Hi16_ZMM) */
    if ((xcr0_lo & 0xE0) != 0xE0) {
        has_avx512 = 0;
        return;
    }

    has_avx512 = 1;
}

void dawn_init(void) {
    /* Validate framebuffer request */
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        panic("Dawn: Framebuffer not provided by bootloader", 0);
    }

    /* Set up framebuffer globals */
    struct limine_framebuffer *lfb = framebuffer_request.response->framebuffers[0];
    framebuffer = (volatile uint32_t *)lfb->address;
    vbe_width = lfb->width;
    vbe_height = lfb->height;
    vbe_pitch = lfb->pitch;
    modeset_done = 1;

    /* Validate module request */
    if (module_request.response == NULL ||
        module_request.response->module_count < 1) {
        panic("Dawn: Module (disk image) not provided by bootloader", 0);
    }

    /* Get Dawn disk module */
    struct limine_file *dawn_module = module_request.response->modules[0];
    uintptr_t ramdisk_loc = (uintptr_t)dawn_module->address - hhdm_offset;

    /* Allocate antibuffers */
    antibuffer0 = kalloc((vbe_pitch / sizeof(uint32_t)) * vbe_height * sizeof(uint32_t));
    antibuffer1 = kalloc((vbe_pitch / sizeof(uint32_t)) * vbe_height * sizeof(uint32_t));

    detect_cpu_features();
    dawn_build_pagemap(ramdisk_loc);

    asm volatile (
        "movq %0, %%cr3"
        :
        : "r" ((size_t)dawn_pagemap - PHYS_MEM_OFFSET)
        : "memory"
    );

    dawn_ready = 1;
    dawn_framebuffer = (uint32_t *)DAWN_IMAGE_SIZE;

    /* CPU name */
    get_cpu_name((char *)DAWN_CPU_NAME);

    /* Display info */
    dawn_writeram(DAWN_DISPLAY_WIDTH, (uint64_t)vbe_width);
    dawn_writeram(DAWN_DISPLAY_HEIGHT, (uint64_t)vbe_height);
    dawn_writeram(DAWN_DISPLAY_BPP, (uint64_t)32);
    dawn_writeram(DAWN_DISPLAY_FLAGS, (uint64_t)2);

    /* Set RTC from boot time */
    uint64_t dawn_epoch = 0;
    if (date_request.response != NULL) {
        int64_t unix_time = date_request.response->timestamp;
        /* Convert to Dawn epoch (seconds since Jan 1, 2016 = Unix 1451606400) */
        dawn_epoch = (unix_time > 1451606400) ? (unix_time - 1451606400) : 0;
    }
    kprint(KPRN_INFO, "Setting date...");
    dawn_writeram(DAWN_RTC, dawn_epoch * DAWN_FIXED_ONE);
    kprint(KPRN_INFO, "Date set");
}
