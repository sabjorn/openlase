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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "libol.h"
#include "text.h"
#include "pty.h"
#include "termbuf.h"
#include "input.h"

static volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
}

long long get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void render_terminal(TerminalBuffer *tb, Font *font) {
    olLoadIdentity();

    float char_height = 0.08;
    float char_width = olGetCharWidth(font, 'M') * char_height;

    float start_x = -(TERM_COLS * char_width) / 2.0;
    float start_y = (TERM_ROWS * char_height) / 2.0;

    for (int row = 0; row < TERM_ROWS; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            char c = tb->cells[row][col];
            if (c >= 0x20 && c <= 0x7E) {
                float x = start_x + col * char_width;
                float y = start_y - row * char_height;
                olDrawChar(font, x, y, char_height, C_WHITE, c);
            }
        }
    }

    if (tb->cursor_visible && (get_time_ms() % 1000) < 500) {
        float x = start_x + tb->cursor_col * char_width;
        float y = start_y - tb->cursor_row * char_height;
        olRect(x, y - char_height, x + char_width, y, C_WHITE);
    }
}

int main(int argc, char **argv) {
    OLRenderParams params;

    memset(&params, 0, sizeof(params));
    params.rate = 48000;
    params.on_speed = 2.0/100.0;
    params.off_speed = 2.0/20.0;
    params.start_wait = 8;
    params.start_dwell = 3;
    params.curve_dwell = 0;
    params.corner_dwell = 8;
    params.curve_angle = cosf(30.0 * (M_PI / 180.0));
    params.end_dwell = 3;
    params.end_wait = 7;
    params.snap = 1/100000.0;
    params.render_flags = RENDER_GRAYSCALE;

    if (olInit(3, 30000) < 0) {
        fprintf(stderr, "OpenLase init failed\n");
        return 1;
    }

    olSetRenderParams(&params);
    Font *font = olGetDefaultFont();

    const char *slave_name;
    int pty_fd = pty_init(&slave_name);
    if (pty_fd < 0) {
        fprintf(stderr, "PTY init failed\n");
        olShutdown();
        return 1;
    }

    pty_set_size(pty_fd, TERM_ROWS, TERM_COLS);

    int shell_pid = pty_spawn_shell(slave_name);
    if (shell_pid < 0) {
        fprintf(stderr, "Shell spawn failed\n");
        close(pty_fd);
        olShutdown();
        return 1;
    }

    TerminalBuffer tb;
    termbuf_init(&tb);

    input_init();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fd_set readfds;
    char buf[256];

    while (running) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(pty_fd, &readfds);

        struct timeval tv = { 0, 16666 };
        int ret = select(pty_fd + 1, &readfds, NULL, NULL, &tv);

        if (ret < 0) {
            if (running) {
                perror("select");
            }
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            int n = input_read(buf, sizeof(buf));
            if (n > 0) {
                pty_write(pty_fd, buf, n);
            } else if (n < 0) {
                break;
            }
        }

        if (FD_ISSET(pty_fd, &readfds)) {
            int n = pty_read(pty_fd, buf, sizeof(buf));
            if (n > 0) {
                for (int i = 0; i < n; i++) {
                    termbuf_putchar(&tb, buf[i]);
                }
            } else if (n < 0) {
                break;
            }
        }

        render_terminal(&tb, font);
        olRenderFrame(60);
    }

    input_cleanup();
    close(pty_fd);
    olShutdown();

    printf("\nLaser TTY terminated\n");

    return 0;
}
