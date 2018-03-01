#include <cio.h>
#include <stdint.h>
#include <klib.h>

typedef struct {
    uint8_t flags;
    uint8_t x_mov;
    uint8_t y_mov;
    uint8_t res;
} mouse_packet_t;

extern uint64_t _readram(uint64_t);
extern void _writeram(uint64_t, uint64_t);

void poll_mouse(void) {
    uint8_t b;
    mouse_packet_t packet;

    int64_t x_mov;
    int64_t y_mov;

    if (((b = port_in_b(0x64)) & 1) && (b & (1 << 5))) {
        /* packet comes from the mouse */
        /* read packet */
        packet.flags = port_in_b(0x60);
        packet.x_mov = port_in_b(0x60);
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

        _writeram(335542176 + 3 * 8, x_mov * 0x800000);
        _writeram(335542176 + 4 * 8, (-y_mov) * 0x800000);

    }

    return;

}

inline void mouse_wait(uint8_t a_type) {
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

inline void mouse_write(uint8_t a_write) {
    //Wait to be able to send a command
    mouse_wait(1);
    //Tell the mouse we are sending a command
    port_out_b(0x64, 0xD4);
    //Wait for the final part
    mouse_wait(1);
    //Finally write
    port_out_b(0x60, a_write);
}

uint8_t mouse_read(void) {
    //Get's response from mouse
    mouse_wait(0);
    return port_in_b(0x60);
}

void mouse_install(void) {
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
