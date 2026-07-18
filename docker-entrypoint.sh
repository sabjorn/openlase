#!/bin/bash
# Docker entrypoint for OpenLase with VNC and JACK

set -e

echo "======================================"
echo "OpenLase Docker Container"
echo "======================================"
echo ""

# Start virtual X server
echo "Starting Xvfb..."
Xvfb :99 -screen 0 1024x768x24 &
export DISPLAY=:99
sleep 2

# Start VNC server
echo "Starting x11vnc on port 5900..."
# Create password file (password: openlase)
mkdir -p /root/.vnc
x11vnc -storepasswd openlase /root/.vnc/passwd
x11vnc -display :99 -rfbauth /root/.vnc/passwd -listen 0.0.0.0 -forever -bg -xkb
sleep 2

# Start JACK in dummy mode
echo "Starting JACK server (dummy mode)..."
jackd -d dummy -r 48000 &
JACK_PID=$!
sleep 3

# Check if JACK started successfully
if ! ps -p $JACK_PID > /dev/null; then
    echo "Error: JACK failed to start"
    exit 1
fi

echo ""
echo "======================================"
echo "✓ Services Started"
echo "======================================"
echo ""
echo "  VNC Server: localhost:5900 (or host port 5901)"
echo "  VNC Password: openlase"
echo "  JACK Server: running (dummy mode, 48kHz)"
echo "  Display: :99"
echo ""
echo "Connect with VNC client to see the display"
echo ""

# Run the command passed to docker run, or keep container alive
if [ $# -eq 0 ]; then
    echo "Container ready. Connect via VNC to start using the simulator."
    echo ""
    echo "Available commands (run via 'docker exec'):"
    echo "  ./tools/simulator     - Start the simulator"
    echo "  ./examples/simple     - Run simple example"
    echo "  ./examples/circlescope - Run circlescope example"
    echo ""
    # Keep container running indefinitely
    tail -f /dev/null
else
    exec "$@"
fi
