# Multi-architecture Dockerfile for Taler cryptocurrency wallet daemon
# Supports: linux/amd64, linux/arm64
# Build without GUI for optimized container size

FROM ubuntu:22.04 AS builder

# Set timezone non-interactively
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install build dependencies (same as GitHub Actions)
RUN apt-get update && apt-get install -y \
    git \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    python3 \
    libssl-dev \
    libevent-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-chrono-dev \
    libboost-test-dev \
    libboost-thread-dev \
    libdb-dev \
    libdb++-dev \
    qtbase5-dev \
    qttools5-dev \
    qttools5-dev-tools \
    protobuf-compiler \
    libprotobuf-dev \
    libqrencode-dev \
    libzmq3-dev \
    && rm -rf /var/lib/apt/lists/*

# Clone source from git and checkout latest tag
WORKDIR /taler
RUN git clone https://github.com/abkvme/taler.git . && \
    git fetch --tags && \
    git checkout $(git describe --tags $(git rev-list --tags --max-count=1))

# Build Taler (exactly like GitHub Actions)
RUN ./autogen.sh && \
    ./configure \
        --with-incompatible-bdb \
        --with-gui \
        CXXFLAGS="-O2" && \
    make -j$(nproc)

# Runtime stage - minimal image
FROM ubuntu:22.04

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libboost-chrono1.74.0 \
    libssl3 \
    libevent-2.1-7 \
    libevent-pthreads-2.1-7 \
    libdb5.3++ \
    libzmq5 \
    libqrencode4 \
    && rm -rf /var/lib/apt/lists/*

# Copy binaries from builder (from src/ not /usr/local/bin)
COPY --from=builder /taler/src/talerd /usr/local/bin/
COPY --from=builder /taler/src/taler-cli /usr/local/bin/
COPY --from=builder /taler/src/taler-tx /usr/local/bin/

# Copy entrypoint script from builder (where git repo was cloned)
COPY --from=builder /taler/docker/entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/entrypoint.sh

# Create taler user and data directory
RUN useradd -r -m -u 1000 taler && \
    mkdir -p /data && \
    chown -R taler:taler /data

# Switch to taler user
USER taler

# Expose P2P and RPC ports
EXPOSE 23153 23333

# Set data directory as volume
VOLUME ["/data"]

# Set working directory
WORKDIR /data

# Entrypoint
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

# Default command
CMD ["talerd"]
