# ============================================================================
# Rats Search v2 — C++/Qt6 Docker Build
# ============================================================================

# === Build Stage ===
FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    qt6-base-dev \
    qt6-websockets-dev \
    qt6-l10n-tools \
    libgl1-mesa-dev \
    libxkbcommon-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Build the project (console-only, no tests)
RUN cmake -B build -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DRATS_SEARCH_USE_SYSTEM_LIBRATS=OFF \
    -DRATS_SEARCH_BUILD_TESTS=OFF

RUN cmake --build build --config Release --parallel

# === Runtime Stage ===
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# Install only the runtime libraries needed
RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt6core6t64 \
    libqt6gui6t64 \
    libqt6widgets6t64 \
    libqt6network6t64 \
    libqt6sql6t64 \
    libqt6sql6-mysql \
    libqt6concurrent6t64 \
    libqt6websockets6t64 \
    libgl1 \
    libxkbcommon0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built binary
COPY --from=builder /src/build/bin/RatsSearch /app/

# Copy Manticore search engine binaries
COPY imports/linux/x64/searchd  /app/
COPY imports/linux/x64/indexer  /app/
COPY imports/linux/x64/indextool /app/

RUN chmod +x /app/RatsSearch /app/searchd /app/indexer /app/indextool

# Persistent data directory for database, config, and logs
VOLUME /data

# Default HTTP API port
EXPOSE 8095

# Run in console mode with spider enabled and 30 max peers
CMD ["/app/RatsSearch", "--console", "--spider", "--max-peers", "30", "--data-dir", "/data"]
