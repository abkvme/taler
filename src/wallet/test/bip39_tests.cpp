// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/bip39.h>
#include <wallet/bip39_english.h>

#include <crypto/sha256.h>
#include <test/test_bitcoin.h>
#include <utilstrencodings.h>

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(bip39_tests, BasicTestingSetup)

namespace {

struct MnemonicVector {
    const char* entropy_hex;
    const char* mnemonic;
    const char* seed_hex;
};

//! A representative slice of the official BIP-39 English vectors. Those use the
//! passphrase "TREZOR"; Taler wallets always use an empty one.
const MnemonicVector OFFICIAL_VECTORS[] = {
    {"00000000000000000000000000000000",
     "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about",
     "c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04"},
    {"80808080808080808080808080808080",
     "letter advice cage absurd amount doctor acoustic avoid letter advice cage above",
     "d71de856f81a8acc65e6fc851a38d4d7ec216fd0796d0a6827a3ad6ed5511a30fa280f12eb2e47ed2ac03b5c462a0358d18d69fe4f985ec81778c1b370b652a8"},
    {"7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f",
     "legal winner thank year wave sausage worth useful legal winner thank year wave sausage worth useful legal will",
     "f2b94508732bcbacbcc020faefecfc89feafa6649a5491b8c952cede496c214a0c7b3c392d168748f2d4a612bada0753b52a1c7ac53c1e93abd5c6320b9e95dd"},
    {"0000000000000000000000000000000000000000000000000000000000000000",
     "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art",
     "bda85446c68413707090a52022edd26a1c9462295029f2e60cd7c4f2bbd3097170af7a4d73245cafa9c3cca8d561a7c3de6f5d4a10be8ed2a5e608d68f92fcc8"},
    {"9e885d952ad362caeb4efe34a8e91bd2",
     "ozone drill grab fiber curtain grace pudding thank cruise elder eight picnic",
     "274ddc525802f7c828d8ef7ddbcdc5304e87ac3535913611fbbfa986d0c9e5476c91689f9c8a54fd55bd38606aa6a8595ad213d4c9c9f9aca3fb217069a41028"},
    {"f585c11aec520db57dd353c69554b21a89b20fb0650966fa0a9d6f74fd989d8f",
     "void come effort suffer camp survey warrior heavy shoot primary clutch crush open amazing screen patrol group space point ten exist slush involve unfold",
     "01f5bced59dec48e362f2c45b5de68b9fd6c92c6634f44d6d40aab69056506f0e35524a518034ddc1192e1dacd32c1ed3eaa3c3b131c88ed8e7e54c49a5d0998"},
};

//! From taler-spec/vectors/seed-derivation-v1.json - the shared cross-implementation
//! vectors, with the empty passphrase Taler actually uses.
const MnemonicVector TALER_VECTORS[] = {
    {"0000000000000000000000000000000000000000000000000000000000000000",
     "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art",
     "408b285c123836004f4b8842c89324c1f01382450c0d439af345ba7fc49acf705489c6fc77dbd4e3dc1dd8cc6bc9f043db8ada1e243c4a0eafb290d399480840"},
    {"0c1e24e5917779d297e14d45f14e1a1a2ba1ab4b1e1a1a1b1c1d1e1f20212223",
     "army van defense carry jealous true garbage claim echo media make crowd ring stick cluster select borrow suggest also destroy velvet awesome mass involve",
     "78099c3375e9c7323c52c4fc8b9a0b8b2052ab37f539affbacc4a962b423c127ce1821d839b3ca5e1bd9ac99ba880e2eacd6c158aeef4af09cd2858315472d82"},
};

bip39::SecureBytes HexToSecureBytes(const std::string& hex)
{
    const std::vector<unsigned char> v = ParseHex(hex);
    return bip39::SecureBytes(v.begin(), v.end());
}

std::string SeedHex(const SecureString& mnemonic, const SecureString& passphrase)
{
    unsigned char seed[bip39::SEED_BYTES];
    bip39::MnemonicToSeed(mnemonic, passphrase, seed);
    return HexStr(seed, seed + sizeof(seed));
}

} // namespace

//! The wordlist is a consensus artifact shared with every other Taler wallet.
//! If this hash changes, phrases stop being portable.
BOOST_AUTO_TEST_CASE(wordlist_integrity)
{
    BOOST_CHECK_EQUAL(bip39::EnglishWordlistSize(), 2048U);

    std::string joined;
    for (size_t i = 0; i < bip39::EnglishWordlistSize(); ++i) {
        joined += bip39::EnglishWordlist()[i];
        joined += '\n';
    }
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(reinterpret_cast<const unsigned char*>(joined.data()), joined.size()).Finalize(hash);
    BOOST_CHECK_EQUAL(HexStr(hash, hash + sizeof(hash)), std::string(bip39::ENGLISH_WORDLIST_SHA256));

    // Sorted, so the binary search in WordIndex() is exact.
    for (size_t i = 1; i < bip39::EnglishWordlistSize(); ++i) {
        BOOST_CHECK(std::string(bip39::EnglishWordlist()[i - 1]) < std::string(bip39::EnglishWordlist()[i]));
    }
    BOOST_CHECK_EQUAL(bip39::WordIndex("abandon"), 0);
    BOOST_CHECK_EQUAL(bip39::WordIndex("zoo"), 2047);
    BOOST_CHECK_EQUAL(bip39::WordIndex("notaword"), -1);
    BOOST_CHECK_EQUAL(bip39::WordIndex(""), -1);
}

BOOST_AUTO_TEST_CASE(official_bip39_vectors)
{
    for (const MnemonicVector& v : OFFICIAL_VECTORS) {
        SecureString mnemonic;
        BOOST_CHECK(bip39::MnemonicFromEntropy(HexToSecureBytes(v.entropy_hex), mnemonic));
        BOOST_CHECK_EQUAL(std::string(mnemonic.c_str()), std::string(v.mnemonic));

        BOOST_CHECK(bip39::MnemonicIsValid(SecureString(v.mnemonic)));
        BOOST_CHECK_EQUAL(SeedHex(SecureString(v.mnemonic), SecureString("TREZOR")), std::string(v.seed_hex));

        bip39::SecureBytes entropy;
        BOOST_CHECK(bip39::MnemonicToEntropy(SecureString(v.mnemonic), entropy));
        BOOST_CHECK_EQUAL(HexStr(entropy.begin(), entropy.end()), std::string(v.entropy_hex));
    }
}

//! The cross-implementation vectors: the same phrases must produce the same
//! seeds here and in the Taler mobile wallet.
BOOST_AUTO_TEST_CASE(taler_shared_vectors)
{
    for (const MnemonicVector& v : TALER_VECTORS) {
        SecureString mnemonic;
        BOOST_CHECK(bip39::MnemonicFromEntropy(HexToSecureBytes(v.entropy_hex), mnemonic));
        BOOST_CHECK_EQUAL(std::string(mnemonic.c_str()), std::string(v.mnemonic));
        BOOST_CHECK(bip39::MnemonicIsValidForWallet(mnemonic));
        BOOST_CHECK_EQUAL(SeedHex(mnemonic, SecureString("")), std::string(v.seed_hex));
    }
}

BOOST_AUTO_TEST_CASE(generated_mnemonics_round_trip)
{
    for (int i = 0; i < 16; ++i) {
        SecureString mnemonic;
        BOOST_CHECK(bip39::GenerateMnemonic(bip39::ENTROPY_DEFAULT_BYTES, mnemonic));
        BOOST_CHECK(bip39::MnemonicIsValidForWallet(mnemonic));

        bip39::SecureBytes entropy;
        BOOST_CHECK(bip39::MnemonicToEntropy(mnemonic, entropy));
        BOOST_CHECK_EQUAL(entropy.size(), bip39::ENTROPY_DEFAULT_BYTES);

        SecureString again;
        BOOST_CHECK(bip39::MnemonicFromEntropy(entropy, again));
        BOOST_CHECK(again == mnemonic);
    }

    // Entropy lengths outside BIP-39, or not a multiple of 4, are refused.
    SecureString unused;
    BOOST_CHECK(!bip39::GenerateMnemonic(15, unused));
    BOOST_CHECK(!bip39::GenerateMnemonic(18, unused));
    BOOST_CHECK(!bip39::GenerateMnemonic(33, unused));
    BOOST_CHECK(!bip39::GenerateMnemonic(0, unused));
}

BOOST_AUTO_TEST_CASE(invalid_mnemonics_are_rejected)
{
    const std::string good24 = TALER_VECTORS[0].mnemonic;

    // Last word swapped: valid words, broken checksum.
    std::string bad_checksum = good24.substr(0, good24.rfind(' ') + 1) + "zoo";
    BOOST_CHECK(!bip39::MnemonicIsValid(SecureString(bad_checksum.c_str())));

    // A word outside the list.
    std::string not_a_word = good24.substr(0, good24.rfind(' ') + 1) + "notaword";
    BOOST_CHECK(!bip39::MnemonicIsValid(SecureString(not_a_word.c_str())));

    // Wrong word counts.
    BOOST_CHECK(!bip39::MnemonicIsValid(SecureString("abandon abandon about")));
    BOOST_CHECK(!bip39::MnemonicIsValid(SecureString("")));

    // A valid 12-word phrase is a valid mnemonic, but not one this wallet creates.
    const SecureString twelve(OFFICIAL_VECTORS[0].mnemonic);
    BOOST_CHECK(bip39::MnemonicIsValid(twelve));
    BOOST_CHECK(!bip39::MnemonicIsValidForWallet(twelve));
}

//! Users retype phrases with stray spacing and capitals; the wordlist is ASCII,
//! so trimming, collapsing whitespace and lowercasing is the whole job.
BOOST_AUTO_TEST_CASE(normalization)
{
    const std::string good24 = TALER_VECTORS[1].mnemonic;

    std::string shouty = good24;
    for (char& c : shouty) c = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
    const SecureString messy(("  \t" + shouty.substr(0, shouty.find(' ')) + "   " +
                              shouty.substr(shouty.find(' ') + 1) + "\n").c_str());

    BOOST_CHECK(bip39::MnemonicIsValidForWallet(messy));
    BOOST_CHECK_EQUAL(std::string(bip39::NormalizeMnemonic(messy).c_str()), good24);

    // Normalisation must not change the derived seed.
    BOOST_CHECK_EQUAL(SeedHex(messy, SecureString("")), std::string(TALER_VECTORS[1].seed_hex));
}

BOOST_AUTO_TEST_SUITE_END()
