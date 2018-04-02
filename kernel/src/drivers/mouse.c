#include <stdint.h>
#include <cio.h>
#include <klib.h>
#include <graphics.h>
#include <mouse.h>
#include <subleq.h>

typedef struct {
    uint8_t flags;
    uint8_t x_mov;
    uint8_t y_mov;
    uint8_t res;
} mouse_packet_t;

static int mouse_x = 0;
static int mouse_y = 0;
static int old_mouse_x = 0;
static int old_mouse_y = 0;

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

void put_mouse_cursor(void) {
    for (size_t x = 0; x < 16; x++) {
        for (size_t y = 0; y < 16; y++) {
            plot_px(old_mouse_x + x, old_mouse_y + y, 
                get_old_px(old_mouse_x + x, old_mouse_y + y), 0);
        }
    }
    for (size_t x = 0; x < 16; x++) {
        for (size_t y = 0; y < 16; y++) {
            if (cursor.bitmap[x * 16 + y] != -1)
                plot_px(mouse_x + x, mouse_y + y, cursor.bitmap[x * 16 + y], 0);
        }
    }
    return;
}

void poll_mouse(void) {
    uint8_t b;
    mouse_packet_t packet;

    int64_t x_mov;
    int64_t y_mov;

    if (((b = port_in_b(0x64)) & 1) && (b & (1 << 5))) {
        /* packet comes from the mouse */
        /* read packet */
        packet.flags = port_in_b(0x60);
        mouse_wait(0);

        if (!(packet.flags & (1 << 3)))
            return;

        packet.x_mov = port_in_b(0x60);
        mouse_wait(0);
        packet.y_mov = port_in_b(0x60);

        if (packet.flags & (1 << 0))
            _writeram(335542176 + 0 * 8, 0x100000000);
        else
            _writeram(335542176 + 0 * 8, 0);

        if (packet.flags & (1 << 1))
            _writeram(335542176 + 1 * 8, 0x100000000);
        else
            _writeram(335542176 + 1 * 8, 0);

        if (packet.flags & (1 << 2))
            _writeram(335542176 + 2 * 8, 0x100000000);
        else
            _writeram(335542176 + 2 * 8, 0);

        if (packet.flags & (1 << 4))
            x_mov = (int8_t)packet.x_mov;
        else
            x_mov = packet.x_mov;

        if (packet.flags & (1 << 5))
            y_mov = (int8_t)packet.y_mov;
        else
            y_mov = packet.y_mov;

        old_mouse_x = mouse_x;
        old_mouse_y = mouse_y;

        if (!(mouse_x + x_mov < 0) && !(mouse_x + x_mov >= edid_width)) {
            mouse_x += x_mov;
        }

        if (!(mouse_y - y_mov < 0) && !(mouse_y - y_mov >= edid_height)) {
            mouse_y -= y_mov;
        }

        put_mouse_cursor();

        _writeram(335542176 + 6 * 8, scale_position(0, edid_width, 0, 0x100000000, mouse_x));
        _writeram(335542176 + 7 * 8, scale_position(0, edid_height, 0, 0x100000000, mouse_y));

    }

    return;

}

void init_mouse(void) {
    mouse_x = edid_width / 2;
    mouse_y = edid_height / 2;

    //Enable the auxiliary mouse device
    mouse_wait(1);
    port_out_b(0x64, 0xA8);

    //Tell the mouse to use default settings
    mouse_write(0xF6);
    mouse_read();  //Acknowledge

    //Enable the mouse
    mouse_write(0xF4);
    mouse_read();  //Acknowledge
}
