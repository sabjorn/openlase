# Running OpenLase GUI Applications in Docker

This guide explains how to run OpenLase GUI applications (simulator, spectrogram3d) from Docker with X11 forwarding.

## Prerequisites

### macOS
1. Install XQuartz:
   ```bash
   brew install --cask xquartz
   ```

2. Start XQuartz:
   ```bash
   open -a XQuartz
   ```

3. Configure XQuartz:
   - Open XQuartz Preferences (XQuartz → Preferences)
   - Go to the "Security" tab
   - Enable "Allow connections from network clients"
   - Restart XQuartz

### Linux
No additional software needed - X11 is typically already installed.

## Building the Docker Image

Build the Docker image with GUI support:

```bash
docker build -t openlase:gui .
```

This builds OpenLase with:
- OpenGL and GLUT support (for simulator)
- FFTW3 support (for spectrogram3d)
- X11 libraries for display forwarding

## Running GUI Applications

### Quick Start (Recommended)

Use the provided helper scripts:

```bash
# Run the laser simulator
./run-simulator.sh

# Run the 3D spectrogram (with custom options)
./run-spectrogram3d.sh -f 1024 -d 64
```

### Manual Execution

#### macOS

```bash
# Get your IP address
IP=$(ifconfig en0 | grep inet | awk '$1=="inet" {print $2}')

# Allow X11 forwarding
xhost + $IP

# Run simulator
docker run --rm -it \
    -e DISPLAY=$IP:0 \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    openlase:gui ./tools/simulator

# Run spectrogram3d
docker run --rm -it \
    -e DISPLAY=$IP:0 \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    openlase:gui ./tools/spectrogram3d
```

#### Linux

```bash
# Allow docker to connect to X server
xhost +local:docker

# Run simulator
docker run --rm -it \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    --network host \
    openlase:gui ./tools/simulator

# Run spectrogram3d
docker run --rm -it \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    --network host \
    openlase:gui ./tools/spectrogram3d
```

## Connecting Audio (JACK)

The simulator and other tools use JACK for audio I/O. To connect audio:

1. **Inside the container**, JACK will try to connect to a running JACK server

2. **On the host**, run a JACK server:
   ```bash
   # macOS
   brew install jack
   jackd -d coreaudio

   # Linux
   jackd -d alsa
   ```

3. Use `jack_connect` or a GUI tool like `qjackctl` to connect audio ports:
   ```bash
   # Example: Connect system audio to scope input
   jack_connect system:capture_1 scope:in_l
   jack_connect system:capture_2 scope:in_r

   # Connect scope output to simulator
   jack_connect scope:out_x simulator:in_x
   jack_connect scope:out_y simulator:in_y
   ```

## Available GUI Tools

### Simulator
Displays laser output in a OpenGL window:
```bash
./run-simulator.sh
```

### Spectrogram3D
3D waterfall spectrogram visualization:
```bash
# Default settings
./run-spectrogram3d.sh

# Custom FFT size and depth
./run-spectrogram3d.sh -f 1024 -d 64

# Custom rotation (top-down view)
./run-spectrogram3d.sh -x -90 -y 0 -z 0

# Hide axes
./run-spectrogram3d.sh -a
```

## Troubleshooting

### macOS: "cannot open display"
- Ensure XQuartz is running: `pgrep XQuartz`
- Check XQuartz Security preferences allow network clients
- Verify xhost: `xhost`
- Try `xhost +localhost` or `xhost +127.0.0.1`

### Linux: "No protocol specified"
- Run: `xhost +local:docker`
- Check DISPLAY variable: `echo $DISPLAY`

### JACK connection refused
- Ensure JACK server is running on the host
- JACK communication between host and container may require:
  - Shared `/tmp` or JACK socket directory
  - Network mode or specific port forwarding
  - Consider running JACK inside the container for full isolation

### Black screen or no output
- Check that the OpenLase tool is producing output
- Verify JACK connections are established
- Use `jack_lsp` to list available ports
- Use `qjackctl` to visualize and manage connections

## Examples

### Complete Pipeline: Audio → Spectrogram → Simulator

Terminal 1 (JACK server):
```bash
jackd -d coreaudio  # or -d alsa on Linux
```

Terminal 2 (Spectrogram):
```bash
./run-spectrogram3d.sh -f 512 -d 32
```

Terminal 3 (Simulator):
```bash
./run-simulator.sh
```

Terminal 4 (Connect audio):
```bash
# Connect microphone to spectrogram
jack_connect system:capture_1 spectrogram3d:in_l
jack_connect system:capture_2 spectrogram3d:in_r

# Connect spectrogram output to simulator (if using libol rendering)
# Note: spectrogram3d uses olRenderFrame which outputs via JACK
```

## Notes

- The Docker image includes both headless and GUI builds
- X11 forwarding may have performance overhead for complex graphics
- For production use, consider native builds or dedicated rendering solutions
- Platform mismatch warnings (linux/amd64 vs arm64) are expected on Apple Silicon Macs
