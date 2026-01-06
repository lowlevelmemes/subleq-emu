#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include <stdint.h>

void swap_vbufs(void);
void plot_px(int x, int y, uint32_t hex);
uint32_t get_ab0_px(int x, int y);
void plot_ab0_px(int x, int y, uint32_t hex);
extern volatile uint32_t *framebuffer;
extern volatile uint32_t *antibuffer0;
extern volatile uint32_t *antibuffer1;
extern int vbe_width;
extern int vbe_height;
extern int vbe_pitch;

extern int modeset_done;


#endif
