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
#define NAK_STOP_CONDITION '!'

// Command bytes
#define CMD_PREPARE 'p'
#define CMD_BEGIN 'b'
#define CMD_QUEUE_RATE 'q'
#define CMD_DATA 'd'
#define CMD_STOP 's'
#define CMD_ESTOP 0x00
#define CMD_ESTOP_ALT 0xFF
#define CMD_CLEAR_ESTOP 'c'
#define CMD_PING '?'

// DAC State flags (bit positions for status field)
#define STATE_BIT_EMERGENCY_STOP (1 << 0)
#define STATE_BIT_EMERGENCY_STOP_OLD (1 << 1)
#define STATE_BIT_PLAYING (1 << 2)

// Buffer configuration
#define BUFFER_CAPACITY 65535   // Max for uint16_t (protocol limit)
#define MAX_POINT_RATE 100000   // 100kpps

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

// DAC status structure (20 bytes - per Ether Dream spec)
struct dac_status {
    uint8_t protocol;           // Protocol version (0)
    uint8_t light_engine_state; // Light engine state (0=Ready, 3=E-Stop)
    uint8_t playback_state;     // Playback state (0=Idle, 1=Prepared, 2=Playing)
    uint8_t source;             // Data source (0=network streaming)
    uint16_t light_engine_flags;// Light engine flags
    uint16_t playback_flags;    // Playback flags
    uint16_t source_flags;      // Source flags
    uint16_t buffer_fullness;   // Number of points in buffer
    uint32_t point_rate;        // Current point rate (pps)
    uint32_t point_count;       // Points emitted since playback started
};

// Response packet sent to client (22 bytes - per Ether Dream spec)
struct dac_response {
    uint8_t response;       // ACK ('a') or NAK ('F', 'I', '!')
    uint8_t command;        // Echo of command being acknowledged
    struct dac_status status; // Full DAC status (20 bytes)
};

// UDP broadcast structure (sent every ~1 second for discovery - per Ether Dream spec)
struct dac_broadcast {
    uint8_t mac_address[6];     // MAC address
    uint16_t hw_revision;       // Hardware revision
    uint16_t sw_revision;       // Software/firmware revision
    uint16_t buffer_capacity;   // Buffer capacity in points
    uint32_t max_point_rate;    // Maximum point rate (pps)
    struct dac_status status;   // Full DAC status (20 bytes)
};

// 'b' Begin command payload
struct begin_command {
    uint16_t low_water_mark;  // Point threshold
    uint32_t point_rate;       // Points per second
};

// 'q' Queue Rate Change command payload
struct queue_rate_command {
    uint32_t point_rate;       // New point rate (pps)
};

// 'd' Data command header
struct data_command_header {
    uint16_t npoints;  // Number of points following
};

#pragma pack(pop)

// Light Engine States
enum class LightEngineState : uint8_t {
    READY = 0,
    WARMUP = 1,
    COOLDOWN = 2,
    ESTOP = 3
};

// Playback States
enum class PlaybackState : uint8_t {
    IDLE = 0,
    PREPARED = 1,
    PLAYING = 2
};

#endif // ETHERDREAM_PROTOCOL_H
