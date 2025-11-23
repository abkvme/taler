#!/bin/bash
set -e

# Taler Docker Entrypoint Script

# Set default data directory
TALER_DATA=${TALER_DATA:-/data}

# Default config file location (separate from data)
TALER_CONF=${TALER_CONF:-/taler.conf}

# Wallet directory (optional - wallet disabled if not set)
# TALER_WALLETDIR is intentionally left unset by default

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

# Show wallet status
if [ -n "$TALER_WALLETDIR" ]; then
    echo "Wallet: ENABLED (directory: $TALER_WALLETDIR)"
else
    echo "Wallet: DISABLED (set TALER_WALLETDIR to enable)"
fi

echo "========================================="
echo ""
if [ ! -f "$TALER_CONF" ]; then
    echo "NOTE: No config file found - using defaults"
    echo "RPC is disabled by default."
else
    echo "NOTE: Using custom configuration from $TALER_CONF"
fi
echo "========================================="

# Build command arguments
ARGS="-datadir=$TALER_DATA"

# Add config file if it exists
if [ -f "$TALER_CONF" ]; then
    ARGS="$ARGS -conf=$TALER_CONF"
else
    # No config file - explicitly disable RPC server for security
    ARGS="$ARGS -server=0"
fi

# Handle wallet configuration
if [ -n "$TALER_WALLETDIR" ]; then
    # Wallet enabled - specify wallet directory
    ARGS="$ARGS -walletdir=$TALER_WALLETDIR"
else
    # Wallet disabled by default
    ARGS="$ARGS -disablewallet"
fi

# Execute command
if [ "$1" = "talerd" ]; then
    exec talerd $ARGS -printtoconsole
else
    exec "$@"
fi
