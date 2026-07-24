#!/usr/bin/env python3
"""
Interactive web interface for Ether Dream laser control
Draws on HTML canvas and streams to etherdream_bridge in real-time
"""

from flask import Flask, render_template, request, jsonify
import socket
import struct
import threading
import time
import argparse

app = Flask(__name__)

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
        self.connected = False
        self.lock = threading.Lock()

    def connect(self):
        """Connect to the Ether Dream DAC"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            # Read initial status response (per Ether Dream protocol spec)
            initial_response = self.sock.recv(22)
            if len(initial_response) != 22:
                print(f"Warning: unexpected initial response length: {len(initial_response)}")
            self.connected = True
            print(f"Connected to {self.host}:{self.port} (received initial status)")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            self.connected = False
            return False

    def send_command(self, command):
        """Send a command and wait for ACK"""
        if not self.connected:
            return False

        try:
            self.sock.send(bytes([command]))
            response = self.sock.recv(22)  # dac_response is 22 bytes (response + command + 22-byte status)
            if len(response) >= 1 and response[0] == ACK:
                return True
            else:
                print(f"Command {chr(command)} failed: {response.hex()}")
                return False
        except Exception as e:
            print(f"Send command error: {e}")
            self.connected = False
            return False

    def send_data(self, points):
        """Send a data frame with points"""
        if not self.connected or not points:
            return False

        try:
            with self.lock:
                num_points = len(points)
                # Pack header: command byte + point count (little-endian)
                header = struct.pack('<BH', CMD_DATA, num_points)

                # Pack all points
                point_data = b''
                for x, y, r, g, b, blank in points:
                    # Convert float coords [-1.0, 1.0] to int16 [-32768, 32767]
                    x_int = int(max(-1.0, min(1.0, x)) * 32767)
                    y_int = int(max(-1.0, min(1.0, y)) * 32767)

                    # Convert float colors [0.0, 1.0] to uint16 [0, 65535]
                    r_int = int(max(0.0, min(1.0, r)) * 65535)
                    g_int = int(max(0.0, min(1.0, g)) * 65535)
                    b_int = int(max(0.0, min(1.0, b)) * 65535)

                    # Set control byte: bit 0 = blanking bit
                    control = 0x01 if blank else 0x00

                    # Pack point: control, x, y, r, g, b, i, u1, u2
                    # All values in little-endian (Ether Dream protocol standard)
                    point_data += struct.pack('<HhhHHHHHH',
                                             control,  # control (blanking bit)
                                             x_int,    # x
                                             y_int,    # y
                                             r_int,    # r
                                             g_int,    # g
                                             b_int,    # b
                                             r_int,    # i (intensity)
                                             0,        # u1
                                             0)        # u2

                self.sock.send(header + point_data)

                # Wait for ACK
                response = self.sock.recv(22)  # dac_response is 22 bytes
                if len(response) >= 1 and response[0] == ACK:
                    return True
                else:
                    print(f"Data frame failed: {response.hex()}")
                    return False
        except Exception as e:
            print(f"Send data error: {e}")
            self.connected = False
            return False

    def prepare(self):
        """Send prepare command"""
        return self.send_command(CMD_PREPARE)

    def begin(self, point_rate=30000, low_water_mark=1000):
        """Send begin playback command"""
        if not self.connected:
            return False

        try:
            # All values in little-endian (Ether Dream protocol standard)
            data = struct.pack('<BHI', CMD_BEGIN, low_water_mark, point_rate)
            self.sock.send(data)
            response = self.sock.recv(22)  # dac_response is 22 bytes
            if len(response) >= 1 and response[0] == ACK:
                print(f"Playback started")
                return True
            else:
                print(f"Begin failed: {response.hex()}")
                return False
        except Exception as e:
            print(f"Begin error: {e}")
            self.connected = False
            return False

    def stop(self):
        """Send stop command"""
        return self.send_command(CMD_STOP)

    def close(self):
        """Close connection"""
        if self.sock:
            try:
                self.stop()
            except:
                pass
            self.sock.close()
            self.connected = False
            print("Connection closed")

# Global client instance
client = EtherDreamClient()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/connect', methods=['POST'])
def connect():
    """Connect to Ether Dream bridge"""
    if client.connect():
        if client.prepare():
            time.sleep(0.1)
            if client.begin():
                return jsonify({'status': 'success', 'message': 'Connected and ready'})
    return jsonify({'status': 'error', 'message': 'Connection failed'}), 500

@app.route('/disconnect', methods=['POST'])
def disconnect():
    """Disconnect from Ether Dream bridge"""
    client.close()
    return jsonify({'status': 'success', 'message': 'Disconnected'})

@app.route('/send_points', methods=['POST'])
def send_points():
    """Receive points from browser and send to Ether Dream"""
    data = request.json
    points = data.get('points', [])

    if not points:
        return jsonify({'status': 'error', 'message': 'No points provided'}), 400

    # Convert from browser format [{x, y, r, g, b, blank}, ...] to tuple list
    point_tuples = [(p['x'], p['y'], p['r'], p['g'], p['b'], p.get('blank', False)) for p in points]

    if client.send_data(point_tuples):
        return jsonify({'status': 'success', 'points_sent': len(point_tuples)})
    else:
        return jsonify({'status': 'error', 'message': 'Failed to send points'}), 500

@app.route('/status', methods=['GET'])
def status():
    """Get connection status"""
    return jsonify({
        'connected': client.connected,
        'host': client.host,
        'port': client.port
    })

def main():
    parser = argparse.ArgumentParser(description='Interactive Ether Dream Web Interface')
    parser.add_argument('--port', type=int, default=8000, help='Port to run the web server on (default: 8000)')
    parser.add_argument('--host', type=str, default='0.0.0.0', help='Host to bind to (default: 0.0.0.0)')
    parser.add_argument('--bridge-host', type=str, default='localhost', help='Ether Dream bridge host (default: localhost)')
    parser.add_argument('--bridge-port', type=int, default=7765, help='Ether Dream bridge port (default: 7765)')
    args = parser.parse_args()

    # Configure the global client
    client.host = args.bridge_host
    client.port = args.bridge_port

    print("="*60)
    print("Interactive Ether Dream Web Interface")
    print("="*60)
    print(f"\nStarting Flask server on http://localhost:{args.port}")
    print(f"Will connect to etherdream_bridge at {args.bridge_host}:{args.bridge_port}")
    print("\nMake sure etherdream_bridge is running on the target host")
    print(f"\nThen open http://localhost:{args.port} in your browser\n")

    try:
        app.run(debug=True, host=args.host, port=args.port)
    finally:
        client.close()

if __name__ == '__main__':
    main()
