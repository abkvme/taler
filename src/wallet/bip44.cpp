// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/bip44.h>

#include <tinyformat.h>

namespace bip44 {

namespace {
const uint32_t HARDENED = 0x80000000;
} // namespace

void MasterKeyFromSeed(const unsigned char* seed, unsigned int seed_len, CExtKey& master_out)
{
    master_out.SetSeed(seed, seed_len);
}

bool DeriveAccount(const CExtKey& master, uint32_t coin_type, uint32_t account, CExtKey& account_out)
{
    // Hardened at every level here, so an account xpub cannot be walked back up
    // to its siblings or to the master key.
    CExtKey purpose_key, coin_key;
    if (!master.Derive(purpose_key, PURPOSE | HARDENED)) return false;
    if (!purpose_key.Derive(coin_key, coin_type | HARDENED)) return false;
    return coin_key.Derive(account_out, account | HARDENED);
}

bool DeriveChild(const CExtKey& account_key, uint32_t chain, uint32_t index, CExtKey& child_out)
{
    // Non-hardened, so the account xpub can derive these public keys on its own.
    if (index >= HARDENED || chain >= HARDENED) return false;
    CExtKey chain_key;
    if (!account_key.Derive(chain_key, chain)) return false;
    return chain_key.Derive(child_out, index);
}

std::string FormatAccountPath(uint32_t coin_type, uint32_t account)
{
    return strprintf("m/%u'/%u'/%u'", PURPOSE, coin_type, account);
}

std::string FormatPath(uint32_t coin_type, uint32_t account, uint32_t chain, uint32_t index)
{
    return strprintf("%s/%u/%u", FormatAccountPath(coin_type, account), chain, index);
}

} // namespace bip44
