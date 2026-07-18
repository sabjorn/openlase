#!/bin/bash
# OpenLase Simulator Quick Start for macOS
# Starts JACK, simulator, and optionally an example with auto-connection

set -e

EXAMPLE="${1:-simple}"

echo "======================================"
echo "Starting OpenLase Simulator"
echo "======================================"
echo ""

# Check if XQuartz is running
if ! pgrep -i xquartz > /dev/null; then
    echo "Starting XQuartz..."
    open -a XQuartz
    sleep 3
fi

# Check if JACK is running
if ! pgrep jackd > /dev/null; then
    echo "Starting JACK server..."
    jackd -d dummy -r 48000 &
    JACK_PID=$!
    sleep 2
    echo "✓ JACK started (PID: $JACK_PID)"
else
    echo "✓ JACK is already running"
fi

# Start the simulator
if ! pgrep simulator > /dev/null; then
    echo "Starting simulator..."
    cd build
    export DISPLAY=:0
    ./tools/simulator &
    SIMULATOR_PID=$!
    sleep 2
    echo "✓ Simulator started (PID: $SIMULATOR_PID)"
    cd ..
else
    echo "✓ Simulator is already running"
fi

# Ask if user wants to run an example
echo ""
echo "Would you like to run an example? (Default: $EXAMPLE)"
echo "Available examples: simple, circlescope, scope, harp, multihead"
echo ""
read -p "Run '$EXAMPLE'? [Y/n] " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    if [ -f "build/examples/$EXAMPLE" ]; then
        echo "Starting example: $EXAMPLE"
        cd build/examples
        ./$EXAMPLE &
        EXAMPLE_PID=$!
        sleep 2
        echo "✓ Example started (PID: $EXAMPLE_PID)"
        cd ../..

        echo ""
        echo "======================================"
        echo "⚠️  IMPORTANT: Connect JACK Ports"
        echo "======================================"
        echo ""
        echo "The example is running but NOT connected to the simulator."
        echo "You need to connect the JACK ports:"
        echo ""
        echo "Option 1 - Use QjackCtl GUI:"
        echo "  1. Run: qjackctl &"
        echo "  2. Click the 'Connect' or 'Graph' button"
        echo "  3. Connect:"
        echo "       $EXAMPLE:out_x -> simulator:in_x"
        echo "       $EXAMPLE:out_y -> simulator:in_y"
        echo "       $EXAMPLE:out_r -> simulator:in_r"
        echo "       $EXAMPLE:out_g -> simulator:in_g"
        echo "       $EXAMPLE:out_b -> simulator:in_b"
        echo ""
        echo "Option 2 - Launch QjackCtl now:"
        read -p "Launch QjackCtl? [Y/n] " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            qjackctl &
            echo "✓ QjackCtl launched - use the Connect button to wire up the ports"
        fi
    else
        echo "Error: Example '$EXAMPLE' not found in build/examples/"
        echo "Available examples:"
        ls -1 build/examples/ 2>/dev/null | grep -v "\.c\|CMake\|Makefile" || echo "  (none built yet)"
        exit 1
    fi
fi

echo ""
echo "======================================"
echo "✓ Setup Complete"
echo "======================================"
echo ""
echo "Running processes:"
pgrep jackd > /dev/null && echo "  - JACK server (PID: $(pgrep jackd))"
pgrep simulator > /dev/null && echo "  - Simulator (PID: $(pgrep simulator))"
pgrep "$EXAMPLE" > /dev/null && echo "  - Example '$EXAMPLE' (PID: $(pgrep $EXAMPLE))"
echo ""
echo "To stop everything:"
echo "  killall jackd simulator $EXAMPLE"
echo ""
