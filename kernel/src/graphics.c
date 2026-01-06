/*
 * Graphics driver for subleq-emu
 * With Limine, framebuffer is set up by the bootloader
 * and passed to us via the Limine protocol.
 */

#include <stdint.h>
#include <stddef.h>
#include <graphics.h>

/* Framebuffer state - set by main.c from Limine response */
int modeset_done = 0;

volatile uint32_t *framebuffer;
volatile uint32_t *antibuffer0;
volatile uint32_t *antibuffer1;

int vbe_width = 1024;
int vbe_height = 768;
int vbe_pitch;

/* Optimized buffer swap - only copy changed pixels */
void swap_vbufs(void) {
    volatile uint32_t *_ab0 = antibuffer0;
    volatile uint32_t *_ab1 = antibuffer1;
    volatile uint32_t *_fb  = framebuffer;
    size_t _c = (vbe_pitch / sizeof(uint32_t)) * vbe_height;
    asm volatile (
        "1: "
        "lodsl;"
        "cmpl (%%rbx), %%eax;"
        "jne 2f;"
        "addq $4, %%rdi;"
        "jmp 3f;"
        "2: "
        "stosl;"
        "3: "
        "addq $4, %%rbx;"
        "decq %%rcx;"
        "jnz 1b;"
        : "+S" (_ab0),
          "+D" (_fb),
          "+b" (_ab1),
          "+c" (_c)
        :
        : "rax", "memory"
    );
}

void plot_px(int x, int y, uint32_t hex) {
    if (x >= vbe_width || y >= vbe_height)
        return;

    size_t fb_i = x + (vbe_pitch / sizeof(uint32_t)) * y;

    framebuffer[fb_i] = hex;
}

uint32_t get_ab0_px(int x, int y) {
    if (x >= vbe_width || y >= vbe_height)
        return 0;

    size_t fb_i = x + (vbe_pitch / sizeof(uint32_t)) * y;

    return antibuffer0[fb_i];
}

void plot_ab0_px(int x, int y, uint32_t hex) {
    if (x >= vbe_width || y >= vbe_height)
        return;

    size_t fb_i = x + (vbe_pitch / sizeof(uint32_t)) * y;

    antibuffer0[fb_i] = hex;
}
