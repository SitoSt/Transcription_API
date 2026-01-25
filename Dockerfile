# Build Stage
FROM nvidia/cuda:12.3.1-devel-ubuntu22.04 AS builder

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
# Note: cuda-devel image includes nvcc, but we need standard build tools and app dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the application with CUDA support
# GGML_CUDA=1 enables CUDA backend in whisper.cpp
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DGGML_CUDA=1 && \
    cmake --build build -j$(nproc)

# Runtime Stage
FROM nvidia/cuda:12.3.1-runtime-ubuntu22.04 AS runtime

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-atomic1.74.0 \
    libboost-chrono1.74.0 \
    libboost-date-time1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-program-options1.74.0 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy artifacts from builder
COPY --from=builder /app/build/transcription_server /app/transcription_server
COPY --from=builder /app/generate_certs.sh /app/generate_certs.sh

# Create directory for models
RUN mkdir -p /app/models

# Expose the server port
EXPOSE 9001

# Entrypoint
CMD ["./transcription_server", "--bind", "0.0.0.0", "--port", "9001", "--model", "/app/models/ggml-base.bin"]
