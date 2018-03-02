#include <kernel.h>
#include <stdint.h>
#include <cio.h>
#include <klib.h>
#include <tty.h>

#define MAX_CODE 0x57
#define CAPSLOCK 0x3A
#define RIGHT_SHIFT 0x36
#define LEFT_SHIFT 0x2A
#define RIGHT_SHIFT_REL 0xB6
#define LEFT_SHIFT_REL 0xAA
#define LEFT_CTRL 0x1D
#define LEFT_CTRL_REL 0x9D

static int capslock_active = 0;
static int shift_active = 0;
static int ctrl_active = 0;

static const char ascii_capslock[] = {
    '\0', '?', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
};

static const char ascii_shift[] = {
    '\0', '?', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char ascii_shift_capslock[] = {
    '\0', '?', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char ascii_nomod[] = {
    '\0', '?', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

extern uint64_t _readram(uint64_t);
extern void _writeram(uint64_t, uint64_t);

void keyboard_handler(uint8_t input_byte) {
    char c;

    switch (input_byte) {
        case 0x4b:
            /* keypad 4 */
            _writeram(335542176 + 3 * 8, -0x3000000);
            return;
        case 0x4d:
            /* keypad 6 */
            _writeram(335542176 + 3 * 8, 0x3000000);
            return;
        case 0x48:
            /* keypad 8 */
            _writeram(335542176 + 4 * 8, -0x3000000);
            return;
        case 0x50:
            /* keypad 2 */
            _writeram(335542176 + 4 * 8, 0x3000000);
            return;
        case 0x47:
            /* keypad 7 */
            _writeram(335542176 + 0 * 8, 0x100000000);
            return;
        case 0xc7:
            /* keypad 7 rel */
            _writeram(335542176 + 0 * 8, 0);
            return;
        case 0x49:
            /* keypad 9 */
            _writeram(335542176 + 1 * 8, 0x100000000);
            return;
        case 0xc9:
            /* keypad 9 rel */
            _writeram(335542176 + 1 * 8, 0);
            return;
        case 0x4c:
            /* keypad 5 */
            _writeram(335542176 + 2 * 8, 0x100000000);
            return;
        case 0xcc:
            /* keypad 5 rel */
            _writeram(335542176 + 2 * 8, 0);
            return;
        default:
            break;
    }

    if (input_byte == LEFT_SHIFT || input_byte == RIGHT_SHIFT || input_byte == LEFT_SHIFT_REL || input_byte == RIGHT_SHIFT_REL)
		shift_active = !shift_active;

    else if (input_byte == LEFT_CTRL || input_byte == LEFT_CTRL_REL)
        ctrl_active = !ctrl_active;

    else if (tty[current_tty].kb_l1_buffer_index < KB_L1_SIZE) {

        if (input_byte < MAX_CODE) {
            
            if (!capslock_active && !shift_active)
                c = ascii_nomod[input_byte];

            else if (!capslock_active && shift_active)
                c = ascii_shift[input_byte];

            else if (capslock_active && shift_active)
                c = ascii_shift_capslock[input_byte];

            else
                c = ascii_capslock[input_byte];

            /* TODO */
            /* implement special keys */

            _writeram(335542256, (uint64_t)c);

        }

    }

    return;
}
