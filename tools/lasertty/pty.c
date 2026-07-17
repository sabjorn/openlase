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

#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "pty.h"

int pty_init(const char **slave_name) {
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd < 0) {
        perror("posix_openpt");
        return -1;
    }

    if (grantpt(master_fd) < 0) {
        perror("grantpt");
        close(master_fd);
        return -1;
    }

    if (unlockpt(master_fd) < 0) {
        perror("unlockpt");
        close(master_fd);
        return -1;
    }

    *slave_name = ptsname(master_fd);
    if (*slave_name == NULL) {
        perror("ptsname");
        close(master_fd);
        return -1;
    }

    int flags = fcntl(master_fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL");
        close(master_fd);
        return -1;
    }

    if (fcntl(master_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL");
        close(master_fd);
        return -1;
    }

    return master_fd;
}

int pty_read(int master_fd, char *buf, size_t len) {
    int n = read(master_fd, buf, len);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("pty_read");
        return -1;
    }
    if (n < 0) {
        return 0;
    }
    return n;
}

int pty_write(int master_fd, const char *buf, size_t len) {
    int n = write(master_fd, buf, len);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("pty_write");
        return -1;
    }
    if (n < 0) {
        return 0;
    }
    return n;
}

void pty_set_size(int master_fd, int rows, int cols) {
    struct winsize ws;
    ws.ws_row = rows;
    ws.ws_col = cols;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    if (ioctl(master_fd, TIOCSWINSZ, &ws) < 0) {
        perror("ioctl TIOCSWINSZ");
    }
}

int pty_spawn_shell(const char *slave_name) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        if (setsid() < 0) {
            perror("setsid");
            exit(1);
        }

        int slave_fd = open(slave_name, O_RDWR);
        if (slave_fd < 0) {
            perror("open slave");
            exit(1);
        }

        if (dup2(slave_fd, STDIN_FILENO) < 0 ||
            dup2(slave_fd, STDOUT_FILENO) < 0 ||
            dup2(slave_fd, STDERR_FILENO) < 0) {
            perror("dup2");
            exit(1);
        }

        if (slave_fd > STDERR_FILENO) {
            close(slave_fd);
        }

        const char *shell = getenv("SHELL");
        if (!shell) {
            shell = "/bin/bash";
        }

        char *args[] = { (char *)shell, NULL };
        execvp(shell, args);

        perror("execvp");
        exit(1);
    }

    return pid;
}
