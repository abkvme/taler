#!/bin/bash
set -e

# Taler Docker Entrypoint Script

# Set default data directory
TALER_DATA=${TALER_DATA:-/data}

# Initialize data directory if needed
if [ ! -d "$TALER_DATA" ]; then
    echo "Creating data directory at $TALER_DATA"
    mkdir -p "$TALER_DATA"
fi

# Create taler.conf if it doesn't exist
if [ ! -f "$TALER_DATA/taler.conf" ]; then
    echo "Creating default taler.conf"
    cat > "$TALER_DATA/taler.conf" <<EOF
# Taler Configuration
server=1
rpcbind=0.0.0.0
rpcport=23333
rpcuser=${TALER_RPCUSER:-rpcuser}
rpcpassword=${TALER_RPCPASSWORD:-changeme}
rpcallowip=${TALER_RPCALLOWIP:-0.0.0.0/0}
listen=1
maxconnections=128
dbcache=450
logips=1
shrinkdebugfile=1
logtimestamps=1
EOF
    echo "Default configuration created. Please customize $TALER_DATA/taler.conf"
fi

# Handle RPC credentials from environment variables
if [ -n "$TALER_RPCUSER" ] && [ -n "$TALER_RPCPASSWORD" ]; then
    echo "Configuring RPC credentials from environment"
    sed -i "s/^rpcuser=.*/rpcuser=$TALER_RPCUSER/" "$TALER_DATA/taler.conf" 2>/dev/null || true
    sed -i "s/^rpcpassword=.*/rpcpassword=$TALER_RPCPASSWORD/" "$TALER_DATA/taler.conf" 2>/dev/null || true
fi

# Handle RPC allow IP from environment
if [ -n "$TALER_RPCALLOWIP" ]; then
    sed -i "s/^rpcallowip=.*/rpcallowip=$TALER_RPCALLOWIP/" "$TALER_DATA/taler.conf" 2>/dev/null || true
fi

# Print startup message
echo "========================================="
echo "  Starting Taler Node"
echo "========================================="
echo "Data directory: $TALER_DATA"
echo "Configuration: $TALER_DATA/taler.conf"
echo "P2P Port: 23153"
echo "RPC Port: 23333"
echo "========================================="

# Execute command
if [ "$1" = "talerd" ]; then
    exec talerd -datadir="$TALER_DATA" -conf="$TALER_DATA/taler.conf" -printtoconsole
else
    exec "$@"
fi
