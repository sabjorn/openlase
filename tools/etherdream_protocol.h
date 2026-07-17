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

#ifndef ETHERDREAM_PROTOCOL_H
#define ETHERDREAM_PROTOCOL_H

#include <stdint.h>

// Ether Dream Protocol Constants
#define ETHERDREAM_UDP_PORT 7654
#define ETHERDREAM_TCP_PORT 7765

// Protocol response codes
#define ACK 'a'
#define NAK_FULL 'F'
#define NAK_INVALID 'I'

// Command bytes
#define CMD_PREPARE 'p'
#define CMD_BEGIN 'b'
#define CMD_DATA 'd'
#define CMD_STOP 's'
#define CMD_ESTOP 0x00
#define CMD_ESTOP_ALT 0xFF

// DAC State flags (bit positions for status field)
#define STATE_BIT_EMERGENCY_STOP (1 << 0)
#define STATE_BIT_EMERGENCY_STOP_OLD (1 << 1)
#define STATE_BIT_PLAYING (1 << 2)

// Buffer configuration
#define BUFFER_CAPACITY 4000
#define MAX_POINT_RATE 100000  // 100kpps

// Safety timeout (milliseconds)
#define SAFETY_TIMEOUT_MS 50

#pragma pack(push, 1)

// 18-byte point structure sent from client
struct EtherDreamPoint {
    uint16_t control;       // Bit 15: Change rate flag, others reserved
    int16_t x;              // -32768 to 32767
    int16_t y;              // -32768 to 32767
    uint16_t r;             // 0 to 65535
    uint16_t g;             // 0 to 65535
    uint16_t b;             // 0 to 65535
    uint16_t i;             // Intensity (often unused)
    uint16_t u1;            // User field 1
    uint16_t u2;            // User field 2
};

// 6-byte response packet sent to client
struct dac_response {
    uint8_t response;       // ACK ('a') or NAK ('F', 'I')
    uint8_t command;        // Echo of command being acknowledged
    uint16_t status;        // DAC status flags
    uint16_t buffer_empty;  // Number of free points in buffer
};

// UDP broadcast structure (sent every ~1 second for discovery)
struct dac_broadcast {
    uint8_t mac_address[6];
    uint16_t hw_revision;
    uint16_t sw_revision;
    uint16_t buffer_capacity;
    uint32_t max_point_rate;
    uint16_t status;
    uint16_t buffer_empty;
};

// 'b' Begin command payload
struct begin_command {
    uint16_t low_water_mark;  // Point threshold
    uint32_t point_rate;       // Points per second
};

// 'd' Data command header
struct data_command_header {
    uint16_t npoints;  // Number of points following
};

#pragma pack(pop)

// DAC State Machine
enum class DACState : uint8_t {
    IDLE = 0,
    PREPARED = 1,
    PLAYING = 2
};

// Helper to convert DACState to status bits
inline uint16_t state_to_status(DACState state) {
    switch (state) {
        case DACState::IDLE:
            return 0;
        case DACState::PREPARED:
            return 0;
        case DACState::PLAYING:
            return STATE_BIT_PLAYING;
        default:
            return 0;
    }
}

#endif // ETHERDREAM_PROTOCOL_H
