#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <graphics.h>
#include <cio.h>
#include <panic.h>

vbe_info_struct_t vbe_info_struct;
vbe_mode_info_t vbe_mode_info;
get_vbe_t get_vbe;

int modeset_done = 0;

volatile uint32_t *framebuffer;
volatile uint32_t *antibuffer0;
volatile uint32_t *antibuffer1;

int vbe_width = 1024;
int vbe_height = 768;
int vbe_pitch;

uint16_t vid_modes[1024];

void get_vbe_info(vbe_info_struct_t *vbe_info_struct);
void get_vbe_mode_info(get_vbe_t *get_vbe);
void set_vbe_mode(uint16_t mode);
void dump_vga_font(uint8_t *bitmap);

uint8_t vga_font[4096];

void swap_vbufs(void) {
    volatile uint32_t *_ab0 = antibuffer0;
    volatile uint32_t *_ab1 = antibuffer1;
    volatile uint32_t *_fb  = framebuffer;
    size_t _c = (vbe_pitch / sizeof(uint32_t)) * vbe_height;
    asm volatile (
        "1: "
        "lodsd;"
        "cmp eax, dword ptr ds:[rbx];"
        "jne 2f;"
        "add rdi, 4;"
        "jmp 3f;"
        "2: "
        "stosd;"
        "3: "
        "add rbx, 4;"
        "dec rcx;"
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

    return;
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

    return;
}

void init_graphics(void) {
    /* interrupts are supposed to be OFF */

    kprint(KPRN_INFO, "Dumping VGA font...");

    dump_vga_font(vga_font);

    kprint(KPRN_INFO, "Initialising VBE...");

    get_vbe_info(&vbe_info_struct);
    /* copy the video mode array somewhere else because it might get overwritten */
    for (size_t i = 0; ; i++) {
        vid_modes[i] = ((uint16_t *)(size_t)vbe_info_struct.vid_modes)[i];
        if (((uint16_t *)(size_t)vbe_info_struct.vid_modes)[i+1] == 0xffff) {
            vid_modes[i+1] = 0xffff;
            break;
        }
    }

    kprint(KPRN_INFO, "Version: %u.%u", vbe_info_struct.version_maj, vbe_info_struct.version_min);
    kprint(KPRN_INFO, "OEM: %s", (char *)(size_t)vbe_info_struct.oem);
    kprint(KPRN_INFO, "Graphics vendor: %s", (char *)(size_t)vbe_info_struct.vendor);
    kprint(KPRN_INFO, "Product name: %s", (char *)(size_t)vbe_info_struct.prod_name);
    kprint(KPRN_INFO, "Product revision: %s", (char *)(size_t)vbe_info_struct.prod_rev);

    /* try to set the mode */
retry:
    get_vbe.vbe_mode_info = (uint32_t)(((size_t)&vbe_mode_info) - KERNEL_PHYS_OFFSET);
    for (size_t i = 0; vid_modes[i] != 0xffff; i++) {
        get_vbe.mode = vid_modes[i];
        get_vbe_mode_info(&get_vbe);
        if (vbe_mode_info.res_x == vbe_width
            && vbe_mode_info.res_y == vbe_height
            && vbe_mode_info.bpp == 32) {
            /* mode found */
            kprint(KPRN_INFO, "VBE found matching mode %x, attempting to set.", get_vbe.mode);
            vbe_pitch = (int)vbe_mode_info.pitch;
            framebuffer = (uint32_t *)((size_t)vbe_mode_info.framebuffer + PHYS_MEM_OFFSET);
            antibuffer0 = kalloc((vbe_pitch / sizeof(uint32_t)) * vbe_height * sizeof(uint32_t));
            antibuffer1 = kalloc((vbe_pitch / sizeof(uint32_t)) * vbe_height * sizeof(uint32_t));
            kprint(KPRN_INFO, "Framebuffer address: %X", framebuffer);
            set_vbe_mode(get_vbe.mode);
            goto success;
        }
    }

    if (vbe_width == 1024 && vbe_height == 768) {
        vbe_width = 800;
        vbe_height = 600;
        goto retry;
    }

    if (vbe_width == 800 && vbe_height == 600) {
        vbe_width = 640;
        vbe_height = 480;
        goto retry;
    }

    if (vbe_width == 640 && vbe_height == 480) {
        panic("VBE: can't set video mode.", 0);
    }

    vbe_width = 1024;
    vbe_height = 768;
    goto retry;

success:
    modeset_done = 1;
    kprint(KPRN_INFO, "VBE init done.");
    return;
}
