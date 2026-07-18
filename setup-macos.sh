#!/bin/bash
# OpenLase macOS Setup Script
# Installs dependencies and builds OpenLase with simulator support

set -e

echo "======================================"
echo "OpenLase macOS Setup"
echo "======================================"
echo ""

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "Error: Homebrew is not installed."
    echo "Install it from: https://brew.sh"
    exit 1
fi

echo "Step 1/4: Installing Homebrew dependencies..."
echo "--------------------------------------"
brew install \
    jack \
    freeglut \
    cmake \
    yasm \
    qjackctl

echo ""
echo "Step 2/4: Checking for XQuartz..."
echo "--------------------------------------"
if ! command -v xquartz &> /dev/null && ! [ -d /Applications/Utilities/XQuartz.app ]; then
    echo "XQuartz not found. Installing via Homebrew cask..."
    brew install --cask xquartz
    echo ""
    echo "⚠️  IMPORTANT: XQuartz has been installed."
    echo "    You need to log out and log back in (or restart) for XQuartz to work properly."
    echo "    After logging back in, run this script again to continue."
    exit 0
else
    echo "✓ XQuartz is installed"
fi

# Check if XQuartz is running
if ! pgrep -i xquartz > /dev/null; then
    echo "Starting XQuartz..."
    open -a XQuartz
    sleep 3
fi

echo ""
echo "Step 3/4: Building OpenLase..."
echo "--------------------------------------"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_TRACER=OFF \
    ..

# Build
echo "Building OpenLase library and tools..."
make -j$(sysctl -n hw.ncpu)

echo ""
echo "Step 4/4: Building examples..."
echo "--------------------------------------"
make -C ../build examples

echo ""
echo "======================================"
echo "✓ Setup Complete!"
echo "======================================"
echo ""
echo "Built executables:"
echo "  Simulator:  ./build/tools/simulator"
echo "  Examples:   ./build/examples/{simple,circlescope,scope,harp,multihead}"
echo ""
echo "To test the setup:"
echo "  1. Start JACK server:"
echo "     jackd -d dummy -r 48000 &"
echo ""
echo "  2. Start the simulator:"
echo "     cd build && ./tools/simulator &"
echo ""
echo "  3. Run an example:"
echo "     cd build && ./examples/simple &"
echo ""
echo "  4. Connect JACK ports using QjackCtl:"
echo "     qjackctl"
echo "     Click 'Connect' or 'Graph' and connect:"
echo "       simple:out_x -> simulator:in_x"
echo "       simple:out_y -> simulator:in_y"
echo "       simple:out_r -> simulator:in_r"
echo "       simple:out_g -> simulator:in_g"
echo "       simple:out_b -> simulator:in_b"
echo ""
echo "Or use the convenience script:"
echo "  ./run-simulator-macos.sh"
echo ""
