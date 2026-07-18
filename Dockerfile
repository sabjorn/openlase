FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libjack-jackd2-dev \
    jackd2 \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libfftw3-dev \
    libswscale-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libgl1-mesa-dri \
    libgl1-mesa-glx \
    freeglut3-dev \
    libx11-dev \
    libxext-dev \
    mesa-utils \
    x11-apps \
    xvfb \
    x11vnc \
    python3 \
    python3-dev \
    python3-pip \
    yasm \
    && rm -rf /var/lib/apt/lists/*

# Set environment for X11 and software rendering
ENV DISPLAY=:0
ENV LIBGL_ALWAYS_SOFTWARE=1

WORKDIR /openlase

COPY . .

RUN mkdir -p build && cd build && \
    cmake -DBUILD_TRACER=OFF .. && \
    make -j$(nproc)

# Make entrypoint script executable
RUN chmod +x /openlase/docker-entrypoint.sh

WORKDIR /openlase/build

ENTRYPOINT ["/openlase/docker-entrypoint.sh"]
