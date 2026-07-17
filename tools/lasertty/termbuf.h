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

#ifndef TERMBUF_H
#define TERMBUF_H

#define TERM_ROWS 24
#define TERM_COLS 80

typedef struct {
    char cells[TERM_ROWS][TERM_COLS];
    int cursor_row;
    int cursor_col;
    int cursor_visible;
    int dirty;
} TerminalBuffer;

void termbuf_init(TerminalBuffer *tb);
void termbuf_clear(TerminalBuffer *tb);
void termbuf_putchar(TerminalBuffer *tb, char c);
void termbuf_scroll(TerminalBuffer *tb);
void termbuf_move_cursor(TerminalBuffer *tb, int row, int col);

#endif
