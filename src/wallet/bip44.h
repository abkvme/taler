// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_BIP44_H
#define BITCOIN_WALLET_BIP44_H

#include <key.h>

#include <string>

/**
 * BIP-44 key derivation for recovery-phrase wallets.
 *
 *     m / 44' / <coin_type>' / <account>' / <change> / <index>
 *
 * Purpose, coin type and account are hardened; change and index are NOT. That
 * is deliberate: non-hardened leaves are what make an account-level xpub able
 * to derive receive addresses, which the Taler hot-wallet API depends on.
 *
 * The consequence is standard BIP-32 and is documented in the spec: an account
 * xpub combined with any single child private key yields the account xprv. The
 * wallet therefore never offers a single-address key export, and dumpprivkey
 * warns on such wallets.
 *
 * Legacy wallets are untouched by this file. They keep deriving m/0'/0'/k' with
 * a hardened leaf, exactly as they always have.
 *
 * The scheme is shared byte for byte with the Taler mobile wallet:
 * https://github.com/abkvme/taler.spec
 */
namespace bip44 {

//! BIP-43 purpose field: 44' for BIP-44.
static const uint32_t PURPOSE = 44;
//! The only account this wallet version uses.
static const uint32_t DEFAULT_ACCOUNT = 0;
//! Chain (change) values, per BIP-44.
static const uint32_t CHAIN_EXTERNAL = 0;
static const uint32_t CHAIN_INTERNAL = 1;

/** Master key from a BIP-39 seed. */
void MasterKeyFromSeed(const unsigned char* seed, unsigned int seed_len, CExtKey& master_out);

/** Derive m/44'/<coin_type>'/<account>' from the master key. */
bool DeriveAccount(const CExtKey& master, uint32_t coin_type, uint32_t account, CExtKey& account_out);

/** Derive <account>/<chain>/<index> from an account key. */
bool DeriveChild(const CExtKey& account_key, uint32_t chain, uint32_t index, CExtKey& child_out);

/** Human-readable path, e.g. "m/44'/1524'/0'/0/5". Used for key metadata. */
std::string FormatPath(uint32_t coin_type, uint32_t account, uint32_t chain, uint32_t index);

/** Path of the account node itself, e.g. "m/44'/1524'/0'". */
std::string FormatAccountPath(uint32_t coin_type, uint32_t account);

} // namespace bip44

#endif // BITCOIN_WALLET_BIP44_H
