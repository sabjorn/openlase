FROM --platform=linux/amd64 ubuntu:22.04

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
    freeglut3-dev \
    libx11-dev \
    libxext-dev \
    mesa-utils \
    x11-apps \
    python3 \
    python3-dev \
    python3-pip \
    yasm \
    && rm -rf /var/lib/apt/lists/*

# Set environment for X11
ENV DISPLAY=:0

WORKDIR /openlase

COPY . .

RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

WORKDIR /openlase/build

CMD ["/bin/bash"]
