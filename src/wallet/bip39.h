// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_BIP39_H
#define BITCOIN_WALLET_BIP39_H

#include <support/allocators/secure.h>

#include <string>
#include <vector>

/**
 * BIP-39 mnemonic recovery phrases.
 *
 * Taler wallets created from a recovery phrase use 24 words (256 bits of
 * entropy) from the English wordlist, with no BIP-39 passphrase. The shorter
 * lengths are accepted by MnemonicToEntropy() so that a phrase produced
 * elsewhere can still be inspected, but wallet creation always uses 24.
 *
 * The scheme is shared byte for byte with the Taler mobile wallet; see
 * taler-spec/spec/seed-derivation-v1.md and the vectors in
 * taler-spec/vectors/seed-derivation-v1.json.
 *
 * Everything here handles secret material: callers pass and receive
 * SecureString/secure vectors, and nothing in this module logs its inputs.
 */
namespace bip39 {

//! Entropy sizes BIP-39 allows, in bytes.
static const size_t ENTROPY_MIN_BYTES = 16;
static const size_t ENTROPY_MAX_BYTES = 32;
//! What Taler wallets create: 256 bits -> 24 words.
static const size_t ENTROPY_DEFAULT_BYTES = 32;
static const size_t DEFAULT_WORD_COUNT = 24;
static const size_t SEED_BYTES = 64;

typedef std::vector<unsigned char, secure_allocator<unsigned char>> SecureBytes;

/**
 * Normalise a phrase for comparison and validation: trim, collapse runs of
 * whitespace to single spaces, and lowercase.
 *
 * The English wordlist is pure ASCII, so NFKD normalisation is a no-op here and
 * is deliberately not implemented — it would pull in a Unicode dependency to
 * achieve nothing. A non-English wordlist would change that.
 */
SecureString NormalizeMnemonic(const SecureString& mnemonic);

/** Build a mnemonic from entropy. Entropy must be 16..32 bytes and a multiple of 4. */
bool MnemonicFromEntropy(const SecureBytes& entropy, SecureString& mnemonic_out);

/** Generate fresh entropy from the platform CSPRNG and return its mnemonic. */
bool GenerateMnemonic(size_t entropy_bytes, SecureString& mnemonic_out);

/**
 * Recover the entropy behind a mnemonic, validating word membership and the
 * BIP-39 checksum. Input is normalised first.
 */
bool MnemonicToEntropy(const SecureString& mnemonic, SecureBytes& entropy_out);

/** True if the phrase is a well-formed BIP-39 mnemonic (any allowed length). */
bool MnemonicIsValid(const SecureString& mnemonic);

/** True if the phrase is valid AND has the 24 words Taler wallets use. */
bool MnemonicIsValidForWallet(const SecureString& mnemonic);

/**
 * PBKDF2-HMAC-SHA512(mnemonic, "mnemonic" + passphrase, 2048) -> 64 bytes.
 * The mnemonic is normalised first. Taler v1 always passes an empty passphrase.
 */
void MnemonicToSeed(const SecureString& mnemonic, const SecureString& passphrase, unsigned char seed_out[SEED_BYTES]);

/** Index of a word in the English wordlist, or -1. */
int WordIndex(const std::string& word);

/** The English wordlist, exposed for autocomplete in the GUI. */
const char* const* EnglishWordlist();
size_t EnglishWordlistSize();

} // namespace bip39

#endif // BITCOIN_WALLET_BIP39_H
