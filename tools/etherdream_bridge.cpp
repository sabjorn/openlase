/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>
Copyright (C) 2026 Ether Dream Bridge - Network interface for OpenLase

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "libol.h"
#include "etherdream_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>

// Global state
static std::atomic<bool> g_running{true};
static std::atomic<DACState> g_dac_state{DACState::IDLE};
static std::atomic<uint32_t> g_point_rate{30000};

// Thread-safe point buffer
static std::mutex g_points_mutex;
static std::vector<EtherDreamPoint> g_point_buffer;
static std::atomic<uint16_t> g_buffer_free{BUFFER_CAPACITY};

// Safety watchdog
static std::atomic<uint64_t> g_last_packet_time{0};

// Get current time in milliseconds
static uint64_t get_time_ms() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// Signal handler for clean shutdown
static void signal_handler(int sig) {
    (void)sig;
    printf("Caught signal, shutting down...\n");
    g_running = false;
}

// Send ACK response to client
static void send_ack(int client_fd, uint8_t command) {
    dac_response resp;
    resp.response = ACK;
    resp.command = command;
    resp.status = state_to_status(g_dac_state.load());
    resp.buffer_empty = g_buffer_free.load();
    send(client_fd, &resp, sizeof(resp), 0);
}

// Send NAK response to client
static void send_nak(int client_fd, uint8_t command, uint8_t nak_type) {
    dac_response resp;
    resp.response = nak_type;
    resp.command = command;
    resp.status = state_to_status(g_dac_state.load());
    resp.buffer_empty = g_buffer_free.load();
    send(client_fd, &resp, sizeof(resp), 0);
}

// Get points from buffer (thread-safe)
static std::vector<EtherDreamPoint> get_points_from_buffer() {
    std::lock_guard<std::mutex> lock(g_points_mutex);
    std::vector<EtherDreamPoint> points = g_point_buffer;
    int consumed = g_point_buffer.size();
    g_point_buffer.clear();
    g_buffer_free.fetch_add(consumed);
    return points;
}

// Add points to buffer (thread-safe)
static bool add_points_to_buffer(const std::vector<EtherDreamPoint>& points) {
    std::lock_guard<std::mutex> lock(g_points_mutex);

    if (g_point_buffer.size() + points.size() > BUFFER_CAPACITY) {
        return false;  // Buffer full
    }

    g_point_buffer.insert(g_point_buffer.end(), points.begin(), points.end());
    g_buffer_free.fetch_sub(points.size());
    return true;
}

// UDP Broadcast Thread - announces DAC presence every second
static void udp_broadcast_thread() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket creation failed");
        return;
    }

    int broadcast_enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("setsockopt SO_BROADCAST failed");
        close(sock);
        return;
    }

    sockaddr_in broadcast_addr{};
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(ETHERDREAM_UDP_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    dac_broadcast beacon{};
    // Set a recognizable MAC address (can be customized)
    memcpy(beacon.mac_address, "\x00\x0A\x95\x9D\x68\x16", 6);
    beacon.hw_revision = htons(3);
    beacon.sw_revision = htons(3);
    beacon.buffer_capacity = htons(BUFFER_CAPACITY);
    beacon.max_point_rate = htonl(MAX_POINT_RATE);

    printf("UDP broadcast thread started on port %d\n", ETHERDREAM_UDP_PORT);

    while (g_running) {
        beacon.status = htons(state_to_status(g_dac_state.load()));
        beacon.buffer_empty = htons(g_buffer_free.load());

        ssize_t sent = sendto(sock, &beacon, sizeof(beacon), 0,
                             (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

        if (sent < 0) {
            perror("sendto failed");
        }

        // Broadcast every second
        for (int i = 0; i < 10 && g_running; i++) {
            usleep(100000);  // 100ms x 10 = 1 second
        }
    }

    close(sock);
    printf("UDP broadcast thread stopped\n");
}

// TCP Server Thread - handles Ether Dream protocol commands
static void tcp_server_thread() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("TCP socket creation failed");
        return;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(ETHERDREAM_TCP_PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("TCP bind failed");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 1) < 0) {
        perror("TCP listen failed");
        close(server_fd);
        return;
    }

    // Set server socket to non-blocking mode so accept() can be interrupted
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    printf("TCP server listening on port %d\n", ETHERDREAM_TCP_PORT);

    while (g_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No connection pending, sleep briefly and check g_running
                usleep(10000);  // 10ms
                continue;
            }
            if (errno == EINTR) continue;
            perror("accept failed");
            continue;
        }

        printf("Client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Set receive timeout so we can check g_running periodically
        struct timeval tv;
        tv.tv_sec = 1;  // 1 second timeout
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Handle client commands
        bool client_connected = true;
        while (g_running && client_connected) {
            uint8_t command_byte;
            ssize_t bytes_read = recv(client_fd, &command_byte, 1, 0);

            if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Timeout - check g_running and continue
                    continue;
                }
                perror("recv failed");
                break;
            } else if (bytes_read == 0) {
                printf("Client disconnected\n");
                break;
            }

            g_last_packet_time = get_time_ms();

            switch (command_byte) {
                case CMD_PREPARE: {
                    if (g_dac_state == DACState::IDLE) {
                        // Clear buffer and transition to PREPARED
                        {
                            std::lock_guard<std::mutex> lock(g_points_mutex);
                            int cleared = g_point_buffer.size();
                            g_point_buffer.clear();
                            g_buffer_free.fetch_add(cleared);
                        }
                        g_dac_state = DACState::PREPARED;
                        send_ack(client_fd, CMD_PREPARE);
                        printf("State: PREPARED\n");
                    } else {
                        send_nak(client_fd, CMD_PREPARE, NAK_INVALID);
                    }
                    break;
                }

                case CMD_BEGIN: {
                    begin_command begin_cmd;
                    if (recv(client_fd, &begin_cmd, sizeof(begin_cmd), MSG_WAITALL) != sizeof(begin_cmd)) {
                        client_connected = false;
                        break;
                    }

                    uint32_t rate = ntohl(begin_cmd.point_rate);

                    if (g_dac_state == DACState::PREPARED) {
                        g_point_rate = rate;
                        g_dac_state = DACState::PLAYING;
                        send_ack(client_fd, CMD_BEGIN);
                        printf("State: PLAYING at %u pps\n", rate);
                    } else {
                        send_nak(client_fd, CMD_BEGIN, NAK_INVALID);
                    }
                    break;
                }

                case CMD_DATA: {
                    data_command_header data_hdr;
                    if (recv(client_fd, &data_hdr, sizeof(data_hdr), MSG_WAITALL) != sizeof(data_hdr)) {
                        client_connected = false;
                        break;
                    }

                    uint16_t npoints = ntohs(data_hdr.npoints);

                    if (npoints == 0 || npoints > BUFFER_CAPACITY) {
                        send_nak(client_fd, CMD_DATA, NAK_INVALID);
                        break;
                    }

                    std::vector<EtherDreamPoint> incoming(npoints);
                    ssize_t expected = npoints * sizeof(EtherDreamPoint);
                    ssize_t received = recv(client_fd, incoming.data(), expected, MSG_WAITALL);

                    if (received != expected) {
                        client_connected = false;
                        break;
                    }

                    // Convert network byte order
                    for (auto& pt : incoming) {
                        pt.control = ntohs(pt.control);
                        pt.x = (int16_t)ntohs(pt.x);
                        pt.y = (int16_t)ntohs(pt.y);
                        pt.r = ntohs(pt.r);
                        pt.g = ntohs(pt.g);
                        pt.b = ntohs(pt.b);
                        pt.i = ntohs(pt.i);
                        pt.u1 = ntohs(pt.u1);
                        pt.u2 = ntohs(pt.u2);
                    }

                    if (g_dac_state == DACState::PREPARED || g_dac_state == DACState::PLAYING) {
                        if (add_points_to_buffer(incoming)) {
                            send_ack(client_fd, CMD_DATA);
                        } else {
                            send_nak(client_fd, CMD_DATA, NAK_FULL);
                        }
                    } else {
                        send_nak(client_fd, CMD_DATA, NAK_INVALID);
                    }
                    break;
                }

                case CMD_STOP: {
                    g_dac_state = DACState::IDLE;
                    {
                        std::lock_guard<std::mutex> lock(g_points_mutex);
                        int cleared = g_point_buffer.size();
                        g_point_buffer.clear();
                        g_buffer_free.fetch_add(cleared);
                    }
                    send_ack(client_fd, CMD_STOP);
                    printf("State: IDLE (stopped)\n");
                    break;
                }

                case CMD_ESTOP:
                case CMD_ESTOP_ALT: {
                    printf("EMERGENCY STOP\n");
                    g_dac_state = DACState::IDLE;
                    {
                        std::lock_guard<std::mutex> lock(g_points_mutex);
                        int cleared = g_point_buffer.size();
                        g_point_buffer.clear();
                        g_buffer_free.fetch_add(cleared);
                    }
                    send_ack(client_fd, command_byte);
                    break;
                }

                default:
                    printf("Unknown command: 0x%02X\n", command_byte);
                    send_nak(client_fd, command_byte, NAK_INVALID);
                    break;
            }
        }

        close(client_fd);
        printf("Client handler terminated\n");

        // Reset to IDLE when client disconnects
        g_dac_state = DACState::IDLE;
    }

    close(server_fd);
    printf("TCP server thread stopped\n");
}

// Main OpenLase Rendering Loop
int main(int argc, char *argv[]) {
    printf("Ether Dream Bridge for OpenLase\n");
    printf("Copyright (C) 2026 - Network interface daemon\n\n");

    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize OpenLase
    if (olInit(3, 30000) < 0) {
        fprintf(stderr, "OpenLase initialization failed\n");
        return 1;
    }

    // Configure render parameters
    OLRenderParams params;
    memset(&params, 0, sizeof(params));
    params.rate = 48000;
    params.on_speed = 2.0 / 100.0;
    params.off_speed = 2.0 / 20.0;
    params.start_wait = 8;
    params.start_dwell = 3;
    params.curve_dwell = 0;
    params.corner_dwell = 8;
    params.curve_angle = cosf(30.0 * (M_PI / 180.0));
    params.end_dwell = 3;
    params.end_wait = 7;
    params.snap = 1.0 / 100000.0;
    params.render_flags = 0;  // Full color mode

    olSetRenderParams(&params);

    printf("OpenLase initialized (48kHz, full color)\n");
    printf("Buffer capacity: %d points\n", BUFFER_CAPACITY);
    printf("Max point rate: %d pps\n\n", MAX_POINT_RATE);

    // Start network threads
    std::thread udp_thread(udp_broadcast_thread);
    std::thread tcp_thread(tcp_server_thread);

    printf("Network threads started, ready for connections\n\n");

    // Initialize watchdog
    g_last_packet_time = get_time_ms();

    // Main rendering loop
    int frame_count = 0;
    while (g_running) {
        olLoadIdentity();

        // Check safety watchdog
        uint64_t now = get_time_ms();
        uint64_t last_packet = g_last_packet_time.load();
        bool timeout = (now - last_packet) > SAFETY_TIMEOUT_MS;

        if (timeout && g_dac_state == DACState::PLAYING) {
            // Safety timeout - clear buffer and render blank
            printf("WARNING: Safety timeout (%llu ms), blanking output\n", now - last_packet);
            std::lock_guard<std::mutex> lock(g_points_mutex);
            int cleared = g_point_buffer.size();
            g_point_buffer.clear();
            g_buffer_free.fetch_add(cleared);
        }

        // Get points from buffer
        auto points = get_points_from_buffer();

        if (!points.empty()) {
            olBegin(OL_LINESTRIP);

            for (const auto& pt : points) {
                // Convert coordinates: int16 [-32768, 32767] -> float [-1.0, 1.0]
                float x = (float)pt.x / 32768.0f;
                float y = (float)pt.y / 32768.0f;

                // Check blanking bit (control field bit 0)
                uint32_t color;
                if (pt.control & 0x01) {
                    // Blanking bit set - laser off
                    color = 0x000000;
                } else {
                    // Convert colors: uint16 [0, 65535] -> uint8 [0, 255]
                    uint8_t r = pt.r >> 8;
                    uint8_t g = pt.g >> 8;
                    uint8_t b = pt.b >> 8;
                    // Pack into RGB uint32_t
                    color = (r << 16) | (g << 8) | b;
                }

                olVertex(x, y, color);
            }

            olEnd();
        } else {
            // No points - render a single blank point for safety
            olBegin(OL_POINTS);
            olVertex(0.0f, 0.0f, 0x000000);
            olEnd();
        }

        // Render frame at 60 FPS
        float ftime = olRenderFrame(60);
        frame_count++;

        if (frame_count % 300 == 0) {  // Every ~5 seconds at 60fps
            printf("Status: %d frames, state=%d, buffer_free=%d\n",
                   frame_count, (int)g_dac_state.load(), g_buffer_free.load());
        }
    }

    printf("\nShutting down...\n");

    // Wait for network threads to finish
    udp_thread.join();
    tcp_thread.join();

    // Shutdown OpenLase
    olShutdown();

    printf("Shutdown complete\n");
    return 0;
}
