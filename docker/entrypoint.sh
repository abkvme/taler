#!/bin/bash
set -e

# Taler Docker Entrypoint Script

# Set default data directory
TALER_DATA=${TALER_DATA:-/data}

# Default config file location (separate from data)
TALER_CONF=${TALER_CONF:-/taler.conf}

# Initialize data directory if needed
if [ ! -d "$TALER_DATA" ]; then
    echo "Creating data directory at $TALER_DATA"
    mkdir -p "$TALER_DATA"
fi

# Print startup message
echo "========================================="
echo "  Starting Taler Node"
echo "========================================="
echo "Data directory: $TALER_DATA"
echo "Config file: $TALER_CONF"
echo "P2P Port: 23153"
echo "========================================="
echo ""
if [ ! -f "$TALER_CONF" ]; then
    echo "NOTE: No config file found - using defaults"
    echo "RPC is disabled by default."
else
    echo "NOTE: Using custom configuration from $TALER_CONF"
fi
echo "========================================="

# Execute command
if [ "$1" = "talerd" ]; then
    exec talerd -datadir="$TALER_DATA" -conf="$TALER_CONF" -printtoconsole
else
    exec "$@"
fi
