#!/bin/bash
# Script to run OpenLase simulator with X11 forwarding
# Supports both macOS (with XQuartz) and Linux

set -e

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS - requires XQuartz
    echo "Running on macOS..."

    # Check if XQuartz is running
    if ! pgrep -x "XQuartz" > /dev/null; then
        echo "XQuartz is not running. Please install and start XQuartz:"
        echo "  brew install --cask xquartz"
        echo "  open -a XQuartz"
        echo ""
        echo "After starting XQuartz, in XQuartz preferences:"
        echo "  Security tab -> Enable 'Allow connections from network clients'"
        exit 1
    fi

    # Get IP address
    IP=$(ifconfig en0 | grep inet | awk '$1=="inet" {print $2}')
    if [ -z "$IP" ]; then
        IP="host.docker.internal"
    fi

    # Allow X11 forwarding
    xhost + "$IP" > /dev/null 2>&1 || true

    echo "Starting simulator with DISPLAY=$IP:0"
    docker run --rm -it \
        -e DISPLAY="$IP:0" \
        -v /tmp/.X11-unix:/tmp/.X11-unix \
        openlase:gui ./tools/simulator "$@"

elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Linux
    echo "Running on Linux..."

    # Allow local docker to connect
    xhost +local:docker > /dev/null 2>&1 || true

    echo "Starting simulator with DISPLAY=$DISPLAY"
    docker run --rm -it \
        -e DISPLAY="$DISPLAY" \
        -v /tmp/.X11-unix:/tmp/.X11-unix \
        --network host \
        openlase:gui ./tools/simulator "$@"
else
    echo "Unsupported OS: $OSTYPE"
    exit 1
fi
