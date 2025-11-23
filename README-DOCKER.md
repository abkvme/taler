# Taler Docker Deployment Guide

Run a Taler cryptocurrency node in Docker with support for both **amd64** and **arm64** architectures.

## Quick Start

### Using Pre-built Docker Image (Recommended)

1. **Download the docker-compose.yml:**
   ```bash
   curl -O https://raw.githubusercontent.com/abkvme/taler/main/docker-compose.yml
   ```

2. **Optional - Configure environment variables:**
   ```bash
   # Download example environment file
   curl -O https://raw.githubusercontent.com/abkvme/taler/main/deploy/.env.example
   cp .env.example .env
   # Edit .env with your settings
   nano .env
   ```

3. **Start the node:**
   ```bash
   docker-compose up -d
   ```

4. **Check logs:**
   ```bash
   docker-compose logs -f taler
   ```

5. **Interact with the node:**
   ```bash
   docker-compose exec taler taler-cli getblockchaininfo
   ```

### Using Docker Compose with Host Path

If you prefer to use a host directory for data instead of a Docker volume:

1. **Download the host-based compose file:**
   ```bash
   curl -O https://raw.githubusercontent.com/abkvme/taler/main/deploy/docker-compose.host.yml
   ```

2. **Create data directory:**
   ```bash
   mkdir -p ./data
   ```

3. **Start the node:**
   ```bash
   docker-compose -f docker-compose.host.yml up -d
   ```

### Building from Source

1. **Clone the repository:**
   ```bash
   git clone https://github.com/abkvme/taler.git
   cd taler
   ```

2. **Build and start:**
   ```bash
   docker-compose -f docker-compose.build.yml up -d
   ```

### Using Docker CLI

**Pull and run the pre-built image:**

```bash
docker run -d \
  --name taler-node \
  -p 23153:23153 \
  -p 23333:23333 \
  -v taler-data:/data \
  ghcr.io/abkvme/taler:latest
```

**With environment variables (optional):**

```bash
docker run -d \
  --name taler-node \
  -p 23153:23153 \
  -p 23333:23333 \
  -v taler-data:/data \
  -e TALER_RPCUSER=yourusername \
  -e TALER_RPCPASSWORD=yourpassword \
  -e TALER_RPCALLOWIP=192.168.1.0/24 \
  ghcr.io/abkvme/taler:latest
```

**View logs:**

```bash
docker logs -f taler-node
```

## Architecture Support

This Docker image supports the following platforms:

- **linux/amd64** (x86_64)
- **linux/arm64** (ARM64/aarch64)

Docker will automatically select the correct image for your platform.

## Configuration

The Taler daemon **does not require a configuration file** and works with sensible defaults. Configuration can be done through environment variables or an optional custom config file.

### Environment Variables (Recommended)

The easiest way to customize your node is using environment variables. Download the example file:

```bash
curl -O https://raw.githubusercontent.com/abkvme/taler/main/deploy/.env.example
cp .env.example .env
# Edit .env with your settings
```

Available variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `TALER_RPCUSER` | `rpcuser` | RPC username |
| `TALER_RPCPASSWORD` | `changeme` | RPC password (change this!) |
| `TALER_RPCALLOWIP` | `0.0.0.0/0` | IP range allowed to connect to RPC |
| `TALER_DATA` | `/data` | Data directory inside container |

Docker Compose will automatically load the `.env` file if present.

### Custom Configuration File (Optional)

For advanced users who need a custom `taler.conf`:

**Using Docker Compose:**

1. Create your `taler.conf` in the same directory as `docker-compose.yml`
2. Edit `docker-compose.yml` and uncomment the config volume line:
   ```yaml
   volumes:
     - taler-data:/data
     - ./taler.conf:/data/taler.conf  # Uncomment this line
   ```
3. Start the container:
   ```bash
   docker-compose up -d
   ```

**Using Docker CLI:**

```bash
docker run -d \
  --name taler-node \
  -v $(pwd)/taler.conf:/data/taler.conf \
  -v taler-data:/data \
  ghcr.io/abkvme/taler:latest
```

**Note:** You can download an example config from the repository:
```bash
curl -O https://raw.githubusercontent.com/abkvme/taler/main/docker/taler.conf.example
mv taler.conf.example taler.conf
# Edit taler.conf as needed
```

## Exposed Ports

| Port | Protocol | Description |
|------|----------|-------------|
| **23153** | TCP | P2P network port (mainnet) |
| **23333** | TCP | JSON-RPC port (mainnet) |

For testnet, ports are 18333 (P2P) and 18332 (RPC).

## Data Persistence

Blockchain data is stored in the Docker volume `taler-data`. To manage this volume:

```bash
# List volumes
docker volume ls

# Inspect volume
docker volume inspect taler-data

# Backup volume
docker run --rm -v taler-data:/data -v $(pwd):/backup ubuntu tar czf /backup/taler-backup.tar.gz /data

# Restore volume
docker run --rm -v taler-data:/data -v $(pwd):/backup ubuntu tar xzf /backup/taler-backup.tar.gz -C /
```

## Using the RPC Interface

### From Host Machine

```bash
# Get blockchain info
docker-compose exec taler taler-cli getblockchaininfo

# Get wallet info
docker-compose exec taler taler-cli getwalletinfo

# Get peer info
docker-compose exec taler taler-cli getpeerinfo
```

### From External Applications

Configure your application to connect to:
- **Host:** `localhost` (or your server IP)
- **Port:** `23333`
- **Username:** Value of `TALER_RPCUSER`
- **Password:** Value of `TALER_RPCPASSWORD`

Example using `curl`:

```bash
curl --user rpcuser:changeme \
  --data-binary '{"jsonrpc":"1.0","id":"curltest","method":"getblockchaininfo","params":[]}' \
  -H 'content-type: text/plain;' \
  http://localhost:23333/
```

## Multi-Architecture Build

To build images for multiple architectures:

```bash
# Enable Docker buildx
docker buildx create --name multiarch --use

# Build for both amd64 and arm64
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -t yourusername/taler:latest \
  --push \
  .
```

## Health Checks

The container includes a health check that runs every 30 seconds:

```bash
# Check container health status
docker inspect --format='{{.State.Health.Status}}' taler-node

# View health check logs
docker inspect --format='{{range .State.Health.Log}}{{.Output}}{{end}}' taler-node
```

## Troubleshooting

### Container won't start

1. **Check logs:**
   ```bash
   docker-compose logs taler
   ```

2. **Verify ports are available:**
   ```bash
   netstat -tuln | grep -E '23153|23333'
   ```

3. **Check permissions:**
   ```bash
   docker-compose exec taler ls -la /data
   ```

### RPC connection refused

1. **Verify RPC is enabled** in `taler.conf`:
   ```
   server=1
   rpcbind=0.0.0.0
   ```

2. **Check firewall rules** on host machine

3. **Test from inside container:**
   ```bash
   docker-compose exec taler taler-cli getblockchaininfo
   ```

### Slow sync / High memory usage

Increase database cache in `taler.conf`:
```
dbcache=1024  # Increase from default 450MB
```

Or reduce connections:
```
maxconnections=50  # Reduce from default 128
```

## Production Deployment

### Security Best Practices

1. **Change default credentials:**
   ```bash
   export TALER_RPCUSER="your_secure_username"
   export TALER_RPCPASSWORD="$(openssl rand -base64 32)"
   ```

2. **Restrict RPC access:**
   ```
   rpcallowip=192.168.1.0/24  # Only allow local network
   ```

3. **Use a reverse proxy** (nginx/traefik) with TLS for external RPC access

4. **Enable firewall rules:**
   ```bash
   ufw allow 23153/tcp  # P2P
   ufw allow 23333/tcp from 192.168.1.0/24  # RPC only from local network
   ```

### Resource Limits

Add resource limits in `docker-compose.yml`:

```yaml
services:
  taler:
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 4G
        reservations:
          memory: 2G
```

## Publishing to Docker Hub

```bash
# Tag the image
docker tag taler:latest yourusername/taler:0.18.14.7
docker tag taler:latest yourusername/taler:latest

# Push to Docker Hub
docker push yourusername/taler:0.18.14.7
docker push yourusername/taler:latest
```

## Support

- **GitHub Issues:** https://github.com/yourusername/taler/issues
- **Documentation:** See main README.md
- **Configuration Reference:** See `docker/taler.conf.example`

## License

Same as Taler - see LICENSE file in repository root.
