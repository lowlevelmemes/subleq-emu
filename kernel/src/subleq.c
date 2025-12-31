#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <graphics.h>
#include <paging.h>
#include <klib.h>
#include <system.h>
#include <subleq.h>
#include <mouse.h>
#include <panic.h>
#include <smp.h>

int subleq_ready = 0;
extern uint64_t has_movbe;
extern uint64_t has_avx2;
extern uint64_t has_avx512;

/* Assembly functions for SIMD screen redraw */
extern void screen_redraw_avx2(volatile uint32_t *dst, uint32_t *src,
                                int width, int height, int dst_pitch, int src_pitch);
extern void screen_redraw_avx512(volatile uint32_t *dst, uint32_t *src,
                                  int width, int height, int dst_pitch, int src_pitch);

typedef struct {
    uint64_t io_loc;
    uint64_t value;
} io_stack_t;

#define IO_STACK_MAX 4096

static io_stack_t io_stack[IO_STACK_MAX];
static size_t io_stack_head = 0;
static size_t io_stack_tail = 0;

static inline size_t io_stack_count(void) {
    return (io_stack_tail - io_stack_head + IO_STACK_MAX) % IO_STACK_MAX;
}

void subleq_io_write(uint64_t io_loc, uint64_t value) {
    size_t next_tail = (io_stack_tail + 1) % IO_STACK_MAX;
    if (next_tail == io_stack_head)
        panic("io_stack overflow", 0);

    io_stack[io_stack_tail].io_loc = io_loc;
    io_stack[io_stack_tail].value = value;
    io_stack_tail = next_tail;

    return;
}

void subleq_io_flush(void) {
    while (io_stack_head != io_stack_tail) {
        if (!_readram(io_stack[io_stack_head].io_loc)) {
            _writeram(io_stack[io_stack_head].io_loc, io_stack[io_stack_head].value);
            io_stack_head = (io_stack_head + 1) % IO_STACK_MAX;
        } else {
            break;
        }
    }

    return;
}

static volatile uint64_t last_frame_counter = 0;

static uint32_t *dawn_framebuffer;

pt_entry_t *subleq_pagemap;

static void subleq_acquire_mem(uintptr_t ramdisk_loc) {
    subleq_pagemap = kmalloc(1);
    if (!subleq_pagemap)
        for (;;);

    subleq_pagemap = (pt_entry_t *)((size_t)subleq_pagemap + PHYS_MEM_OFFSET);

    /* Map in Dawn */
    for (size_t i = 0; i < (256*1024*1024) / PAGE_SIZE; i++) {
        map_page(subleq_pagemap, ramdisk_loc + i * PAGE_SIZE, i * PAGE_SIZE);
    }

    size_t pg;
    uint64_t *lastptr = 0;

    for (pg = 0; ; pg++) {
        uint64_t *ptr = kmalloc(1);
        if (!ptr)
            break;
        if (map_page(subleq_pagemap, (size_t)ptr, (size_t)(256*1024*1024) + pg * PAGE_SIZE))
            break;
        lastptr = (pt_entry_t *)((size_t)ptr + PHYS_MEM_OFFSET);
    }

    /* Add 8 KiB of gibberish because Dawn is great */
    for (size_t i = 0; i < 8192 / sizeof(uint64_t); i++) {
        size_t base = (PAGE_SIZE - 8192) / sizeof(uint64_t);
        lastptr[base + i] = 0xffffffffffffffff;
    }

    /* Map in kernel */
    for (size_t i = 256; i < 512; i++) {
        subleq_pagemap[i] = kernel_pagemap[i];
    }

    kprint(KPRN_INFO, "subleq: Acquired %U 2 MiB pages.", pg);

    return;
}

void subleq_redraw_screen(void) {
    if (last_frame_counter != _readram(335540096 + 32)) {
        last_frame_counter = _readram(335540096 + 32);
        volatile uint32_t *tmp = antibuffer0;
        antibuffer0 = antibuffer1;
        antibuffer1 = tmp;

        int pitch_pixels = vbe_pitch / sizeof(uint32_t);

        if (has_avx512) {
            /* AVX-512 fast path - processes 16 pixels at a time with VPSHUFB */
            screen_redraw_avx512(antibuffer0, dawn_framebuffer,
                                 vbe_width, vbe_height, pitch_pixels, vbe_width);
        } else if (has_avx2) {
            /* AVX2 fast path - processes 8 pixels at a time with VPSHUFB */
            screen_redraw_avx2(antibuffer0, dawn_framebuffer,
                               vbe_width, vbe_height, pitch_pixels, vbe_width);
        } else {
            /* Scalar fallback */
            for (int y = 0; y < vbe_height; y++) {
                uint32_t *src = dawn_framebuffer + y * vbe_width;
                volatile uint32_t *dst = antibuffer0 + y * pitch_pixels;

                int x = 0;
                /* Process 4 pixels at a time */
                for (; x + 4 <= vbe_width; x += 4) {
                    uint32_t v0 = src[x], v1 = src[x+1], v2 = src[x+2], v3 = src[x+3];
                    asm volatile (
                        "rol eax, 8; bswap eax;"
                        "rol ebx, 8; bswap ebx;"
                        "rol ecx, 8; bswap ecx;"
                        "rol edx, 8; bswap edx;"
                        : "=a"(v0), "=b"(v1), "=c"(v2), "=d"(v3)
                        : "a"(v0), "b"(v1), "c"(v2), "d"(v3)
                    );
                    dst[x] = v0; dst[x+1] = v1; dst[x+2] = v2; dst[x+3] = v3;
                }
                /* Handle remaining pixels */
                for (; x < vbe_width; x++) {
                    uint32_t val = src[x];
                    asm volatile ("rol eax, 8; bswap eax" : "=a"(val) : "a"(val));
                    dst[x] = val;
                }
            }
        }
        swap_vbufs();
        put_mouse_cursor(1);
    }

    return;
}

static void get_cpu_name(char *str) {
    asm volatile (
        "mov eax, 0x80000002;"
        "cpuid;"
        "stosd;"
        "mov eax, ebx;"
        "stosd;"
        "mov eax, ecx;"
        "stosd;"
        "mov eax, edx;"
        "stosd;"
        "mov eax, 0x80000003;"
        "cpuid;"
        "stosd;"
        "mov eax, ebx;"
        "stosd;"
        "mov eax, ecx;"
        "stosd;"
        "mov eax, edx;"
        "stosd;"
        "mov eax, 0x80000004;"
        "cpuid;"
        "stosd;"
        "mov eax, ebx;"
        "stosd;"
        : "+D" (str)
        :
        : "rax", "rbx", "rcx", "rdx", "memory"
    );

    return;
}

static void detect_cpu_features(void) {
    uint32_t ecx, ebx;

    /* CPUID.01H for MOVBE and AVX */
    asm volatile (
        "mov eax, 1;"
        "cpuid;"
        : "=c" (ecx)
        :
        : "eax", "ebx", "edx"
    );
    /* MOVBE is bit 22 of ECX */
    has_movbe = (ecx >> 22) & 1;

    /* Check if AVX is supported (bit 28) AND OSXSAVE is enabled (bit 27) */
    /* OSXSAVE (bit 27) indicates the OS has enabled XSAVE - required for AVX */
    if (!((ecx >> 27) & 1) || !((ecx >> 28) & 1)) {
        has_avx2 = 0;
        return;
    }

    /* Verify AVX is actually enabled in XCR0 (bit 2) */
    uint32_t xcr0_lo, xcr0_hi;
    asm volatile (
        "xor ecx, ecx;"
        "xgetbv;"
        : "=a" (xcr0_lo), "=d" (xcr0_hi)
        :
        : "ecx"
    );
    if (!((xcr0_lo >> 2) & 1)) {
        has_avx2 = 0;
        return;
    }

    /* CPUID.07H for AVX2 and AVX-512 */
    asm volatile (
        "mov eax, 7;"
        "xor ecx, ecx;"
        "cpuid;"
        : "=b" (ebx)
        :
        : "eax", "ecx", "edx"
    );
    /* AVX2 is bit 5 of EBX */
    has_avx2 = (ebx >> 5) & 1;

    /* AVX-512F is bit 16 of EBX */
    if (!((ebx >> 16) & 1)) {
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

void init_subleq(uintptr_t ramdisk_loc) {
    detect_cpu_features();
    init_cpu0();

    asm volatile ("sti" ::: "memory");

    subleq_acquire_mem(ramdisk_loc);

    init_smp();

    asm volatile (
        "mov cr3, %0"
        :
        : "r" ((size_t)subleq_pagemap - PHYS_MEM_OFFSET)
        : "memory"
    );

    subleq_ready = 1;

    dawn_framebuffer = (uint32_t *)(256*1024*1024);

    /* CPU name */
    get_cpu_name((char *)335413288);

    /* display */
    _writeram(335540096, (uint64_t)vbe_width);
    _writeram(335540096 + 8, (uint64_t)vbe_height);
    _writeram(335540096 + 16, (uint64_t)32);
    _writeram(335540096 + 24, (uint64_t)2);

    subleq_ready = 1;

    return;
}
