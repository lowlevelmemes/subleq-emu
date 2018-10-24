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

io_stack_t io_stack[IO_STACK_MAX];
int io_stack_ptr = 0;

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

    if (io_stack_ptr) {
        if (!_readram(io_stack[0].io_loc) || io_stack[0].tries == 10) {
            _writeram(io_stack[0].io_loc, io_stack[0].value);
            for (size_t j = 1; j < io_stack_ptr; j++) {
                io_stack[j - 1] = io_stack[j];
            }
            io_stack_ptr--;
        } else {
            io_stack[0].tries++;
        }
    }

    return;
}

static volatile uint64_t last_frame_counter = 0;

extern uint8_t initramfs[];

uint32_t *dawn_framebuffer;

void _strcpyram(uint64_t dest, const char *mem) {
    for (size_t i = 0; mem[i]; i++)
        initramfs[dest++] = mem[i];

    initramfs[dest] = 0;

    return;
}

void subleq_acquire_mem(void) {
    size_t i;
    uint64_t *lastptr = 0;

    for (i = 0; ; i++) {
        uint64_t *ptr = kmalloc(1);
        if (!ptr)
            break;
        lastptr = ptr;
        map_page((pt_entry_t *)subleq_pagemap, (size_t)ptr, (size_t)0x1a000000 - (size_t)initramfs + i * PAGE_SIZE);
        for (size_t j = 0; j < PAGE_SIZE / sizeof(uint64_t); j++)
            ptr[j] = 0;
    }

    /* Add 8 KiB of gibberish because Dawn is great */
    for (size_t i = 0; i < 8192 / sizeof(uint64_t); i++) {
        size_t base = (PAGE_SIZE - 8192) / sizeof(uint64_t);
        lastptr[base + i] = 0xffffffffffffffff;
    }

    kprint(KPRN_INFO, "subleq: Acquired %U 2 MiB pages.", i);

    return;
}

void subleq_redraw_screen(void) {
    if (last_frame_counter != _readram(335540096 + 32)) {
        last_frame_counter = _readram(335540096 + 32);
        volatile uint32_t *tmp = antibuffer0;
        antibuffer0 = antibuffer1;
        antibuffer1 = tmp;
        for (size_t x = 0; x < vbe_width; x++) {
            for (size_t y = 0; y < vbe_height; y++) {
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
        put_mouse_cursor();
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
        :
        : "D" (str)
        : "rax", "rbx", "rcx", "rdx"
    );

    return;
}

void init_subleq(void) {
    zero_subleq_memory();

    dawn_framebuffer = (uint32_t *)&initramfs[256*1024*1024];

    dawn_framebuffer = (uint32_t *)((size_t)dawn_framebuffer + KERNEL_PHYS_OFFSET);

    /* CPU name */
    get_cpu_name((char *)(&initramfs[335413288]));

    /* display */
    _writeram(335540096, (uint64_t)vbe_width);
    _writeram(335540096 + 8, (uint64_t)vbe_height);
    _writeram(335540096 + 16, (uint64_t)32);
    _writeram(335540096 + 24, (uint64_t)2);

    subleq_acquire_mem();

    init_smp();

    subleq_ready = 1;

    return;
}
