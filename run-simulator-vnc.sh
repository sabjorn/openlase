#!/bin/bash
# Script to run OpenLase simulator with Xvfb and VNC in Docker

set -e

EXAMPLE="${1:-simple}"

echo "======================================"
echo "OpenLase Docker VNC Simulator"
echo "======================================"
echo ""

# Check if container is already running
if docker ps --format '{{.Names}}' | grep -q '^openlase-vnc$'; then
    echo "Container 'openlase-vnc' is already running"
    echo ""
    echo "To stop it: docker stop openlase-vnc"
    echo "To attach to it: docker exec -it openlase-vnc /bin/bash"
    echo ""
    exit 0
fi

# Remove old container if it exists
docker rm -f openlase-vnc 2>/dev/null || true

echo "Starting Docker container with VNC..."
echo ""

docker run -d \
    --name openlase-vnc \
    -p 5901:5900 \
    openlase:gui

# Wait for services to start
echo "Waiting for services to start..."
sleep 5

# Check if container is still running
if ! docker ps --format '{{.Names}}' | grep -q '^openlase-vnc$'; then
    echo "Error: Container failed to start"
    echo ""
    echo "Container logs:"
    docker logs openlase-vnc
    exit 1
fi

echo ""
echo "======================================"
echo "✓ Container Started"
echo "======================================"
echo ""
echo "VNC Connection:"
echo "  Address: localhost:5901"
echo "  Password: (none)"
echo ""
echo "Connect now with: open vnc://localhost:5901"
echo ""

# Ask if user wants to open VNC now
read -p "Open VNC viewer now? [Y/n] " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    open vnc://localhost:5901 || echo "Please open VNC manually to: vnc://localhost:5901"
    sleep 2
fi

# Start simulator in the container
echo ""
echo "Starting simulator in container..."
docker exec -d openlase-vnc ./tools/simulator

sleep 2

# Ask if user wants to run an example
echo ""
echo "Would you like to run an example? (Default: $EXAMPLE)"
echo "Available: simple, circlescope, scope, harp, multihead"
echo ""
read -p "Run '$EXAMPLE'? [Y/n] " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    echo "Starting example: $EXAMPLE"
    docker exec -d openlase-vnc ./examples/$EXAMPLE
    sleep 2

    echo ""
    echo "⚠️  Note: You still need to connect JACK ports inside the container"
    echo ""
    echo "To connect ports, run in another terminal:"
    echo "  docker exec -it openlase-vnc /bin/bash"
    echo ""
    echo "Then inside the container, install jack tools and connect:"
    echo "  apt-get update && apt-get install -y jack-tools"
    echo "  jack_connect $EXAMPLE:out_x simulator:in_x"
    echo "  jack_connect $EXAMPLE:out_y simulator:in_y"
    echo "  jack_connect $EXAMPLE:out_r simulator:in_r"
    echo "  jack_connect $EXAMPLE:out_g simulator:in_g"
    echo "  jack_connect $EXAMPLE:out_b simulator:in_b"
fi

echo ""
echo "======================================"
echo "Container Running"
echo "======================================"
echo ""
echo "Container name: openlase-vnc"
echo "VNC port: localhost:5901"
echo ""
echo "Useful commands:"
echo "  View logs:    docker logs openlase-vnc"
echo "  Shell access: docker exec -it openlase-vnc /bin/bash"
echo "  Stop:         docker stop openlase-vnc"
echo "  Remove:       docker rm openlase-vnc"
echo ""
