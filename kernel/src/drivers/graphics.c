#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <graphics.h>
#include <cio.h>
#include <panic.h>

vbe_info_struct_t vbe_info_struct;
edid_info_struct_t edid_info_struct;
vbe_mode_info_t vbe_mode_info;
get_vbe_t get_vbe;

int modeset_done = 0;

volatile uint32_t *framebuffer;
volatile uint32_t *antibuffer0;
volatile uint32_t *antibuffer1;

int edid_width = 0;
int edid_height = 0;

uint16_t vid_modes[1024];

void get_vbe_info(vbe_info_struct_t *vbe_info_struct);
void get_edid_info(edid_info_struct_t *edid_info_struct);
void get_vbe_mode_info(get_vbe_t *get_vbe);
void set_vbe_mode(uint16_t mode);
void dump_vga_font(uint8_t *bitmap);

uint8_t vga_font[4096];

void swap_vbufs(void) {
    asm volatile (
        "1: "
        "lodsd;"
        "cmp eax, dword ptr ds:[rbx];"
        "jne 2f;"
        "inc rdi;"
        "inc rdi;"
        "inc rdi;"
        "inc rdi;"
        "jmp 3f;"
        "2: "
        "stosd;"
        "3: "
        "inc rbx;"
        "inc rbx;"
        "inc rbx;"
        "inc rbx;"
        "loop 1b;"
        :
        : "S" (antibuffer0),
          "D" (framebuffer),
          "b" (antibuffer1),
          "c" (edid_width * edid_height)
        : "rax"
    );

    return;
}

void plot_px(int x, int y, uint32_t hex) {
    size_t fb_i = x + edid_width * y;

    framebuffer[fb_i] = hex;

    return;
}

uint32_t get_old_px(int x, int y) {
    size_t fb_i = x + edid_width * y;

    return antibuffer0[fb_i];
}

void init_graphics(void) {
    /* interrupts are supposed to be OFF */

    kprint(KPRN_INFO, "Dumping VGA font...");

    dump_vga_font(vga_font);

    kprint(KPRN_INFO, "Initialising VBE...");

    get_vbe_info(&vbe_info_struct);
    /* copy the video mode array somewhere else because it might get overwritten */
    for (size_t i = 0; ; i++) {
        vid_modes[i] = ((uint16_t *)vbe_info_struct.vid_modes)[i];
        if (((uint16_t *)vbe_info_struct.vid_modes)[i+1] == 0xffff) {
            vid_modes[i+1] = 0xffff;
            break;
        }
    }

    kprint(KPRN_INFO, "Version: %u.%u", vbe_info_struct.version_maj, vbe_info_struct.version_min);
    kprint(KPRN_INFO, "OEM: %s", (char *)vbe_info_struct.oem);
    kprint(KPRN_INFO, "Graphics vendor: %s", (char *)vbe_info_struct.vendor);
    kprint(KPRN_INFO, "Product name: %s", (char *)vbe_info_struct.prod_name);
    kprint(KPRN_INFO, "Product revision: %s", (char *)vbe_info_struct.prod_rev);

    edid_width = 1024;
    edid_height = 768;

    /* try to set the mode */
    get_vbe.vbe_mode_info = (uint32_t)&vbe_mode_info;
    for (size_t i = 0; vid_modes[i] != 0xffff; i++) {
        get_vbe.mode = vid_modes[i];
        get_vbe_mode_info(&get_vbe);
        if (vbe_mode_info.res_x == edid_width
            && vbe_mode_info.res_y == edid_height
            && vbe_mode_info.bpp == 32) {
            /* mode found */
            kprint(KPRN_INFO, "VBE found matching mode %x, attempting to set.", get_vbe.mode);
            framebuffer = (uint32_t *)vbe_mode_info.framebuffer;
            antibuffer0 = kalloc(edid_width * edid_height * sizeof(uint32_t));
            antibuffer1 = kalloc(edid_width * edid_height * sizeof(uint32_t));
            kprint(KPRN_INFO, "Framebuffer address: %x", vbe_mode_info.framebuffer);
            set_vbe_mode(get_vbe.mode);
            goto success;
        }
    }

    panic("VBE: can't set video mode.", 0);

success:
    modeset_done = 1;
    kprint(KPRN_INFO, "VBE init done.");
    return;
}
