#!/usr/bin/env python3
"""
Simple Ether Dream protocol test client
Sends test patterns to the etherdream_bridge
"""

import socket
import struct
import time
import math
import argparse

# Ether Dream protocol constants
ETHERDREAM_PORT = 7765
CMD_PREPARE = ord('p')
CMD_BEGIN = ord('b')
CMD_DATA = ord('d')
CMD_STOP = ord('s')

ACK = ord('a')

class EtherDreamClient:
    def __init__(self, host='localhost', port=ETHERDREAM_PORT):
        self.host = host
        self.port = port
        self.sock = None

    def connect(self):
        """Connect to the Ether Dream DAC"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)  # 5 second timeout
        print(f"Connecting to {self.host}:{self.port}...")
        self.sock.connect((self.host, self.port))
        print("TCP connection established, waiting for initial status...")
        # Read initial status response (per Ether Dream protocol spec)
        try:
            initial_response = self.sock.recv(22)
            print(f"Received {len(initial_response)} bytes")
            if len(initial_response) == 22:
                print(f"Connected to {self.host}:{self.port} (received initial status)")
            else:
                print(f"Connected to {self.host}:{self.port} (warning: unexpected initial response length: {len(initial_response)})")
        except socket.timeout:
            print("WARNING: Timeout waiting for initial status response")
        self.sock.settimeout(None)  # Remove timeout for normal operation

    def send_command(self, command):
        """Send a command and wait for ACK"""
        self.sock.send(bytes([command]))
        response = self.sock.recv(22)  # dac_response is 22 bytes (response + command + 22-byte status)
        if len(response) >= 1 and response[0] == ACK:
            print(f"Command {chr(command)} ACKed")
            return True
        else:
            print(f"Command {chr(command)} failed: {response.hex()}")
            return False

    def send_data(self, points):
        """
        Send a data frame with points
        Format: 'd' + uint16_le(num_points) + points
        Each point: control(u16), x(i16), y(i16), r(u16), g(u16), b(u16), i(u16), u1(u16), u2(u16)
        All multi-byte values in little-endian (Ether Dream protocol standard)
        """
        num_points = len(points)
        # Pack header: command byte + point count (little-endian)
        header = struct.pack('<BH', CMD_DATA, num_points)

        # Pack all points
        point_data = b''
        for x, y, r, g, b in points:
            # Convert float coords [-1.0, 1.0] to int16 [-32768, 32767]
            x_int = int(x * 32767)
            y_int = int(y * 32767)

            # Convert float colors [0.0, 1.0] to uint16 [0, 65535]
            r_int = int(r * 65535)
            g_int = int(g * 65535)
            b_int = int(b * 65535)

            # Pack point: control, x, y, r, g, b, i, u1, u2
            # control=0 (normal point), i=r (use color for intensity), u1=u2=0
            # All values in little-endian
            point_data += struct.pack('<HhhHHHHHH',
                                     0,      # control (uint16)
                                     x_int,  # x (int16)
                                     y_int,  # y (int16)
                                     r_int,  # r (uint16)
                                     g_int,  # g (uint16)
                                     b_int,  # b (uint16)
                                     r_int,  # i (uint16 - intensity)
                                     0,      # u1 (uint16)
                                     0)      # u2 (uint16)

        self.sock.send(header + point_data)

        # Wait for ACK
        response = self.sock.recv(22)  # dac_response is 22 bytes
        if len(response) >= 1 and response[0] == ACK:
            return True
        else:
            print(f"Data frame failed: {response.hex()}")
            return False

    def prepare(self):
        """Send prepare command"""
        return self.send_command(CMD_PREPARE)

    def begin(self, point_rate=30000, low_water_mark=1000):
        """
        Send begin playback command with parameters
        Format: 'b' + uint16_le(low_water_mark) + uint32_le(point_rate)
        All values in little-endian (Ether Dream protocol standard)
        """
        # Pack: command + low_water_mark (uint16 LE) + point_rate (uint32 LE)
        data = struct.pack('<BHI', CMD_BEGIN, low_water_mark, point_rate)
        self.sock.send(data)

        # Wait for ACK
        response = self.sock.recv(22)  # dac_response is 22 bytes
        if len(response) >= 1 and response[0] == ACK:
            print(f"Command {chr(CMD_BEGIN)} ACKed")
            return True
        else:
            print(f"Command {chr(CMD_BEGIN)} failed: {response.hex()}")
            return False

    def stop(self):
        """Send stop command"""
        return self.send_command(CMD_STOP)

    def close(self):
        """Close connection"""
        if self.sock:
            self.sock.close()
            print("Connection closed")


def generate_square(size=0.7, num_points=400):
    """Generate points for a square"""
    points = []
    # Four sides of the square
    for i in range(num_points):
        t = i / num_points
        if t < 0.25:
            # Bottom edge (left to right)
            x = -size + (t * 4) * (2 * size)
            y = -size
        elif t < 0.5:
            # Right edge (bottom to top)
            x = size
            y = -size + ((t - 0.25) * 4) * (2 * size)
        elif t < 0.75:
            # Top edge (right to left)
            x = size - ((t - 0.5) * 4) * (2 * size)
            y = size
        else:
            # Left edge (top to bottom)
            x = -size
            y = size - ((t - 0.75) * 4) * (2 * size)

        # Color: rainbow based on position
        hue = t * 6.28  # 0 to 2π
        r = (math.sin(hue) + 1) / 2
        g = (math.sin(hue + 2.09) + 1) / 2
        b = (math.sin(hue + 4.19) + 1) / 2

        points.append((x, y, r, g, b))

    return points


def generate_circle(radius=0.7, num_points=500):
    """Generate points for a circle"""
    points = []
    for i in range(num_points):
        angle = (i / num_points) * 2 * math.pi
        x = radius * math.cos(angle)
        y = radius * math.sin(angle)

        # Color: rainbow around the circle
        r = (math.sin(angle) + 1) / 2
        g = (math.sin(angle + 2.09) + 1) / 2
        b = (math.sin(angle + 4.19) + 1) / 2

        points.append((x, y, r, g, b))

    return points


def generate_color_test(size=0.3, num_points=300):
    """Generate three separate squares - red, green, blue"""
    points = []

    # Left square - RED only
    for i in range(num_points // 3):
        t = i / (num_points // 3)
        if t < 0.25:
            local_x = -size + (t * 4) * (2 * size)
            local_y = -size
        elif t < 0.5:
            local_x = size
            local_y = -size + ((t - 0.25) * 4) * (2 * size)
        elif t < 0.75:
            local_x = size - ((t - 0.5) * 4) * (2 * size)
            local_y = size
        else:
            local_x = -size
            local_y = size - ((t - 0.75) * 4) * (2 * size)

        x = local_x - 0.5  # Shift left
        y = local_y
        points.append((x, y, 1.0, 0.0, 0.0))  # Pure RED

    # Center square - GREEN only
    for i in range(num_points // 3):
        t = i / (num_points // 3)
        if t < 0.25:
            local_x = -size + (t * 4) * (2 * size)
            local_y = -size
        elif t < 0.5:
            local_x = size
            local_y = -size + ((t - 0.25) * 4) * (2 * size)
        elif t < 0.75:
            local_x = size - ((t - 0.5) * 4) * (2 * size)
            local_y = size
        else:
            local_x = -size
            local_y = size - ((t - 0.75) * 4) * (2 * size)

        x = local_x  # Center
        y = local_y
        points.append((x, y, 0.0, 1.0, 0.0))  # Pure GREEN

    # Right square - BLUE only
    for i in range(num_points // 3):
        t = i / (num_points // 3)
        if t < 0.25:
            local_x = -size + (t * 4) * (2 * size)
            local_y = -size
        elif t < 0.5:
            local_x = size
            local_y = -size + ((t - 0.25) * 4) * (2 * size)
        elif t < 0.75:
            local_x = size - ((t - 0.5) * 4) * (2 * size)
            local_y = size
        else:
            local_x = -size
            local_y = size - ((t - 0.75) * 4) * (2 * size)

        x = local_x + 0.5  # Shift right
        y = local_y
        points.append((x, y, 0.0, 0.0, 1.0))  # Pure BLUE

    return points


def generate_rotating_square(angle, size=0.6, num_points=400):
    """Generate a rotating square"""
    points = []
    for i in range(num_points):
        t = i / num_points
        if t < 0.25:
            local_x = -size + (t * 4) * (2 * size)
            local_y = -size
        elif t < 0.5:
            local_x = size
            local_y = -size + ((t - 0.25) * 4) * (2 * size)
        elif t < 0.75:
            local_x = size - ((t - 0.5) * 4) * (2 * size)
            local_y = size
        else:
            local_x = -size
            local_y = size - ((t - 0.75) * 4) * (2 * size)

        # Rotate
        x = local_x * math.cos(angle) - local_y * math.sin(angle)
        y = local_x * math.sin(angle) + local_y * math.cos(angle)

        # Color based on angle
        r = (math.sin(angle + t * 6.28) + 1) / 2
        g = (math.sin(angle + t * 6.28 + 2.09) + 1) / 2
        b = (math.sin(angle + t * 6.28 + 4.19) + 1) / 2

        points.append((x, y, r, g, b))

    return points


def main():
    parser = argparse.ArgumentParser(description='Ether Dream Test Client')
    parser.add_argument('--host', type=str, default='localhost', help='Ether Dream bridge host (default: localhost)')
    parser.add_argument('--port', type=int, default=ETHERDREAM_PORT, help=f'Ether Dream bridge port (default: {ETHERDREAM_PORT})')
    args = parser.parse_args()

    print("="*60)
    print("Ether Dream Test Client")
    print("="*60)
    print(f"Connecting to {args.host}:{args.port}\n")

    client = EtherDreamClient(host=args.host, port=args.port)

    try:
        # Connect
        client.connect()

        # Prepare
        if not client.prepare():
            print("Failed to prepare")
            return

        time.sleep(0.1)

        # Begin playback
        if not client.begin():
            print("Failed to begin")
            return

        print("\nSending animated patterns...")
        print("Press Ctrl+C to stop")
        print("-"*60)

        # Send animated frames
        frame = 0
        start_time = time.time()

        while True:
            angle = (time.time() - start_time) * 2  # Rotate 2 rad/sec

            # Alternate between patterns every 5 seconds
            pattern_type = int((time.time() - start_time) / 5) % 4

            if pattern_type == 0:
                # Color test - pure RGB squares
                points = generate_color_test()
                pattern_name = "Color Test (R/G/B)"
            elif pattern_type == 1:
                # Rotating square
                points = generate_rotating_square(angle, size=0.6)
                pattern_name = "Rotating Square"
            elif pattern_type == 2:
                # Circle
                points = generate_circle(radius=0.7)
                pattern_name = "Circle"
            else:
                # Static square
                points = generate_square(size=0.7)
                pattern_name = "Static Square"

            # Send frame
            if client.send_data(points):
                frame += 1
                if frame % 60 == 0:
                    print(f"Frame {frame:4d} - {pattern_name} - {len(points)} points")
            else:
                print("Failed to send frame")
                break

            # Target ~60 FPS for smoother display
            time.sleep(1/60)

    except KeyboardInterrupt:
        print("\n\nStopping...")
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
    finally:
        # Stop playback
        try:
            client.stop()
        except:
            pass
        client.close()

    print("\nTest complete!")


if __name__ == "__main__":
    main()
