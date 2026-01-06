#include <keyboard.h>
#include <cpu.h>
#include <mouse.h>
#include <dawn.h>

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

static const uint8_t ascii_capslock[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_shift_capslock[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const uint8_t ascii_nomod[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

static int extra_scancodes = 0;

void keyboard_handler(uint8_t input_byte) {
    uint8_t c;

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
                    dawn_io_write(DAWN_KEYBOARD_BASE, 18);
                else
                    dawn_io_write(DAWN_KEYBOARD_BASE, 14);
                break;
            case 0x4B:
                /* cursor left */
                if (shift_active)
                    dawn_io_write(DAWN_KEYBOARD_BASE, 19);
                else if (ctrl_active)
                    dawn_io_write(DAWN_KEYBOARD_BASE, 4);
                else
                    dawn_io_write(DAWN_KEYBOARD_BASE, 15);
                break;
            case 0x50:
                /* cursor down */
                if (shift_active)
                    dawn_io_write(DAWN_KEYBOARD_BASE, 20);
                else
                    dawn_io_write(DAWN_KEYBOARD_BASE, 16);
                break;
            case 0x4D:
                /* cursor right */
                if (shift_active)
                    dawn_io_write(DAWN_KEYBOARD_BASE, 21);
                else if (ctrl_active)
                    dawn_io_write(DAWN_KEYBOARD_BASE, 7);
                else
                    dawn_io_write(DAWN_KEYBOARD_BASE, 17);
                break;
            case 0x49:
                /* pgup */
                dawn_io_write(DAWN_KEYBOARD_BASE, 9);
                break;
            case 0x51:
                /* pgdown */
                dawn_io_write(DAWN_KEYBOARD_BASE, 10);
                break;
            case 0x53:
                /* delete */
                dawn_io_write(DAWN_KEYBOARD_BASE, 29);
                break;
            case CTRL:
                ctrl_active = 1;
                break;
            case CTRL_REL:
                ctrl_active = 0;
                break;
        }
        return;
    }

    switch (input_byte) {
        case 0x4b:
            /* keypad 4 */
            hw_mouse_enabled = 0;
            dawn_io_write(DAWN_MOUSE_REL_X, -0x3000000);
            return;
        case 0x4d:
            /* keypad 6 */
            hw_mouse_enabled = 0;
            dawn_io_write(DAWN_MOUSE_REL_X, 0x3000000);
            return;
        case 0x48:
            /* keypad 8 */
            hw_mouse_enabled = 0;
            dawn_io_write(DAWN_MOUSE_REL_Y, -0x3000000);
            return;
        case 0x50:
            /* keypad 2 */
            hw_mouse_enabled = 0;
            dawn_io_write(DAWN_MOUSE_REL_Y, 0x3000000);
            return;
        case 0x47:
            /* keypad 7 */
            hw_mouse_enabled = 0;
            dawn_writeram(DAWN_MOUSE_CLICK_L, 0x100000000);
            return;
        case 0xc7:
            /* keypad 7 rel */
            hw_mouse_enabled = 0;
            dawn_writeram(DAWN_MOUSE_CLICK_L, 0);
            return;
        case 0x49:
            /* keypad 9 */
            hw_mouse_enabled = 0;
            dawn_writeram(DAWN_MOUSE_CLICK_R, 0x100000000);
            return;
        case 0xc9:
            /* keypad 9 rel */
            hw_mouse_enabled = 0;
            dawn_writeram(DAWN_MOUSE_CLICK_R, 0);
            return;
        case 0x4c:
            /* keypad 5 */
            hw_mouse_enabled = 0;
            dawn_writeram(DAWN_MOUSE_CLICK_M, 0x100000000);
            return;
        case 0xcc:
            /* keypad 5 rel */
            hw_mouse_enabled = 0;
            dawn_writeram(DAWN_MOUSE_CLICK_M, 0);
            return;
        default:
            break;
    }

    switch (input_byte) {
        case LEFT_SHIFT:
        case RIGHT_SHIFT:
            shift_active = 1;
            return;
        case LEFT_SHIFT_REL:
        case RIGHT_SHIFT_REL:
            shift_active = 0;
            return;
        case CTRL:
            ctrl_active = 1;
            return;
        case CTRL_REL:
            ctrl_active = 0;
            return;
        case CAPSLOCK:
            capslock_active = !capslock_active;
            return;
        default:
            break;
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
            dawn_io_write(DAWN_KEYBOARD_BASE, 22);
            return;
        }
        /* ctrl x */
        if ((c == 'x' || c == 'X') && ctrl_active) {
            dawn_io_write(DAWN_KEYBOARD_BASE, 23);
            return;
        }
        /* ctrl c */
        if ((c == 'c' || c == 'C') && ctrl_active) {
            dawn_io_write(DAWN_KEYBOARD_BASE, 25);
            return;
        }

        switch (c) {
            case '\n':
                dawn_io_write(DAWN_KEYBOARD_BASE, 13);
                break;
            case '\e':
                dawn_io_write(DAWN_KEYBOARD_BASE, 27);
                break;
            default:
                dawn_io_write(DAWN_KEYBOARD_BASE, (uint64_t)c);
                break;
        }

    }

    return;
}
