#!/bin/bash
# Docker entrypoint for OpenLase with VNC and JACK

set -e

APP="${1:-help}"

# Start services
start_services() {
    echo "Starting Xvfb..."
    Xvfb :99 -screen 0 1024x768x24 &
    export DISPLAY=:99
    sleep 2

    echo "Starting x11vnc on port 5900..."
    mkdir -p /root/.vnc
    x11vnc -storepasswd openlase /root/.vnc/passwd 2>/dev/null
    x11vnc -display :99 -rfbauth /root/.vnc/passwd -listen 0.0.0.0 -forever -bg -xkb > /dev/null 2>&1
    sleep 2

    echo "Starting JACK server (dummy mode)..."
    jackd -d dummy -r 48000 > /dev/null 2>&1 &
    sleep 3

    echo "✓ Services ready"
    echo ""
}

# Connect JACK ports
connect_jack() {
    local source="$1"
    local dest="$2"

    echo "Connecting JACK: $source → $dest..."
    sleep 2  # Wait for ports to register

    jack_connect "${source}:out_x" "${dest}:in_x" 2>/dev/null || true
    jack_connect "${source}:out_y" "${dest}:in_y" 2>/dev/null || true
    jack_connect "${source}:out_r" "${dest}:in_r" 2>/dev/null || true
    jack_connect "${source}:out_g" "${dest}:in_g" 2>/dev/null || true
    jack_connect "${source}:out_b" "${dest}:in_b" 2>/dev/null || true

    echo "✓ JACK connected"
}

# Find executable in tools or examples
find_executable() {
    local name="$1"

    if [ -x "./tools/$name" ]; then
        echo "./tools/$name"
        return 0
    elif [ -x "./examples/$name" ]; then
        echo "./examples/$name"
        return 0
    fi

    return 1
}

case "$APP" in
    help|--help|-h)
        echo "OpenLase Docker Container"
        echo ""
        echo "Usage: docker run [options] openlase:gui <command>"
        echo ""
        echo "Special commands:"
        echo "  simulator      - Just the simulator"
        echo ""
        echo "Available examples (auto-connected to simulator):"
        for example in ./examples/*; do
            [ -x "$example" ] && [ -f "$example" ] && echo "  $(basename $example)"
        done
        echo ""
        echo "Available tools:"
        for tool in ./tools/*; do
            [ -x "$tool" ] && [ -f "$tool" ] && echo "  $(basename $tool)"
        done
        echo ""
        echo "Examples:"
        echo "  # Run simple example"
        echo "  docker run -it --rm -p 5901:5900 openlase:gui simple"
        echo ""
        echo "  # Just the simulator"
        echo "  docker run -it --rm -p 5901:5900 openlase:gui simulator"
        echo ""
        echo "Connect via VNC to localhost:5901 (password: openlase)"
        exit 0
        ;;

    simulator)
        echo "======================================"
        echo "Simulator"
        echo "======================================"
        echo ""
        start_services

        echo "Starting simulator..."
        echo ""
        echo "======================================"
        echo "✓ Ready!"
        echo "======================================"
        echo "  VNC: localhost:5901 (password: openlase)"
        echo ""

        exec ./tools/simulator
        ;;

    *)
        # Try to find the executable
        EXECUTABLE=$(find_executable "$APP")

        if [ $? -eq 0 ]; then
            # Found it - determine if it's in examples (needs simulator) or tools
            if [[ "$EXECUTABLE" == ./examples/* ]]; then
                echo "======================================"
                echo "Example: $APP + Simulator"
                echo "======================================"
                echo ""
                start_services

                echo "Starting simulator..."
                ./tools/simulator &

                echo "Starting example: $APP..."
                $EXECUTABLE &
                APP_PID=$!

                connect_jack "libol" "simulator"

                echo ""
                echo "======================================"
                echo "✓ Ready!"
                echo "======================================"
                echo "  VNC: localhost:5901 (password: openlase)"
                echo ""

                wait $APP_PID
            else
                # It's a tool - just run it
                echo "======================================"
                echo "Tool: $APP"
                echo "======================================"
                echo ""
                start_services

                echo "Starting $APP..."
                echo ""
                echo "======================================"
                echo "✓ Ready!"
                echo "======================================"
                echo "  VNC: localhost:5901 (password: openlase)"
                echo ""

                exec $EXECUTABLE
            fi
        else
            echo "Error: Unknown command or executable '$APP'"
            echo ""
            echo "Run with 'help' to see available commands"
            exit 1
        fi
        ;;
esac
