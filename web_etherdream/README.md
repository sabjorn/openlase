# Interactive Ether Dream Web Controller

Browser-based interactive drawing tool that sends laser graphics to the etherdream_bridge in real-time.

## Setup

```bash
cd web_etherdream

# Install uv if you don't have it
curl -LsSf https://astral.sh/uv/install.sh | sh

# Install dependencies (uv will auto-create .venv)
uv sync
```

## Usage

### 1. Start the Docker container with etherdream_bridge

```bash
docker run -it --rm -p 5901:5900 -p 7654:7654/udp -p 7765:7765/tcp openlase:gui etherdream_bridge
```

### 2. Open VNC viewer to see the simulator

```bash
open vnc://localhost:5901
```

Password: `openlase`

### 3. Start the web interface

```bash
# Local (Docker on same machine)
uv run app.py

# Remote (e.g., Raspberry Pi)
uv run app.py --bridge-host 192.168.1.100

# Custom ports
uv run app.py --port 3000 --bridge-host raspberrypi.local --bridge-port 7765
```

### 4. Open browser

Open http://localhost:8000 (or your custom port)

### 5. Draw!

- Click "Connect" button
- Draw on the black canvas with your mouse (click and drag)
- Watch your drawing appear in real-time in the VNC simulator
- Choose different colors with the color picker
- Click "Clear Canvas" to start over

## Features

- Real-time drawing transmission
- Color picker for RGB control
- Auto-batching of points for efficient transmission
- Visual feedback showing connection status and point count
- Compatible with the Ether Dream protocol standard

## How it works

1. You draw on an HTML5 canvas
2. Canvas coordinates are converted to laser coordinates ([-1, 1] range)
3. Points are batched and sent via HTTP POST to Flask
4. Flask sends them to etherdream_bridge via TCP (port 7765)
5. etherdream_bridge renders them through OpenLase
6. Simulator displays the result via VNC
