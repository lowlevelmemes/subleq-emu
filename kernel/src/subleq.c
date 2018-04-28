#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <graphics.h>
#include <klib.h>
#include <system.h>
#include <subleq.h>
#include <mouse.h>

static volatile uint64_t last_frame_counter = 0;

extern uint8_t initramfs[];

uint32_t *dawn_framebuffer;

void _strcpyram(uint64_t dest, const char *mem) {
    for (size_t i = 0; mem[i]; i++)
        initramfs[dest++] = mem[i];

    initramfs[dest] = 0;

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

void init_subleq(void) {
    dawn_framebuffer = (uint32_t *)&initramfs[256*1024*1024];

    /* CPU id */
    _strcpyram(335413288, "subleq-emu x86");

    /* display */
    _writeram(335540096, (uint64_t)vbe_width);
    _writeram(335540096 + 8, (uint64_t)vbe_height);
    _writeram(335540096 + 16, (uint64_t)32);
    _writeram(335540096 + 24, (uint64_t)2);

    return;
}
