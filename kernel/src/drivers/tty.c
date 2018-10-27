#include <tty.h>
#include <vbe_tty.h>
#include <vga_textmode.h>

void tty_putchar(char c) {
    if (!vbe_tty_available) {
        text_putchar(c);
    } else {
        vbe_tty_putchar(c);
    }

    return;
}
