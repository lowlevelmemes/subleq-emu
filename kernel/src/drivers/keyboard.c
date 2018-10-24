#include <kernel.h>
#include <stdint.h>
#include <cio.h>
#include <klib.h>
#include <subleq.h>

#define MAX_CODE 0x57
#define CAPSLOCK 0x3A
#define RIGHT_SHIFT 0x36
#define LEFT_SHIFT 0x2A
#define RIGHT_SHIFT_REL 0xB6
#define LEFT_SHIFT_REL 0xAA
#define CTRL 0x1D
#define CTRL_REL 0x9D

static int capslock_active = 0;
static int shift_active = 0;
static int ctrl_active = 0;

static const char ascii_capslock[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
};

static const char ascii_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char ascii_shift_capslock[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char ascii_nomod[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

static int extra_scancodes = 0;

void keyboard_handler(uint8_t input_byte) {
    char c;

    if (input_byte == 0xe0) {
        extra_scancodes = 1;
        return;
    }

    if (extra_scancodes) {
        /* extra scancodes */
        extra_scancodes = 0;
        switch (input_byte) {
            case 0x48:
                /* cursor up */
                if (shift_active)
                    subleq_io_write(335542256, 18);
                else
                    subleq_io_write(335542256, 14);
                break;
            case 0x4B:
                /* cursor left */
                if (shift_active)
                    subleq_io_write(335542256, 19);
                else if (ctrl_active)
                    subleq_io_write(335542256, 4);
                else
                    subleq_io_write(335542256, 15);
                break;
            case 0x50:
                /* cursor down */
                if (shift_active)
                    subleq_io_write(335542256, 20);
                else
                    subleq_io_write(335542256, 16);
                break;
            case 0x4D:
                /* cursor right */
                if (shift_active)
                    subleq_io_write(335542256, 21);
                else if (ctrl_active)
                    subleq_io_write(335542256, 7);
                else
                    subleq_io_write(335542256, 17);
                break;
            case 0x49:
                /* pgup */
                subleq_io_write(335542256, 9);
                break;
            case 0x51:
                /* pgdown */
                subleq_io_write(335542256, 10);
                break;
            case 0x53:
                /* delete */
                subleq_io_write(335542256, 29);
                break;
            case CTRL:
            case CTRL_REL:
                ctrl_active = !ctrl_active;
                break;
        }
        return;
    }

    if (input_byte == LEFT_SHIFT || input_byte == RIGHT_SHIFT || input_byte == LEFT_SHIFT_REL || input_byte == RIGHT_SHIFT_REL) {
		shift_active = !shift_active;
        return;
    } else if (input_byte == CTRL || input_byte == CTRL_REL) {
        ctrl_active = !ctrl_active;
        return;
    } else if (input_byte == CAPSLOCK) {
        capslock_active = !capslock_active;
        return;
    }

    if (input_byte < MAX_CODE) {

        if (!capslock_active && !shift_active)
            c = ascii_nomod[input_byte];

        else if (!capslock_active && shift_active)
            c = ascii_shift[input_byte];

        else if (capslock_active && shift_active)
            c = ascii_shift_capslock[input_byte];

        else
            c = ascii_capslock[input_byte];

        /* ctrl v */
        if ((c == 'v' || c == 'V') && ctrl_active) {
            subleq_io_write(335542256, 22);
            return;
        }
        /* ctrl x */
        if ((c == 'x' || c == 'X') && ctrl_active) {
            subleq_io_write(335542256, 23);
            return;
        }
        /* ctrl c */
        if ((c == 'c' || c == 'C') && ctrl_active) {
            subleq_io_write(335542256, 25);
            return;
        }

        switch (c) {
            case '\n':
                subleq_io_write(335542256, 13);
                break;
            case '\e':
                subleq_io_write(335542256, 27);
                break;
            default:
                subleq_io_write(335542256, (uint64_t)c);
                break;
        }

    }

    return;
}
