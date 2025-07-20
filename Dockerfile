# Use Ubuntu 22.04 as base image
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_VERSION=3.20.0
ENV OPENCV_VERSION=4.8.0
ENV BOOST_VERSION=1.82.0

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    pkg-config \
    libgtk-3-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libv4l-dev \
    libxvidcore-dev \
    libx264-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libatlas-base-dev \
    gfortran \
    libhdf5-dev \
    libhdf5-serial-dev \
    libhdf5-103 \
    libqtgui4 \
    libqtwebkit4 \
    libqt4-test \
    python3-dev \
    python3-pip \
    python3-numpy \
    libtbb2 \
    libtbb-dev \
    libdc1394-22-dev \
    libopenexr-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-tools \
    gstreamer1.0-x \
    gstreamer1.0-alsa \
    gstreamer1.0-gl \
    gstreamer1.0-gtk3 \
    gstreamer1.0-qt5 \
    gstreamer1.0-pulseaudio \
    libboost-all-dev \
    libgoogle-glog-dev \
    libgflags-dev \
    libatlas-base-dev \
    libeigen3-dev \
    libsuitesparse-dev \
    libceres-dev \
    libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

# Create application directory
WORKDIR /app

# Copy source code
COPY . .

# Create build directory
RUN mkdir -p build && cd build

# Configure and build the project
RUN cd build && \
    cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENCV_ENABLE_NONFREE=ON \
    -DBUILD_TESTS=ON \
    -DBUILD_EXAMPLES=ON && \
    make -j$(nproc)

# Create runtime directories
RUN mkdir -p /app/data /app/logs /app/saves /app/config

# Create non-root user
RUN useradd -m -s /bin/bash evosim && \
    chown -R evosim:evosim /app

# Switch to non-root user
USER evosim

# Set working directory
WORKDIR /app

# Expose ports (if needed for visualization)
EXPOSE 8080

# Set environment variables
ENV EVOSIM_DATA_DIR=/app/data
ENV EVOSIM_LOG_DIR=/app/logs
ENV EVOSIM_SAVE_DIR=/app/saves
ENV EVOSIM_CONFIG_DIR=/app/config

# Create entrypoint script
RUN echo '#!/bin/bash\n\
if [ "$1" = "interactive" ]; then\n\
    exec ./build/bin/evosim --interactive\n\
elif [ "$1" = "daemon" ]; then\n\
    exec ./build/bin/evosim --non-interactive --daemon\n\
else\n\
    exec ./build/bin/evosim "$@"\n\
fi' > /app/entrypoint.sh && \
chmod +x /app/entrypoint.sh

# Set entrypoint
ENTRYPOINT ["/app/entrypoint.sh"]

# Default command
CMD ["--help"] 