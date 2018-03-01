#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <tty.h>
#include <graphics.h>
#include <panic.h>
#include <bios_io.h>

static int ttys_ready = 0;
int tty_needs_refresh = -1;

static int rows;
static int cols;

static void escape_parse(char c, uint8_t which_tty);
static void text_set_cursor_pos(uint32_t x, uint32_t y, uint8_t which_tty);

static void plot_char(char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg, uint8_t which_tty) {
    int orig_x = x;

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            if ((vga_font[c * 16 + i] >> 7 - j) & 1)
                plot_px(x++, y, hex_fg, which_tty);
            else
                plot_px(x++, y, hex_bg, which_tty);
        }
        y++;
        x = orig_x;
    }

    return;
}

static void plot_char_grid(char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg, uint8_t which_tty) {
    plot_char(c, x * 8, y * 16, hex_fg, hex_bg, which_tty);
    tty[which_tty].grid[x + y * cols] = c;
    tty[which_tty].gridfg[x + y * cols] = hex_fg;
    tty[which_tty].gridbg[x + y * cols] = hex_bg;
    return;
}

static void clear_cursor(uint8_t which_tty) {
    plot_char(tty[which_tty].grid[tty[which_tty].cursor_x + tty[which_tty].cursor_y * cols],
        tty[which_tty].cursor_x * 8, tty[which_tty].cursor_y * 16,
        tty[which_tty].text_fg_col, tty[which_tty].text_bg_col, which_tty);
    return;
}

static void draw_cursor(uint8_t which_tty) {
    if (tty[which_tty].cursor_status)
        plot_char(tty[which_tty].grid[tty[which_tty].cursor_x + tty[which_tty].cursor_y * cols],
            tty[which_tty].cursor_x * 8, tty[which_tty].cursor_y * 16,
            tty[which_tty].cursor_fg_col, tty[which_tty].cursor_bg_col, which_tty);
    return;
}

void tty_refresh(int which_tty) {
    if (which_tty == current_tty) {
        /* interpret the grid and print the chars */
        for (size_t i = 0; i < (rows * cols); i++) {
            plot_char_grid(tty[which_tty].grid[i], i % cols, i / cols,
                tty[which_tty].gridfg[i], tty[which_tty].gridbg[i], which_tty);
        }
        draw_cursor(which_tty);
    }
    return;
}

static void scroll(uint8_t which_tty) {
    /* notify grid */
    for (int i = cols; i < rows * cols; i++) {
        tty[which_tty].grid[i - cols] = tty[which_tty].grid[i];
        tty[which_tty].gridbg[i - cols] = tty[which_tty].gridbg[i];
        tty[which_tty].gridfg[i - cols] = tty[which_tty].gridfg[i];
    }
    /* clear the last line of the screen */
    for (int i = rows * cols - cols; i < rows * cols; i++) {
        tty[which_tty].grid[i] = ' ';
        tty[which_tty].gridbg[i] = tty[which_tty].text_bg_col;
        tty[which_tty].gridfg[i] = tty[which_tty].text_fg_col;
    }
    tty_needs_refresh = which_tty;
    return;
}

static void text_clear(uint8_t which_tty) {
    for (size_t i = 0; i < (rows * cols); i++) {
        tty[which_tty].grid[i] = ' ';
        tty[which_tty].gridbg[i] = tty[which_tty].text_bg_col;
        tty[which_tty].gridfg[i] = tty[which_tty].text_fg_col;
    }
    tty[which_tty].cursor_x = 0;
    tty[which_tty].cursor_y = 0;
    tty_refresh(which_tty);
    return;
}

static void text_clear_no_move(uint8_t which_tty) {
    for (size_t i = 0; i < (rows * cols); i++) {
        tty[which_tty].grid[i] = ' ';
        tty[which_tty].gridbg[i] = tty[which_tty].text_bg_col;
        tty[which_tty].gridfg[i] = tty[which_tty].text_fg_col;
    }
    tty_refresh(which_tty);
    return;
}

static void text_enable_cursor(uint8_t which_tty) {
    tty[which_tty].cursor_status = 1;
    draw_cursor(which_tty);
    return;
}

static void text_disable_cursor(uint8_t which_tty) {
    tty[which_tty].cursor_status = 0;
    clear_cursor(which_tty);
    return;
}

void text_putchar(char c, uint8_t which_tty) {
    if (!ttys_ready && !modeset_done) {
        char cstr[2] = {0,0};
        cstr[0] = c;
        bios_print(cstr);
        return;
    }

    if (tty[which_tty].escape) {
        escape_parse(c, which_tty);
        return;
    }
    switch (c) {
        case 0x00:
            break;
        case 0x1B:
            tty[which_tty].escape = 1;
            return;
        case 0x0A:
            if (tty[which_tty].cursor_y == (rows - 1)) {
                text_set_cursor_pos(0, (rows - 1), which_tty);
                scroll(which_tty);
            } else
                text_set_cursor_pos(0, (tty[which_tty].cursor_y + 1), which_tty);
            break;
        case 0x08:
            if (tty[which_tty].cursor_x || tty[which_tty].cursor_y) {
                clear_cursor(which_tty);
                if (tty[which_tty].cursor_x)
                    tty[which_tty].cursor_x--;
                else {
                    tty[which_tty].cursor_y--;
                    tty[which_tty].cursor_x = cols - 1;
                }
                plot_char_grid(' ', tty[which_tty].cursor_x, tty[which_tty].cursor_y,
                    tty[which_tty].text_fg_col, tty[which_tty].text_bg_col, which_tty);
                draw_cursor(which_tty);
            }
            break;
        default:
            plot_char_grid(c, tty[which_tty].cursor_x++, tty[which_tty].cursor_y,
                tty[which_tty].text_fg_col, tty[which_tty].text_bg_col, which_tty);
            if (tty[which_tty].cursor_x == cols) {
                tty[which_tty].cursor_x = 0;
                tty[which_tty].cursor_y++;
            }
            if (tty[which_tty].cursor_y == rows) {
                tty[which_tty].cursor_y--;
                if (tty[which_tty].noscroll)
                    goto dont_move;
                scroll(which_tty);
            }
dont_move:
            draw_cursor(which_tty);
    }
    return;
}

static uint32_t ansi_colours[] = {
    0x00000000,              /* black */
    0x00aa0000,              /* red */
    0x0000aa00,              /* green */
    0x00aa5500,              /* brown */
    0x000000aa,              /* blue */
    0x00aa00aa,              /* magenta */
    0x0000aaaa,              /* cyan */
    0x00aaaaaa,              /* grey */
};

static void sgr(uint8_t which_tty) {

    if (tty[which_tty].esc_value0 >= 30 && tty[which_tty].esc_value0 <= 37) {
        tty[which_tty].text_fg_col = ansi_colours[tty[which_tty].esc_value0 - 30];
        return;
    }

    if (tty[which_tty].esc_value0 >= 40 && tty[which_tty].esc_value0 <= 47) {
        tty[which_tty].text_bg_col = ansi_colours[tty[which_tty].esc_value0 - 40];
        return;
    }

    return;
}

static void escape_parse(char c, uint8_t which_tty) {
    
    if (c >= '0' && c <= '9') {
        *tty[which_tty].esc_value *= 10;
        *tty[which_tty].esc_value += c - '0';
        *tty[which_tty].esc_default = 0;
        return;
    }

    switch (c) {
        case '[':
            return;
        case ';':
            tty[which_tty].esc_value = &tty[which_tty].esc_value1;
            tty[which_tty].esc_default = &tty[which_tty].esc_default1;
            return;
        case 'A':
            if (tty[which_tty].esc_default0) tty[which_tty].esc_value0 = 1;
            if (tty[which_tty].esc_value0 > tty[which_tty].cursor_y)
                tty[which_tty].esc_value0 = tty[which_tty].cursor_y;
            text_set_cursor_pos(tty[which_tty].cursor_x,
                                tty[which_tty].cursor_y - tty[which_tty].esc_value0,
                                which_tty);
            break;
        case 'B':
            if (tty[which_tty].esc_default0) tty[which_tty].esc_value0 = 1;
            if ((tty[which_tty].cursor_y + tty[which_tty].esc_value0) >
                (rows - 1))
                tty[which_tty].esc_value0 = (rows - 1) - tty[which_tty].cursor_y;
            text_set_cursor_pos(tty[which_tty].cursor_x,
                                tty[which_tty].cursor_y + tty[which_tty].esc_value0,
                                which_tty);
            break;
        case 'C':
            if (tty[which_tty].esc_default0) tty[which_tty].esc_value0 = 1;
            if ((tty[which_tty].cursor_x + tty[which_tty].esc_value0) >
                (cols - 1))
                tty[which_tty].esc_value0 = (cols - 1) - tty[which_tty].cursor_x;
            text_set_cursor_pos(tty[which_tty].cursor_x + tty[which_tty].esc_value0,
                                tty[which_tty].cursor_y,
                                which_tty);
            break;
        case 'D':
            if (tty[which_tty].esc_default0) tty[which_tty].esc_value0 = 1;
            if (tty[which_tty].esc_value0 > tty[which_tty].cursor_x)
                tty[which_tty].esc_value0 = tty[which_tty].cursor_x;
            text_set_cursor_pos(tty[which_tty].cursor_x - tty[which_tty].esc_value0,
                                tty[which_tty].cursor_y,
                                which_tty);
            break;
        case 'H':
            tty[which_tty].esc_value0 -= 1;
            tty[which_tty].esc_value1 -= 1;
            if (tty[which_tty].esc_default0) tty[which_tty].esc_value0 = 0;
            if (tty[which_tty].esc_default1) tty[which_tty].esc_value1 = 0;
            if (tty[which_tty].esc_value1 >= cols)
                tty[which_tty].esc_value1 = cols - 1;
            if (tty[which_tty].esc_value0 >= rows)
                tty[which_tty].esc_value0 = rows - 1;
            text_set_cursor_pos(tty[which_tty].esc_value1, tty[which_tty].esc_value0, which_tty);
            break;
        case 'm':
            sgr(which_tty);
            break;
        case 'J':
            switch (tty[which_tty].esc_value0) {
                case 2:
                    text_clear_no_move(which_tty);
                    break;
                default:
                    break;
            }
            break;
        /* non-standard sequences */
        case 'r': /* enter/exit raw mode */
            tty[which_tty].raw = !tty[which_tty].raw;
            break;
        case 'b': /* enter/exit non-blocking mode */
            tty[which_tty].noblock = !tty[which_tty].noblock;
            break;
        case 's': /* enter/exit non-scrolling mode */
            tty[which_tty].noscroll = !tty[which_tty].noscroll;
            break;
        /* end non-standard sequences */
        default:
            tty[which_tty].escape = 0;
            text_putchar('?', which_tty);
            break;
    }
    
    tty[which_tty].esc_value = &tty[which_tty].esc_value0;
    tty[which_tty].esc_value0 = 0;
    tty[which_tty].esc_value1 = 0;
    tty[which_tty].esc_default = &tty[which_tty].esc_default0;
    tty[which_tty].esc_default0 = 1;
    tty[which_tty].esc_default1 = 1;
    tty[which_tty].escape = 0;

    return;
}

static void text_set_cursor_pos(uint32_t x, uint32_t y, uint8_t which_tty) {
    clear_cursor(which_tty);
    tty[which_tty].cursor_x = x;
    tty[which_tty].cursor_y = y;
    draw_cursor(which_tty);
    return;
}

// -- tty --

tty_t tty[KRNL_TTY_COUNT];
uint8_t current_tty = 0;

void switch_tty(uint8_t which_tty) {
    current_tty = which_tty;
    tty_refresh(current_tty);
    return;
}

void init_tty(void) {
    cols = edid_width / 8;
    rows = edid_height / 16;

    for (int i = 0; i < KRNL_TTY_COUNT; i++) {
        tty[i].esc_value = &tty[i].esc_value0;
        tty[i].esc_value0 = 0;
        tty[i].esc_value1 = 0;
        tty[i].esc_default = &tty[i].esc_default0;
        tty[i].esc_default0 = 1;
        tty[i].esc_default1 = 1;
        tty[i].escape = 0;
        tty[i].cursor_x = 0;
        tty[i].cursor_y = 0;
        tty[i].cursor_status = 1;
        tty[i].cursor_bg_col = TTY_DEF_CUR_BG_COL;
        tty[i].cursor_fg_col = TTY_DEF_CUR_FG_COL;
        tty[i].text_bg_col = TTY_DEF_TXT_BG_COL;
        tty[i].text_fg_col = TTY_DEF_TXT_FG_COL;
        tty[i].raw = 0;
        tty[i].noblock = 0;
        tty[i].noscroll = 0;
        tty[i].grid = kalloc(rows * cols);
        tty[i].gridbg = kalloc(rows * cols * sizeof(uint32_t));
        tty[i].gridfg = kalloc(rows * cols * sizeof(uint32_t));
        if (!tty[i].grid || !tty[i].gridbg || !tty[i].gridfg)
            panic("Out of memory while allocating TTYs", 0);
        for (size_t j = 0; j < (rows * cols); j++) {
            tty[i].grid[j] = ' ';
            tty[i].gridbg[j] = TTY_DEF_TXT_BG_COL;
            tty[i].gridfg[j] = TTY_DEF_TXT_FG_COL;
        }
    }

    current_tty = 0;
    ttys_ready = 1;
    tty_refresh(current_tty);
    return;
}
