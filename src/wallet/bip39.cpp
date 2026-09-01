// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/bip39.h>

#include <wallet/bip39_english.h>

#include <crypto/hmac_sha512.h>
#include <crypto/sha256.h>
#include <random.h>
#include <support/cleanse.h>

#include <algorithm>
#include <cstring>

namespace bip39 {

namespace {

const size_t WORDLIST_SIZE = 2048;
const unsigned int PBKDF2_ROUNDS = 2048;

//! PBKDF2-HMAC-SHA512. Only ever called with dkLen == 64, i.e. a single block.
void PBKDF2_HMAC_SHA512(const unsigned char* pass, size_t pass_len,
                        const unsigned char* salt, size_t salt_len,
                        unsigned int rounds, unsigned char out[64])
{
    unsigned char block[64];
    unsigned char u[64];

    // U1 = PRF(pass, salt || INT_32_BE(1))
    const unsigned char index_be[4] = {0, 0, 0, 1};
    CHMAC_SHA512(pass, pass_len).Write(salt, salt_len).Write(index_be, sizeof(index_be)).Finalize(u);
    memcpy(block, u, sizeof(block));

    for (unsigned int i = 1; i < rounds; ++i) {
        CHMAC_SHA512(pass, pass_len).Write(u, sizeof(u)).Finalize(u);
        for (size_t j = 0; j < sizeof(block); ++j) block[j] ^= u[j];
    }

    memcpy(out, block, sizeof(block));
    memory_cleanse(block, sizeof(block));
    memory_cleanse(u, sizeof(u));
}

std::vector<std::string> SplitWords(const SecureString& normalized)
{
    std::vector<std::string> words;
    std::string current;
    for (char c : normalized) {
        if (c == ' ') {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) words.push_back(current);
    return words;
}

bool EntropyLengthOk(size_t bytes)
{
    return bytes >= ENTROPY_MIN_BYTES && bytes <= ENTROPY_MAX_BYTES && (bytes % 4) == 0;
}

} // namespace

const char* const* EnglishWordlist() { return ENGLISH_WORDLIST; }
size_t EnglishWordlistSize() { return WORDLIST_SIZE; }

int WordIndex(const std::string& word)
{
    // The BIP-39 English wordlist is sorted, so a binary search is exact and cheap.
    const char* const* begin = ENGLISH_WORDLIST;
    const char* const* end = ENGLISH_WORDLIST + WORDLIST_SIZE;
    const char* const* it = std::lower_bound(begin, end, word,
        [](const char* a, const std::string& b) { return std::strcmp(a, b.c_str()) < 0; });
    if (it == end || word != *it) return -1;
    return static_cast<int>(it - begin);
}

SecureString NormalizeMnemonic(const SecureString& mnemonic)
{
    SecureString out;
    out.reserve(mnemonic.size());
    bool pending_space = false;
    for (char c : mnemonic) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc == ' ' || uc == '\t' || uc == '\n' || uc == '\r' || uc == '\f' || uc == '\v') {
            if (!out.empty()) pending_space = true;
            continue;
        }
        if (pending_space) {
            out.push_back(' ');
            pending_space = false;
        }
        out.push_back(static_cast<char>(uc >= 'A' && uc <= 'Z' ? uc - 'A' + 'a' : uc));
    }
    return out;
}

bool MnemonicFromEntropy(const SecureBytes& entropy, SecureString& mnemonic_out)
{
    if (!EntropyLengthOk(entropy.size())) return false;

    const size_t entropy_bits = entropy.size() * 8;
    const size_t checksum_bits = entropy_bits / 32;
    const size_t word_count = (entropy_bits + checksum_bits) / 11;

    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(entropy.data(), entropy.size()).Finalize(hash);

    // Bit i of the concatenated entropy||checksum stream.
    auto bit_at = [&](size_t i) -> unsigned {
        if (i < entropy_bits) return (entropy[i / 8] >> (7 - (i % 8))) & 1u;
        const size_t j = i - entropy_bits;
        return (hash[j / 8] >> (7 - (j % 8))) & 1u;
    };

    mnemonic_out.clear();
    for (size_t w = 0; w < word_count; ++w) {
        unsigned index = 0;
        for (size_t b = 0; b < 11; ++b) index = (index << 1) | bit_at(w * 11 + b);
        if (w) mnemonic_out.push_back(' ');
        mnemonic_out += ENGLISH_WORDLIST[index];
    }
    memory_cleanse(hash, sizeof(hash));
    return true;
}

bool GenerateMnemonic(size_t entropy_bytes, SecureString& mnemonic_out)
{
    if (!EntropyLengthOk(entropy_bytes)) return false;
    SecureBytes entropy(entropy_bytes);
    GetStrongRandBytes(entropy.data(), entropy.size());
    const bool ok = MnemonicFromEntropy(entropy, mnemonic_out);
    memory_cleanse(entropy.data(), entropy.size());
    return ok;
}

bool MnemonicToEntropy(const SecureString& mnemonic, SecureBytes& entropy_out)
{
    const SecureString normalized = NormalizeMnemonic(mnemonic);
    const std::vector<std::string> words = SplitWords(normalized);

    const size_t word_count = words.size();
    if (word_count % 3 != 0 || word_count < 12 || word_count > 24) return false;

    const size_t total_bits = word_count * 11;
    const size_t checksum_bits = total_bits / 33;
    const size_t entropy_bits = total_bits - checksum_bits;
    if (!EntropyLengthOk(entropy_bits / 8)) return false;

    std::vector<unsigned char, secure_allocator<unsigned char>> bits(total_bits, 0);
    for (size_t w = 0; w < word_count; ++w) {
        const int index = WordIndex(words[w]);
        if (index < 0) return false;
        for (size_t b = 0; b < 11; ++b) {
            bits[w * 11 + b] = static_cast<unsigned char>((index >> (10 - b)) & 1);
        }
    }

    SecureBytes entropy(entropy_bits / 8, 0);
    for (size_t i = 0; i < entropy_bits; ++i) {
        entropy[i / 8] = static_cast<unsigned char>((entropy[i / 8] << 1) | bits[i]);
    }

    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(entropy.data(), entropy.size()).Finalize(hash);
    for (size_t i = 0; i < checksum_bits; ++i) {
        const unsigned expected = (hash[i / 8] >> (7 - (i % 8))) & 1u;
        if (bits[entropy_bits + i] != expected) {
            memory_cleanse(hash, sizeof(hash));
            return false;
        }
    }
    memory_cleanse(hash, sizeof(hash));

    entropy_out = entropy;
    return true;
}

bool MnemonicIsValid(const SecureString& mnemonic)
{
    SecureBytes entropy;
    return MnemonicToEntropy(mnemonic, entropy);
}

bool MnemonicIsValidForWallet(const SecureString& mnemonic)
{
    if (SplitWords(NormalizeMnemonic(mnemonic)).size() != DEFAULT_WORD_COUNT) return false;
    return MnemonicIsValid(mnemonic);
}

void MnemonicToSeed(const SecureString& mnemonic, const SecureString& passphrase, unsigned char seed_out[SEED_BYTES])
{
    const SecureString normalized = NormalizeMnemonic(mnemonic);
    SecureString salt("mnemonic");
    salt += passphrase;
    PBKDF2_HMAC_SHA512(reinterpret_cast<const unsigned char*>(normalized.data()), normalized.size(),
                       reinterpret_cast<const unsigned char*>(salt.data()), salt.size(),
                       PBKDF2_ROUNDS, seed_out);
}

} // namespace bip39
