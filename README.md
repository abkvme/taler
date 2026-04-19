# Taler (TLR) — Hybrid PoW/PoS Cryptocurrency

> A cryptocurrency inspired by the historic European Thaler — trusted currency
> of the 15th–19th centuries, reimagined for the digital age.

[![Linux x64](https://img.shields.io/github/actions/workflow/status/abkvme/taler/build-linux-x64.yml?branch=main&label=Linux%20x64&logo=linux&logoColor=white)](https://github.com/abkvme/taler/actions/workflows/build-linux-x64.yml)
[![Linux ARM64](https://img.shields.io/github/actions/workflow/status/abkvme/taler/build-linux-arm64.yml?branch=main&label=Linux%20ARM64&logo=linux&logoColor=white)](https://github.com/abkvme/taler/actions/workflows/build-linux-arm64.yml)
[![macOS](https://img.shields.io/github/actions/workflow/status/abkvme/taler/build-macos.yml?branch=main&label=macOS&logo=apple&logoColor=white)](https://github.com/abkvme/taler/actions/workflows/build-macos.yml)
[![Windows x64](https://img.shields.io/github/actions/workflow/status/abkvme/taler/build-windows-x64.yml?branch=main&label=Windows%20x64&logo=windows&logoColor=white)](https://github.com/abkvme/taler/actions/workflows/build-windows-x64.yml)
[![Docker](https://img.shields.io/github/actions/workflow/status/abkvme/taler/docker-publish.yml?branch=main&label=Docker&logo=docker&logoColor=white)](https://github.com/abkvme/taler/actions/workflows/docker-publish.yml)

[![Release](https://img.shields.io/github/v/release/abkvme/taler?label=release)](https://github.com/abkvme/taler/releases)
[![License](https://img.shields.io/github/license/abkvme/taler)](COPYING)
[![Last commit](https://img.shields.io/github/last-commit/abkvme/taler)](https://github.com/abkvme/taler/commits/main)
[![Stars](https://img.shields.io/github/stars/abkvme/taler?style=social)](https://github.com/abkvme/taler/stargazers)
[![Telegram](https://img.shields.io/badge/chat-Telegram-26A5E4?logo=telegram&logoColor=white)](https://t.me/talercommunity)

---

## Table of Contents

- [About](#about)
- [Key Features](#key-features)
- [GUI Wallet (taler-qt)](#gui-wallet-taler-qt)
- [Quick Start — Docker](#quick-start--docker-daemon-only)
- [Quick Start — docker compose](#quick-start--docker-compose)
- [Self-Compile](#self-compile)
- [Connecting](#connecting)
- [Blockchain Explorers](#blockchain-explorers)
- [Community & Resources](#community--resources)
- [Contributing](#contributing)
- [License](#license)

---

## About

**Taler (ticker: TLR)** is a decentralized, open-source cryptocurrency that
secures its blockchain with a hybrid **Proof-of-Work / Proof-of-Stake**
consensus: ASIC-resistant Lyra2Z mining produces blocks while Proof-of-Stake
(active from block 130,000) lets any coin holder earn rewards simply by
running a staking wallet. Transactions are fast, fees are minimal, and the
network is governed by its users — no central authority, no gatekeepers.

Taler is a **maintained fork of Bitcoin Core** and inherits its battle-tested
transaction engine, wallet, and peer-to-peer stack. On top of that foundation
it adds PoS, Lyra2Z PoW, a modern desktop GUI with one-click staking, and
cross-platform builds for Linux x64 / ARM64, macOS (Apple Silicon), and
Windows x64. This repository is the continuation of the original
[cryptadev/taler](https://github.com/cryptadev/taler) codebase — actively
developed, CI-tested on every pull request, and released under the MIT
license.

Learn more on the project website: **[taler.tech](https://taler.tech)**.

## Key Features

- **Hybrid PoW/PoS consensus** — Lyra2Z Proof-of-Work (ASIC-resistant,
  memory-hard) with Proof-of-Stake active from block 130,000
- **1-minute block target** (PoW; ~2.3 minutes in PoS phase)
- **Max supply: 23,333,333 TLR**
- **Genesis block**: September 13, 2017
- **Cross-platform**: Linux x64/ARM64, macOS arm64, Windows x64
- **Bitcoin Core heritage** — forked codebase, MIT licensed

## GUI Wallet (taler-qt)

`taler-qt` is the desktop wallet. It ships with every release for Linux,
macOS, and Windows; you can also build it from source with the scripts in
[Self-Compile](#self-compile).

- **Send / Receive / Transactions** — Bitcoin-Core-class wallet: HD keys,
  coin control, address book, QR codes for receive addresses, labeled
  transaction history, CSV export.
- **One-click staking from the Dashboard** — new in 0.19.5.8. Pick a duration
  (1h / 6h / 24h / 7d / 30d), enter your passphrase once, and the Overview
  page shows a live countdown and progress bar. The wallet auto-relocks when
  the timer expires and on app restart. The panel is hidden for unencrypted
  wallets (where staking is always-on).
- **Nodes page** — new in 0.19.2.8. Displays the hardcoded DNS seeds, a
  community-fetched seed list from [`abkvme/taler-seeds`](https://github.com/abkvme/taler-seeds),
  and the peers your node is currently connected to. A background TCP probe
  colors each entry by reachability.
- **Run it** — Linux/macOS: `./bin/taler-qt`. Windows: double-click
  `taler-qt.exe` from the release zip (or from `./bin/` after a local build).

## Quick Start — Docker (daemon only)

The published Docker image contains the headless daemon (`talerd`) and CLI
(`taler-cli`). For the GUI wallet, grab a binary from
[Releases](https://github.com/abkvme/taler/releases) or build from source.

```bash
docker run -d --name taler \
  -p 23153:23153 \
  -p 127.0.0.1:23333:23333 \
  -v taler-data:/data \
  ghcr.io/abkvme/taler:latest
```

Check in once it's running:

```bash
docker exec -it taler taler-cli getblockchaininfo
```

## Quick Start — docker compose

The repo ships a production-ready [`docker-compose.yml`](docker-compose.yml):

```yaml
services:
  taler:
    image: ghcr.io/abkvme/taler:latest
    container_name: taler-node
    restart: unless-stopped
    ports:
      - "23153:23153"   # P2P network port
      - "23333:23333"   # RPC port
    volumes:
      - taler-data:/data
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"

volumes:
  taler-data:
    driver: local
```

Clone and start:

```bash
git clone https://github.com/abkvme/taler.git
cd taler
docker compose up -d
docker compose logs -f taler
```

To build the image locally instead of pulling the published one, use
[`docker-compose.build.yml`](docker-compose.build.yml).

## Self-Compile

Each platform has a self-contained build script at the repo root that mirrors
the CI workflow. First run takes longer (dependencies are compiled from
source where necessary); subsequent runs are incremental.

- **Linux (Ubuntu 24.04)** — system Qt5/Boost, builds daemon + GUI.
  ```bash
  ./build_linux.sh --install-deps   # first time only
  ./build_linux.sh
  ```
  See [`doc/build-unix.md`](doc/build-unix.md) for details.

- **macOS (Apple Silicon)** — fully static build via `depends/`. First
  compile is 15–30 min.
  ```bash
  brew install automake libtool pkg-config
  ./build_macos.sh
  ```
  See [`doc/build-osx.md`](doc/build-osx.md).

- **Windows x64** — cross-compiled on Ubuntu via MinGW-w64. First compile is
  30–60 min.
  ```bash
  ./build_windows.sh --install-deps   # first time only
  ./build_windows.sh
  ```
  If your dev machine is Windows, run the above inside **WSL2** (Ubuntu
  24.04). See [`doc/build-windows.md`](doc/build-windows.md).

All scripts output binaries to `./bin/` (`talerd`, `taler-cli`, `taler-tx`,
`taler-qt`).

## Connecting

- **P2P port**: `23153`
- **RPC port**: `23333` (the default `docker-compose.yml` binds RPC to
  localhost only)
- **RPC auth**: generate credentials with `share/rpcauth/rpcauth.py` or set
  them manually in `taler.conf`.
- **Seed nodes**: the client seeds automatically from hardcoded DNS seeds
  plus the community-maintained list at
  [`abkvme/taler-seeds`](https://github.com/abkvme/taler-seeds).

## Blockchain Explorers

- [explorer.talercoin.org](https://explorer.talercoin.org/)
- [explorer.talercrypto.com](https://explorer.talercrypto.com/)

## Community & Resources

- **Website** — [taler.tech](https://taler.tech)
- **Telegram** — [t.me/talercommunity](https://t.me/talercommunity)
- **GitHub Discussions** — [abkvme/taler/discussions](https://github.com/abkvme/taler/discussions)
- **Issue tracker** — [abkvme/taler/issues](https://github.com/abkvme/taler/issues)
- **Upstream fork** — [cryptadev/taler](https://github.com/cryptadev/taler)
  (the repository this project was forked from)

## Contributing

Pull requests are welcome. Every PR against `main` runs all four build
workflows (Linux x64, Linux ARM64, macOS, Windows x64) plus Docker build —
please keep the tree green. Update [`CHANGELOG.md`](CHANGELOG.md) for any
user-visible change. See [`doc/developer-notes.md`](doc/developer-notes.md)
for coding style and workflow notes, and
[`CONTRIBUTING.md`](CONTRIBUTING.md) for the contribution process.

## License

Released under the **MIT License**. See [`COPYING`](COPYING) for the full
text and copyright holders.
