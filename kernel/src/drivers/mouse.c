#include <stdint.h>
#include <cio.h>
#include <klib.h>
#include <graphics.h>
#include <mouse.h>
#include <subleq.h>
#include <panic.h>

int hw_mouse_enabled = 0;

typedef struct {
    uint8_t flags;
    uint8_t x_mov;
    uint8_t y_mov;
} mouse_packet_t;

static int mouse_x = 0;
static int mouse_y = 0;

typedef struct {
    int64_t bitmap[16 * 16];
} cursor_t;

#define X 0x00ffffff
#define B 0x00000000
#define o (-1)

cursor_t cursor = {
    {
        X, X, X, X, X, X, X, X, X, X, X, X, o, o, o, o,
        X, B, B, B, B, B, B, B, B, B, X, o, o, o, o, o,
        X, B, B, B, B, B, B, B, B, X, o, o, o, o, o, o,
        X, B, B, B, B, B, B, B, X, o, o, o, o, o, o, o,
        X, B, B, B, B, B, B, X, o, o, o, o, o, o, o, o,
        X, B, B, B, B, B, B, B, X, o, o, o, o, o, o, o,
        X, B, B, B, B, B, B, B, B, X, o, o, o, o, o, o,
        X, B, B, B, X, B, B, B, B, B, X, o, o, o, o, o,
        X, B, B, X, o, X, B, B, B, B, B, X, o, o, o, o,
        X, B, X, o, o, o, X, B, B, B, B, B, X, o, o, o,
        X, X, o, o, o, o, o, X, B, B, B, B, B, X, o, o,
        X, o, o, o, o, o, o, o, X, B, B, B, B, B, X, o,
        o, o, o, o, o, o, o, o, o, X, B, B, B, B, B, X,
        o, o, o, o, o, o, o, o, o, o, X, B, B, B, X, o,
        o, o, o, o, o, o, o, o, o, o, o, X, B, X, o, o,
        o, o, o, o, o, o, o, o, o, o, o, o, X, o, o, o,
    }
};

#undef X
#undef B
#undef o

static inline void mouse_wait(uint8_t a_type) {
    uint32_t _time_out=100000; //unsigned int
    if (a_type==0) {
        while (_time_out--) {
            if ((port_in_b(0x64) & 1)==1) {
                return;
            }
        }
        return;
    } else {
        while (_time_out--) {
            if ((port_in_b(0x64) & 2)==0) {
                return;
            }
        }
        return;
    }
}

static inline void mouse_write(uint8_t a_write) {
    //Wait to be able to send a command
    mouse_wait(1);
    //Tell the mouse we are sending a command
    port_out_b(0x64, 0xD4);
    //Wait for the final part
    mouse_wait(1);
    //Finally write
    port_out_b(0x60, a_write);
}

static inline uint8_t mouse_read(void) {
    //Get's response from mouse
    mouse_wait(0);
    return port_in_b(0x60);
}

static inline uint64_t scale_position(uint64_t min, uint64_t max,
                                      uint64_t oldmin, uint64_t oldmax,
                                      uint64_t val) {
    return (oldmax - oldmin) * (val - min) / (max - min) + oldmin;
}

static int last_plotted_mouse_x = 0;
static int last_plotted_mouse_y = 0;

void put_mouse_cursor(int refresh) {
    if (!hw_mouse_enabled) {
        for (size_t x = 0; x < 16; x++) {
            for (size_t y = 0; y < 16; y++) {
                plot_px(last_plotted_mouse_x + x, last_plotted_mouse_y + y,
                    get_ab0_px(last_plotted_mouse_x + x, last_plotted_mouse_y + y));
            }
        }
        return;
    }

    if (!refresh) {
        for (size_t x = 0; x < 16; x++) {
            for (size_t y = 0; y < 16; y++) {
                plot_px(last_plotted_mouse_x + x, last_plotted_mouse_y + y,
                    get_ab0_px(last_plotted_mouse_x + x, last_plotted_mouse_y + y));
            }
        }
    }

    for (size_t x = 0; x < 16; x++) {
        for (size_t y = 0; y < 16; y++) {
            if (cursor.bitmap[x * 16 + y] != -1)
                plot_px(mouse_x + x, mouse_y + y, cursor.bitmap[x * 16 + y]);
        }
    }

    last_plotted_mouse_x = mouse_x;
    last_plotted_mouse_y = mouse_y;

    return;
}

static uint64_t dawn_mouse_x = 0;
static uint64_t dawn_mouse_y = 0;
static uint64_t dawn_mouse_click_l = 0;
static uint64_t dawn_mouse_click_r = 0;
static uint64_t dawn_mouse_click_m = 0;

typedef struct {
    uint64_t mouse_x;
    uint64_t mouse_y;
    uint64_t mouse_click_l;
    uint64_t mouse_click_r;
    uint64_t mouse_click_m;
} dmouse_packet_t;

#define MAX_PACKETS 1024

dmouse_packet_t packets[MAX_PACKETS];
size_t packet_i = 0;

static dmouse_packet_t process_packet(mouse_packet_t *p) {
    dmouse_packet_t dp;

    int64_t x_mov;
    int64_t y_mov;

    if (p->flags & (1 << 0)) {
        dp.mouse_click_l = 0x100000000;
    } else {
        dp.mouse_click_l = 0;
    }

    if (p->flags & (1 << 1)) {
        dp.mouse_click_r = 0x100000000;
    } else {
        dp.mouse_click_r = 0;
    }

    if (p->flags & (1 << 2)) {
        dp.mouse_click_m = 0x100000000;
    } else {
        dp.mouse_click_m = 0;
    }

    if (p->flags & (1 << 4))
        x_mov = (int8_t)p->x_mov;
    else
        x_mov = p->x_mov;

    if (p->flags & (1 << 5))
        y_mov = (int8_t)p->y_mov;
    else
        y_mov = p->y_mov;

    if (mouse_x + x_mov < 0) {
        mouse_x = 0;
    } else if (mouse_x + x_mov >= vbe_width) {
        mouse_x = vbe_width - 1;
    } else {
        mouse_x += x_mov;
    }

    if (mouse_y - y_mov < 0) {
        mouse_y = 0;
    } else if (mouse_y - y_mov >= vbe_height) {
        mouse_y = vbe_height - 1;
    } else {
        mouse_y -= y_mov;
    }

    dp.mouse_x = scale_position(0, vbe_width, 0, 0x100000000, mouse_x);
    dp.mouse_y = scale_position(0, vbe_height, 0, 0x100000000, mouse_y);

    return dp;
}

void mouse_update(void) {
    /*
    335542176
            +0  =   left click
            +1  =   right click
            +2  =   middle click
            +3  =   relative x
            +4  =   relative y
            +5  =   reserved
            +6  =   absolute x
            +7  =   absolute y
            +8  =   touch
            +9  =   scroll wheel
    */

    if (!hw_mouse_enabled)
        return;

    /* wait for Dawn to clear mouse registers 3 through 9 */
    if (
        _readram(335542176 + 3 * 8) ||
        _readram(335542176 + 4 * 8) ||
        _readram(335542176 + 6 * 8) ||
        _readram(335542176 + 7 * 8) ||
        _readram(335542176 + 8 * 8) ||
        _readram(335542176 + 9 * 8)
    ) return;

    if (packet_i) {
        dawn_mouse_click_l = packets[0].mouse_click_l;
        dawn_mouse_click_r = packets[0].mouse_click_r;
        dawn_mouse_click_m = packets[0].mouse_click_m;
        dawn_mouse_x = packets[0].mouse_x;
        dawn_mouse_y = packets[0].mouse_y;
        for (size_t i = 1; i < packet_i; i++) {
            packets[i - 1] = packets[i];
        }
        packet_i--;
    }

    _writeram(335542176 + 0 * 8, dawn_mouse_click_l);
    _writeram(335542176 + 1 * 8, dawn_mouse_click_r);
    _writeram(335542176 + 2 * 8, dawn_mouse_click_m);
    _writeram(335542176 + 6 * 8, dawn_mouse_x);
    _writeram(335542176 + 7 * 8, dawn_mouse_y);

    return;
}

static int is_location_info(mouse_packet_t *p) {
    if (packet_i) {
        if (p->flags & (1 << 0)) {
            if (!packets[packet_i - 1].mouse_click_l)
                return 0;
        } else {
            if (packets[packet_i - 1].mouse_click_l)
                return 0;
        }

        if (p->flags & (1 << 1)) {
            if (!packets[packet_i - 1].mouse_click_r)
                return 0;
        } else {
            if (packets[packet_i - 1].mouse_click_r)
                return 0;
        }

        if (p->flags & (1 << 2)) {
            if (!packets[packet_i - 1].mouse_click_m)
                return 0;
        } else {
            if (packets[packet_i - 1].mouse_click_m)
                return 0;
        }
    } else {
        if (p->flags & (1 << 0)) {
            if (!dawn_mouse_click_l)
                return 0;
        } else {
            if (dawn_mouse_click_l)
                return 0;
        }

        if (p->flags & (1 << 1)) {
            if (!dawn_mouse_click_r)
                return 0;
        } else {
            if (dawn_mouse_click_r)
                return 0;
        }

        if (p->flags & (1 << 2)) {
            if (!dawn_mouse_click_m)
                return 0;
        } else {
            if (dawn_mouse_click_m)
                return 0;
        }
    }

    return 1;
}

static int handler_cycle = 0;
static mouse_packet_t current_packet;
static int discard_packet = 0;

void mouse_handler(void) {
    switch (handler_cycle) {
        case 0:
            current_packet.flags = mouse_read();
            handler_cycle++;
            if (current_packet.flags & (1 << 6) || current_packet.flags & (1 << 7))
                discard_packet = 1;     // discard rest of packet
            if (!(current_packet.flags & (1 << 3)))
                discard_packet = 1;     // discard rest of packet
            break;
        case 1:
            current_packet.x_mov = mouse_read();
            handler_cycle++;
            break;
        case 2:
            current_packet.y_mov = mouse_read();
            handler_cycle = 0;

            if (discard_packet) {
                discard_packet = 0;
                break;
            }

            hw_mouse_enabled = 1;

            dmouse_packet_t dp = process_packet(&current_packet);
            put_mouse_cursor(0);

            if (is_location_info(&current_packet)) {
                dawn_mouse_click_l = dp.mouse_click_l;
                dawn_mouse_click_r = dp.mouse_click_r;
                dawn_mouse_click_m = dp.mouse_click_m;
                dawn_mouse_x = dp.mouse_x;
                dawn_mouse_y = dp.mouse_y;
            } else {
                if (packet_i == MAX_PACKETS)
                    panic("mouse packets overflow", 0);
                packets[packet_i++] = dp;
            }
    }

    return;
}

void init_mouse(void) {
    mouse_x = vbe_width / 2;
    mouse_y = vbe_height / 2;

    dawn_mouse_x = scale_position(0, vbe_width, 0, 0x100000000, mouse_x);
    dawn_mouse_y = scale_position(0, vbe_height, 0, 0x100000000, mouse_y);

    mouse_wait(1);
    port_out_b(0x64, 0xA8);

    mouse_wait(1);
    port_out_b(0x64, 0x20);
    uint8_t status = mouse_read();
    mouse_read();
    status |= (1 << 1);
    status &= ~(1 << 5);
    mouse_wait(1);
    port_out_b(0x64, 0x60);
    mouse_wait(1);
    port_out_b(0x60, status);
    mouse_read();

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();
}
