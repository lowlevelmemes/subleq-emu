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

typedef struct {
    uint64_t io_loc;
    uint64_t value;
    int tries;
} io_stack_t;

#define IO_STACK_MAX 4096

static io_stack_t io_stack[IO_STACK_MAX];
static size_t io_stack_ptr = 0;

void subleq_io_write(uint64_t io_loc, uint64_t value) {
    if (io_stack_ptr == IO_STACK_MAX)
        panic("io_stack overflow", 0);

    io_stack[io_stack_ptr].io_loc = io_loc;
    io_stack[io_stack_ptr].value = value;
    io_stack[io_stack_ptr].tries = 0;
    io_stack_ptr++;

    return;
}

void subleq_io_flush(void) {

    while (io_stack_ptr) {
        if (!_readram(io_stack[0].io_loc)) {
            _writeram(io_stack[0].io_loc, io_stack[0].value);
            for (size_t j = 1; j < io_stack_ptr; j++) {
                io_stack[j - 1] = io_stack[j];
            }
            io_stack_ptr--;
        } else {
            break;
        }
    }

    return;
}

static volatile uint64_t last_frame_counter = 0;

extern uint8_t initramfs[];

static uint32_t *dawn_framebuffer;

pt_entry_t *subleq_pagemap;

static void subleq_acquire_mem(void) {
    subleq_pagemap = kmalloc(1);
    if (!subleq_pagemap)
        for (;;);

    subleq_pagemap = (pt_entry_t *)((size_t)subleq_pagemap + PHYS_MEM_OFFSET);

    /* Map in Dawn */
    for (size_t i = 0; i < (256*1024*1024) / PAGE_SIZE; i++) {
        map_page(subleq_pagemap, (size_t)initramfs + i * PAGE_SIZE, i * PAGE_SIZE);
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
        for (int x = 0; x < vbe_width; x++) {
            for (int y = 0; y < vbe_height; y++) {
                size_t fb_i = x + vbe_width * y;
                uint32_t val = dawn_framebuffer[fb_i];
                asm volatile (
                    "rol eax, 8;"
                    "bswap eax;"
                    : "=a" (val)
                    : "a" (val)
                );
                plot_ab0_px(x, y, val);
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
        : "rax", "rbx", "rcx", "rdx"
    );

    return;
}

void init_subleq(void) {
    init_cpu0();

    asm volatile ("sti");

    subleq_acquire_mem();

    init_smp();

    asm volatile (
        "mov cr3, rax;"
        :
        : "a" ((size_t)subleq_pagemap - PHYS_MEM_OFFSET)
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
