/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <string.h>
#include "termbuf.h"

void termbuf_init(TerminalBuffer *tb) {
    memset(tb, 0, sizeof(TerminalBuffer));
    tb->cursor_visible = 1;
    tb->dirty = 1;
}

void termbuf_clear(TerminalBuffer *tb) {
    memset(tb->cells, ' ', sizeof(tb->cells));
    tb->cursor_row = 0;
    tb->cursor_col = 0;
    tb->dirty = 1;
}

void termbuf_scroll(TerminalBuffer *tb) {
    memmove(tb->cells[0], tb->cells[1], sizeof(tb->cells[0]) * (TERM_ROWS - 1));
    memset(tb->cells[TERM_ROWS - 1], ' ', TERM_COLS);
    tb->dirty = 1;
}

void termbuf_move_cursor(TerminalBuffer *tb, int row, int col) {
    if (row >= 0 && row < TERM_ROWS) {
        tb->cursor_row = row;
    }
    if (col >= 0 && col < TERM_COLS) {
        tb->cursor_col = col;
    }
    tb->dirty = 1;
}

void termbuf_putchar(TerminalBuffer *tb, char c) {
    if (c >= 0x20 && c <= 0x7E) {
        if (tb->cursor_col < TERM_COLS) {
            tb->cells[tb->cursor_row][tb->cursor_col] = c;
            tb->cursor_col++;
        }

        if (tb->cursor_col >= TERM_COLS) {
            tb->cursor_col = 0;
            tb->cursor_row++;
            if (tb->cursor_row >= TERM_ROWS) {
                tb->cursor_row = TERM_ROWS - 1;
                termbuf_scroll(tb);
            }
        }
    } else if (c == '\n') {
        tb->cursor_col = 0;
        tb->cursor_row++;
        if (tb->cursor_row >= TERM_ROWS) {
            tb->cursor_row = TERM_ROWS - 1;
            termbuf_scroll(tb);
        }
    } else if (c == '\r') {
        tb->cursor_col = 0;
    } else if (c == '\b') {
        if (tb->cursor_col > 0) {
            tb->cursor_col--;
        }
    } else if (c == '\t') {
        int next_tab = ((tb->cursor_col + 8) / 8) * 8;
        if (next_tab >= TERM_COLS) {
            tb->cursor_col = 0;
            tb->cursor_row++;
            if (tb->cursor_row >= TERM_ROWS) {
                tb->cursor_row = TERM_ROWS - 1;
                termbuf_scroll(tb);
            }
        } else {
            tb->cursor_col = next_tab;
        }
    }

    tb->dirty = 1;
}
