#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <graphics.h>
#include <klib.h>
#include <system.h>
#include <subleq.h>
#include <mouse.h>

static volatile uint64_t last_frame_counter = 0;

void _strcpyram(uint64_t dest, const char *mem) {
    for (size_t i = 0; mem[i]; i++)
        _writeram(dest++, (uint64_t)mem[i] << 56);
    _writeram(dest, 0);

    return;
}

void subleq_redraw_screen(void) {
    if (last_frame_counter != _readram(335540096 + 32)) {
        last_frame_counter = _readram(335540096 + 32);
        volatile uint32_t *tmp = antibuffer0;
        antibuffer0 = antibuffer1;
        antibuffer1 = tmp;
        asm volatile (
            "1: "
            "lodsd;"
            "rol eax, 8;"
            "bswap eax;"
            "stosd;"
            "loop 1b;"
            :
            : "S" (initramfs_addr + 256*1024*1024),
              "D" (antibuffer0),
              "c" (edid_width * edid_height)
            : "rax"
        );
        swap_vbufs();
        put_mouse_cursor();
    }

    return;
}

void init_subleq(void) {

    for (size_t i = 0; i < edid_width * edid_height; i++)
        framebuffer[i] = 0;

    /* CPU id */
    _strcpyram(335413288, "subleq-emu x86");

    /* */
    _writeram(334364664, (uint64_t)1);

    /* display */
    _writeram(335540096, (uint64_t)edid_width);
    _writeram(335540096 + 8, (uint64_t)edid_height);
    _writeram(335540096 + 16, (uint64_t)32);
    _writeram(335540096 + 24, (uint64_t)2);

    return;
}
