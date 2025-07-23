# Stage 1: The Builder
# This stage has all the build tools and compiles the application.
FROM ubuntu:24.04 AS builder

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    clang \
    libopencv-dev \
    libboost-all-dev \
    libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

# Set up the build directory
WORKDIR /app

# Copy build configuration first to leverage Docker cache
COPY CMakeLists.txt ./ 
# Copy source code directories. Separating these improves caching.
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/
COPY examples/ ./examples/

# Configure and build the project
RUN cmake -B build . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DOPENCV_ENABLE_NONFREE=ON \
    -DBUILD_TESTS=ON \
    -DBUILD_EXAMPLES=ON
RUN cmake --build build -j$(nproc)

# Use the install rules defined in CMakeLists.txt to create a clean
# distribution package in /app/dist. This is the idiomatic approach.
RUN cmake --install build --prefix /app/dist

# Now, find the external shared libraries required by the installed executable
# and copy them into the distribution's lib directory.
RUN mkdir -p /app/dist/lib && \
    ldd /app/dist/bin/evosim | grep '=> /' | awk '{print $3}' | \
    xargs -I '{}' cp -v -L '{}' /app/dist/lib/

# Stage 2: The Final Image
# This is the minimal, secure image for end-users.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Copy the final executable and its bundled libraries from the builder stage.
# The destination `/usr/local` is standard for user-installed software.
COPY --from=builder /app/dist/ /usr/local/

# Update the dynamic linker cache to recognize the newly copied libraries.
RUN ldconfig

# Create non-root user and application directories.
RUN useradd -m -s /bin/bash evosim && \
    mkdir -p /app/data /app/logs /app/saves /app/config && \
    chown -R evosim:evosim /app

# Switch to non-root user for runtime.
USER evosim

WORKDIR /app

# Set entrypoint
ENTRYPOINT ["evosim"]

# Default command
CMD ["--help"] 
