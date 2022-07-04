#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include <stdint.h>

typedef struct {
    uint8_t version_min;
    uint8_t version_maj;
    uint32_t oem;   // is a 32 bit pointer to char
    uint32_t capabilities;
    uint32_t vid_modes;     // is a 32 bit pointer to uint16_t
    uint16_t vid_mem_blocks;
    uint16_t software_rev;
    uint32_t vendor;   // is a 32 bit pointer to char
    uint32_t prod_name;   // is a 32 bit pointer to char
    uint32_t prod_rev;   // is a 32 bit pointer to char
} __attribute__((packed)) vbe_info_struct_t;

typedef struct {
    uint8_t pad0[16];
    uint16_t pitch;
    uint16_t res_x;
    uint16_t res_y;
    uint8_t pad1[3];
    uint8_t bpp;
    uint8_t pad2[14];
    uint32_t framebuffer;
    uint8_t pad3[212];
} __attribute__((packed)) vbe_mode_info_t;

typedef struct {
    uint32_t vbe_mode_info;      // is a 32 bit pointer to vbe_mode_info_t
    uint16_t mode;
} get_vbe_t;

extern volatile uint32_t *framebuffer;
extern volatile uint32_t *antibuffer0;
extern volatile uint32_t *antibuffer1;
extern uint8_t vga_font[4096];
extern int vbe_width;
extern int vbe_height;
extern int vbe_pitch;

extern int modeset_done;

void init_graphics(void);

void swap_vbufs(void);
void plot_px(int x, int y, uint32_t hex);

static inline uint32_t get_ab0_px(int x, int y) {
    if (x >= vbe_width || y >= vbe_height)
        return 0;

    size_t fb_i = x + (vbe_pitch / sizeof(uint32_t)) * y;

    return antibuffer0[fb_i];
}

static inline void plot_ab0_px(int x, int y, uint32_t hex) {
    if (x >= vbe_width || y >= vbe_height)
        return;

    size_t fb_i = x + (vbe_pitch / sizeof(uint32_t)) * y;

    antibuffer0[fb_i] = hex;

    return;
}


#endif
