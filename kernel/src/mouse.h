#ifndef __MOUSE_H__
#define __MOUSE_H__

void put_mouse_cursor(int);
void mouse_update(void);
void mouse_handler(void);
void mouse_init(void);

extern int hw_mouse_enabled;

#endif
