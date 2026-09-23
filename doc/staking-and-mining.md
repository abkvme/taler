Staking and Mining
==================

Taler is a hybrid chain: blocks are produced both by **staking** (proof-of-stake,
using coins you hold) and by **mining** (proof-of-work, using your processor).
This guide covers both, in the desktop wallet (`taler-qt`) and on a headless
node (`talerd`), as they work from **version 0.20.1**.

- [The one rule: one wallet at a time](#the-one-rule-one-wallet-at-a-time)
- [What staking needs](#what-staking-needs)
- [Desktop wallet: the Earn page](#desktop-wallet-the-earn-page)
  - [Staking](#staking)
  - [Mining](#mining)
  - [Why a button is greyed out](#why-a-button-is-greyed-out)
  - [Seeing which wallet is busy](#seeing-which-wallet-is-busy)
  - [Closing and reopening Taler](#closing-and-reopening-taler)
  - [The Overview page](#the-overview-page)
- [Upgrading from 0.20.0 or earlier](#upgrading-from-0200-or-earlier)
- [Configuration file settings](#configuration-file-settings)
- [Headless node (talerd)](#headless-node-talerd)
- [Docker](#docker)
- [RPC reference](#rpc-reference)
- [Troubleshooting](#troubleshooting)

---

The one rule: one wallet at a time
----------------------------------

A node does **one** of three things at any moment:

| State   | Meaning |
|---------|---------|
| Off     | Neither staking nor mining. |
| Staking | One wallet is staking. |
| Mining  | The node is mining, and the rewards go to one wallet. |

Staking and mining never run together, and only one wallet takes part. Starting
one replaces whatever was running before.

**Switching wallets doesn't stop anything.** You can open another wallet in the
wallet selector, check its balance, send from it, and come back, while the first
wallet keeps staking or mining.

What staking needs
------------------

A wallet can stake only when **all** of these are true:

| Requirement | Mainnet | Testnet |
|-------------|---------|---------|
| The node has finished syncing | - | - |
| The wallet is **encrypted** (desktop wallet only) | - | - |
| It holds a coin of **at least 10 TLR** | 10 TLR | 10 TLR |
| That coin is **old enough** | 2 days | 20 minutes |

A "coin" here is a single unspent output: one payment you received, or one piece
of change. A balance of 50 TLR made of ten 5 TLR payments has **no** coin that
can start a stake. Smaller coins aren't wasted, though: once a stake starts from a
large enough coin, smaller mature coins can be added to it. To make one large
coin, send your balance to one of your own addresses.

Age counts from the block the coin was received in. Sending coins, including to
yourself, creates new coins, and their age starts again from zero. Coin age keeps
adding weight for up to 90 days.

While a wallet stakes, the coin being staked is briefly spent and returned
together with the reward, so the balance can change for a moment. This is
normal.

Mining has no such requirements beyond a synced node and, in the desktop wallet,
an encrypted wallet.

---

Desktop wallet: the Earn page
-----------------------------

Open **Earn** in the toolbar, between **Receive** and **Transactions**. The
shortcut is **Alt+8**. The page shows the wallet selected in the wallet
selector: **Staking** on the left, **Mining** on the right.

### Staking

1. Choose a **Duration**: 1 hour, 6 hours, 24 hours, 7 days or 30 days.
2. Press **Start staking** and enter the wallet passphrase.

The wallet is unlocked for the chosen time and the node stakes with it. The card
shows **"Staking, 1d 04h 12m 30s left."** and counts down. When the time is up,
the node stops staking and **locks the wallet again**.

Staking also stops, straight away, if:

- you press **Stop staking** (after a confirmation),
- you lock the wallet yourself,
- you start staking with another wallet, or start mining.

Below the status the card shows **Stakeable now**: the total of coins that can
start a stake right now. If some large coins are still too young, it also shows
when they become stakeable.

Staking needs no durations longer than 30 days and has no "until stopped"
option. The wallet is unlocked for the whole session, and a limit keeps it from
being left unlocked forever by accident.

### Mining

1. Choose **Threads**: how many processor cores to use, from 1 up to one fewer
   than the number of cores in the computer.
2. Choose a **Duration**: 1 hour to 30 days, or **Until stopped**.
3. Press **Start mining** and enter the wallet passphrase.

The passphrase only authorises the start. **The wallet stays locked while
mining**, because mining needs no private keys. When mining starts, a new address
of the wallet is set aside, and every block you mine pays to it. Mined rewards
appear in the wallet's transactions as generated coins and can be spent once
they mature.

Mining keeps the chosen number of cores fully busy. Expect the computer to run
hot and its fans to be loud. It stops when the duration ends, when you press
**Stop mining**, or when you start staking.

### Why a button is greyed out

When **Start** can't be pressed, the card says why. Only the most important
reason is shown, in this order:

| Staking card says | What to do |
|-------------------|------------|
| Waiting for the node to finish syncing. | Wait for the sync to finish. |
| Encrypt this wallet to stake. | **Settings → Encrypt Wallet…**, then come back. |
| This wallet is mining. Stop mining to stake. | Press **Stop mining** on the right. |
| Wallet "X" is mining. Stop mining there to stake. | Press **Go to "X"** and stop mining there. |
| Wallet "X" is staking. Only one wallet can stake at a time. | Press **Go to "X"** and stop staking there. |
| No coins to stake. A stake starts from a coin of at least 10 TLR. | Receive, or combine coins into, a coin of 10 TLR or more. |
| Coins become stakeable on *date*. A coin must be 2 days old to stake. | Wait until the date shown. |

| Mining card says | What to do |
|------------------|------------|
| Waiting for the node to finish syncing. | Wait for the sync to finish. |
| Encrypt this wallet to mine. | **Settings → Encrypt Wallet…**, then come back. |
| This wallet is staking. Stop staking to mine. | Press **Stop staking** on the left. |
| Wallet "X" is staking. Stop staking there to mine. | Press **Go to "X"** and stop staking there. |
| Wallet "X" is mining. Only one wallet can mine at a time. | Press **Go to "X"** and stop mining there. |

**Go to "X"** switches the window to wallet X and stays on the Earn page.

### Seeing which wallet is busy

- The **wallet selector** shows the busy wallet as `name (staking)` or
  `name (mining)`.
- **Manage wallets** shows it in the name column, next to "in use".
- A wallet that is staking or being mined to **can't be removed or renamed**.
  Stop it on the Earn page first.
- The legacy wallet (the `wallet.dat` from before 0.20.0) appears as **legacy**.

### Closing and reopening Taler

When you exit while staking or mining, the confirmation names the wallet that
will stop.

The next time Taler starts, it offers to pick up where you left off:

- **"Wallet X was staking when Taler closed, with 3h 20m left. Resume
  staking?"** It's only offered if time was left. Answer **Yes** and enter the
  passphrase to stake for the remaining time.
- **"Taler was mining to wallet X with 2 threads when it closed. Resume
  mining?"** It's offered for any unfinished mining session, including "until
  stopped".

Taler never resumes on its own. An encrypted wallet can't be unlocked without
its passphrase, and you should always know when your wallet is unlocked. If you
answer **No**, the session is forgotten.

Staking or mining started by the configuration file (see below) isn't offered
for resume: it simply starts again, as the configuration file asks.

### The Overview page

The Overview page keeps its reward summary:

- **Staking**: how long ago this wallet last earned a staking reward. Green is
  under 7 days, amber 7 to 30 days, red is longer or never. A small line
  underneath shows the last mining reward, for wallets that have mined.
- **New round**: "Ready" once 7 days have passed since the last staking reward,
  otherwise how many days remain.
- **Rewards, last 12 months**: one bar per month, with the total.

The staking controls that used to be at the bottom of the Overview page are now
on the Earn page.

---

Upgrading from 0.20.0 or earlier
--------------------------------

**Wallets created or restored in 0.20.0 never staked.** In 0.20.0, only the wallet
loaded first when the node started could stake. Wallets created, restored or
reopened in the desktop wallet were loaded later and never staked, even while the
Overview page counted down "Staking — time remaining". If you started staking on
such a wallet, start it again from the Earn page.

**The desktop wallet no longer stakes in the background.** Before 0.20.1 the node
staked the first wallet all the time, with no way to turn it off, whenever it was
unlocked. An unencrypted wallet counts as always unlocked, so it always staked.
The desktop wallet now stakes only when you start it on the Earn page, and only
with an encrypted wallet.

If you have an unencrypted wallet with coins to stake, Taler tells you once,
after upgrading. To keep staking:

1. **Settings → Encrypt Wallet…** and choose a passphrase. Write the passphrase
   down: without it the coins can't be spent.
2. Open **Earn** and press **Start staking**.

If your `taler.conf` contains `stakegen=1`, nothing changes for you: see below.

Headless nodes (`talerd`) are not affected: they stake the first wallet by
default, as before.

---

Configuration file settings
---------------------------

These settings go in `taler.conf`, or on the command line with a leading dash
(`-stakegen=1`).

| Setting | Default | Meaning |
|---------|---------|---------|
| `stakegen=1\|0` | talerd: `1`<br>desktop wallet: `0` | Stake from startup, with no time limit. |
| `stakewallet=<name>` | the first wallet loaded | Which wallet `stakegen` stakes with. |
| `gen=1\|0` | `0` | Mine from startup, with up to 7 threads (never more than cores − 1). Takes precedence over `stakegen`. |
| `minewallet=<name>` | the first wallet loaded | Which wallet `gen` mines to. |
| `gencomment=<hex>` | none | Extra bytes (hex) added to the coinbase of blocks you mine or stake. |

Notes:

- **Wallet names** are the names shown in Manage wallets, which are also the
  folder names inside the wallet directory. The legacy `wallet.dat` has an empty
  name: `stakewallet=` with nothing after `=`.
- **`stakewallet` doesn't need to be loaded at startup.** Staking begins as soon
  as a wallet with that name is loaded, for example when the desktop wallet
  reopens its wallets. Loading still happens the usual way: `wallet=<name>`
  lines, or the desktop wallet's own list.
- **Staking started by `stakegen` never times out and waits out a locked
  wallet.** An encrypted wallet stakes only while it's unlocked (see below). An
  unencrypted wallet stakes all the time.
- **The desktop wallet obeys `stakegen` and `gen` only when they're written in
  `taler.conf`.** Without them, it never starts staking or mining on its own. The
  Earn page shows conf-started activity as "Started by stakegen=1 in taler.conf"
  (or `gen=1`) and it can be stopped there. It starts again at the next launch,
  because the configuration file asks for it.

Example: a desktop wallet that always stakes the wallet named `savings`:

    stakegen=1
    stakewallet=savings

Example: a server that mines to the wallet `pool`:

    gen=1
    minewallet=pool
    wallet=
    wallet=pool

---

Headless node (talerd)
----------------------

By default `talerd` stakes the first wallet it loads, exactly as before 0.20.1.

**Unencrypted wallet.** It stakes as soon as the node has synced, with no further
steps.

**Encrypted wallet.** It stakes only while unlocked. Unlock it for as long as you
want it to stake:

    taler-cli walletpassphrase "your passphrase" 99999999

(99999999 seconds is just over three years, the longest allowed.) With more than
one wallet loaded, add `-rpcwallet=<name>`:

    taler-cli -rpcwallet=savings walletpassphrase "your passphrase" 99999999

**Choose the wallet** with `stakewallet=` or `minewallet=` in `taler.conf`, or
switch at runtime with the RPCs below:

    taler-cli -rpcwallet=savings setgeneratepos 1          # stake with "savings" until stopped
    taler-cli -rpcwallet=savings setgeneratepos 1 86400    # stake for one day
    taler-cli -rpcwallet=pool setgeneratepow 2             # mine to "pool" with 2 threads
    taler-cli -rpcwallet=pool setgeneratepow 2 3600        # ... for one hour
    taler-cli setgeneratepos 0                             # stop staking
    taler-cli setgeneratepow 0                             # stop mining
    taler-cli getgenerate                                  # what is running now

Starting one replaces whatever was running. Settings changed with the RPCs last
until the node restarts. After that, `taler.conf` applies again.

---

Docker
------

The Docker image runs `talerd`, so everything in the section above applies. The
wallet is disabled in the container by default. See
[README-DOCKER.md](../README-DOCKER.md#wallet-configuration) to enable it.

Once a wallet directory is mounted, add staking or mining settings to your
mounted `taler.conf`, for example:

    stakegen=1
    stakewallet=

and, for an encrypted wallet, unlock it after each container start:

    docker exec -it taler taler-cli walletpassphrase "your passphrase" 99999999

---

RPC reference
-------------

These RPCs act on the wallet named in the request: `-rpcwallet=<name>` for
`taler-cli`, or the `/wallet/<name>` endpoint over JSON-RPC. With only one wallet
loaded, it's used automatically.

### `setgeneratepos count ( duration )`

| Argument | |
|----------|-|
| `count` | `1`: stake with this wallet, replacing any staking or mining. `0`: stop staking. |
| `duration` | Optional, in seconds. Omitted or `0`: stake until stopped, waiting while the wallet is locked. Given: a **session**, which ends after `duration` seconds or as soon as the wallet is locked, and locks the wallet when it ends. |

### `setgeneratepow count ( duration )`

| Argument | |
|----------|-|
| `count` | Number of mining threads, capped at the number of cores minus one. `0`: stop mining. |
| `duration` | Optional, in seconds. Omitted or `0`: mine until stopped. |

The reward goes to a new address of this wallet, set aside when mining starts.
The wallet can stay locked, provided it still has spare addresses in its key
pool. If it has run out, unlock it once and start again. Stopping mining leaves
the node **off**. It doesn't go back to staking, as versions before 0.20.1 did.

### `getgenerate`

Reports what the node is doing:

    {
      "mode": "staking",          // "off", "staking" or "mining"
      "wallet": "savings",        // absent when off
      "until": 1790000000,        // unix time it ends, 0 = until stopped
      "remaining": 86123,         // seconds left, 0 = until stopped
      "from_config": false,       // started by stakegen / gen
      "minerPOWCount": 0,         // mining threads
      "minerPOSCount": 1          // 1 while staking
    }

`setgeneratepos` and `setgeneratepow` return the same object.

### `unloadwallet`

If the wallet being unloaded is the one staking or being mined to, that stops
first. Unloading then returns straight away. In 0.20.0 it could hang forever on
the staking wallet.

---

Troubleshooting
---------------

**The Earn page says "No coins to stake", but I have a balance.**
Your balance is made of coins smaller than 10 TLR. Send the whole balance to one
of your own addresses to combine it into one coin, then wait 2 days for it to
mature.

**It says "Coins become stakeable on …" right after I moved coins.**
Moving coins creates new ones, and their age starts again from zero. Wait until
the date shown.

**Staking is running but I haven't earned anything yet.**
Staking is a lottery weighted by the size and age of your coins. A small balance
can go days or weeks between rewards. Watch the Staking badge on the Overview
page.

**Staking stopped by itself.**
Its time ran out, the wallet was locked, or another wallet was started. Timed
sessions always lock the wallet when they end.

**"Waiting: the wallet is locked. Unlock it to stake."**
Staking was started by `stakegen=1` in `taler.conf`, which waits for an encrypted
wallet to be unlocked instead of stopping. Unlock the wallet (for example with
`walletpassphrase`), or stop it and start a timed session from the Earn page.

**I can't remove or rename a wallet.**
It's staking or being mined to. Stop it on the Earn page first.

**My computer is hot and loud.**
Mining is running. Use fewer threads, or stop it on the Earn page.

**Mining started without me asking.**
`gen=1` is in your `taler.conf`. Remove it to stop mining at startup.
