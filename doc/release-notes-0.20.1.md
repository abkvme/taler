Taler Core version 0.20.1
=========================

This release makes staking and mining work with more than one wallet, and adds
an **Earn** page to the desktop wallet for both.

**Nothing changes on the network.** There is no consensus change and no protocol
change. 0.20.1 talks to 0.20.0 and 0.19.x nodes exactly as they talk to each other.

The full user guide - the Earn page, every `taler.conf` setting, headless
staking and the RPCs - is in [staking-and-mining.md](staking-and-mining.md).

Upgrading
---------

Stop the node, replace the binaries, start it again. No reindex, no rescan.

Read **What changes for you** below if you stake from the desktop wallet with a
wallet that is not encrypted: that wallet no longer stakes in the background.

Fixed: staking with more than one wallet
----------------------------------------

0.20.0 added multi-wallet support, but staking was never adapted to it:

* **Wallets created or restored in the desktop wallet never staked.** The node
  started one staker at startup, bound to whichever wallet was loaded first.
  Wallets created, restored or reopened from the desktop wallet load after that,
  so they had no staker. A new user with no older wallet had no staker at all.
* **The desktop wallet showed staking that was not happening.** "Staking — time
  remaining" was worked out from the wallet's unlock time alone. On any wallet
  other than the first, you could enter the passphrase and watch the countdown
  while nothing was staked.
* **Removing or renaming the staking wallet hung.** The staker held on to its
  wallet for the whole session, so closing that wallet waited forever.

If you created or restored a wallet in 0.20.0 and started staking on it, it did
not stake. Start it again from the Earn page after upgrading.

Staking and mining now
----------------------

The node stakes **or** mines, with **one wallet at a time**. Starting one replaces
whatever was running. Switching between wallets in the desktop wallet does not
stop anything.

### The Earn page

A new **Earn** page, after Receive, has staking on the left and mining on the
right. Staking and mining controls are no longer on the Overview page; its reward
badges and chart stay where they were.

* **Staking** needs an encrypted wallet. You enter the passphrase and pick how
  long to stake (1 hour up to 30 days); the wallet stays unlocked for that time,
  then the node locks it again. Locking the wallet yourself also ends staking.
* **Mining** also needs an encrypted wallet, and the passphrase to start, but the
  wallet stays locked: the reward address is set aside when mining starts. You
  pick the number of processor threads (up to one fewer than the cores) and how
  long to mine, including **Until stopped**.
* When a button cannot be pressed, the page says why: the node is still syncing,
  the wallet is not encrypted, another wallet is already staking or mining (with
  a link to go to it), mining is running, there is no coin of at least 10 TLR, or
  the coins are not old enough yet (with the date they will be).
* The wallet selector and the Manage wallets list mark the wallet that is staking
  or mining. That wallet cannot be removed or renamed until you stop it.
* When Taler starts again after it was closed while staking or mining, it offers
  to resume, with the time that was left, and asks for the passphrase. It never
  resumes on its own.

### What changes for you

* **Desktop wallet, unencrypted wallet.** Until now the node staked the first
  wallet in the background whether or not it was encrypted, with no way to turn
  it off. The desktop wallet no longer does that. If an unencrypted wallet has
  coins to stake, you are told once after upgrading. To keep staking, encrypt the
  wallet and start staking from the Earn page.
* **Desktop wallet with `stakegen=1` or `gen=1` in taler.conf.** Obeyed as before:
  staking or mining starts on its own and the Earn page shows it as started by
  taler.conf, with a working Stop.
* **talerd.** Unchanged: `-stakegen` (on by default) stakes the first wallet, and
  `-gen` mines to it. Two new settings choose a different wallet:

      -stakewallet=<name>   wallet that -stakegen stakes with
      -minewallet=<name>    wallet that -gen mines to

  A `-stakewallet` that is not loaded yet starts staking as soon as it is.

### RPC

| RPC | Change |
| --- | --- |
| `setgeneratepos count ( duration )` | Acts on the wallet in the request (`-rpcwallet`). `1` stakes with it, replacing any staking or mining; `0` stops staking. With a duration in seconds, staking also stops when the wallet is locked, and locks the wallet when it ends. |
| `setgeneratepow count ( duration )` | Acts on the wallet in the request. `count` mining threads, replacing any staking or mining; `0` stops mining. The reward goes to a new address of the wallet, which can stay locked. **Stopping mining no longer starts staking again**, as it used to. |
| `getgenerate` | Also reports `mode` (`off`, `staking` or `mining`), `wallet`, `until`, `remaining` and `from_config`. `minerPOWCount` and `minerPOSCount` are kept. |
| `unloadwallet` | Stops staking or mining first if the wallet is the one doing it. |

Testing
-------

    python3 test/functional/wallet_generation.py

stakes with a wallet created after startup, checks that one wallet at a time is
enforced, that unloading the staking wallet returns, that timed sessions end and
lock the wallet, that mining pays the chosen wallet, and that `-stakewallet` and
`-minewallet` pick the wallet at startup.
