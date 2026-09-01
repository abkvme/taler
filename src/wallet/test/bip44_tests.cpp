// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/bip44.h>

#include <chainparams.h>
#include <chainparamsbase.h>
#include <key_io.h>
#include <test/test_bitcoin.h>
#include <wallet/bip39.h>

#include <string>

#include <boost/test/unit_test.hpp>

/**
 * Cross-implementation derivation tests.
 *
 * Every value below is copied from the published vectors that all Taler wallet
 * implementations verify against:
 *
 *     https://github.com/abkvme/taler.spec
 *     vectors/seed-derivation-v1.json
 *
 * If this suite fails, the node and the mobile wallet no longer derive the same
 * addresses from the same recovery phrase, which is the one thing the shared
 * spec exists to prevent.
 */

BOOST_FIXTURE_TEST_SUITE(bip44_tests, BasicTestingSetup)

namespace {

struct Entry {
    uint32_t index;
    const char* address;
    const char* wif;
};

struct NetworkCase {
    //! Chain id as a literal rather than CBaseChainParams::MAIN etc: those are
    //! std::string objects in another translation unit, so copying them into a
    //! static table here would depend on static initialisation order.
    const char* chain;
    uint32_t coin_type;        //!< expected BIP44CoinType() for that chain
    const char* account_path;
    const char* account_xpub;
    Entry external[3];
    Entry internal[3];
};

struct PhraseCase {
    const char* mnemonic;
    NetworkCase networks[2];
};

const PhraseCase VECTORS[] = {
    // vectors/seed-derivation-v1.json, cases[0] - entropy 00 * 32
    {"abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art",
     {
         {"main", 1524, "m/44'/1524'/0'",
          "xpub6DTxLvQZAtZFTKExqFZ2BkXwnoi7xMWLPC8DZVmSiiZUXFf1euC8ePXfmYtqCKQAgzZtT4CwEPSVXLSNjiEmsSdbkSsJ3S42QHEHBuMGTz8",
          {{0, "TWGfrdx6ZnJu4wVXijA6Jq759vZKyKjpUZ", "VhSs4DpfEtDEKYdsqk1HviPg9Tx2NmzA9p2mLpm7KrnvsNpuPFxF"},
           {1, "TX9M8jH2tCKuzutY3GDA6T3gpvYdYnwf1a", "VfPNvbJP3bM3PnAx4BZTRAAshuXur3kKvvNyzfaaKAHBNjRX7g8Q"},
           {2, "TUXE1m2e2EGLbn2UoByB6D12bALHpfz9Xx", "VeRgFa1yx9ZWxYFuSmCMdgNrtcxemrhozmSykp99HmGKdorHW3NK"}},
          {{0, "TGfxTdCx7EJC7TaiLv7BiFdurtbJazqP1W", "VcGgbDyrnwx6YVjurqso6kgcEDhXRxiRQbTHtXufc7qkoVz8QZjH"},
           {1, "TS1MVea7NsQdqJmugdKk3Pg3UaNsWt9542", "VZLbFtSDS7hubNX4TDuxA7ifJeiqvdYLdnNf9QSt7vqBXAr7hpZ2"},
           {2, "TF19pkvihM7VwMtg3u9ACGBbEdvGcT1fR8", "Vc9pe9bVAegipWeX9oLbF3eEfDAkANSBGLLZTXaHqeUETP1LycJd"}}},
         {"regtest", 1, "m/44'/1'/0'",
          "tpubDChpT2EEzneiFnhLNM8cTK48EJLpW3uNaARZmipHYQAdCMza8M5ZMoiY7MKv9iRC4dTTGdfZXM4dvj92XDQNKuJLXFzSWStmKo1MAYoZEE6",
          {{0, "mhg4fv8wumcnGm2Ba2Vqx76GEkeT6mYrqk", "cNAk3XjS6aqv93G9wd4yp15ZfKevzoW26xEiimcAchWxdNRmytYX"},
           {1, "n1B9XaLqorEAJypBJxLbFW69LBHodyKNEC", "cVHXrQMCVXhh2rJAdaLKQ5wUKYnLcAaZPnqvd6iYcKi8kwVAA3gr"},
           {2, "mwFLniSHTmFY5jQocfYPGM94bduWuxNU7y", "cPoTZBg2nynULxsgRYuxsZDf4AtZ17J1zbPPzTdmwkRAVFWLvW6z"}},
          {{0, "mkCR3R7VsY66rGSRvQG7zWoQ6NhFiEAftA", "cVqdfGfwzrSFF56cYbbMzWJfRydcJVViSN9sNRamzqxDXYjdW9Vy"},
           {1, "mjmE4KwLDykVFE2L3u7ESMs4Vo67KFJQ39", "cTfCzfhAZgKX7KRjUZZ1fgoS4KYC39aoDPr7FLvqCMZ43J3eNnL7"},
           {2, "msoY8Z9hBp7BN4ixp4Q1je4i12L1j14tCR", "cPCmXuzCnPmmicJWDQ8kK8EPX5kveFNWSLcstUqw7QSL4V8QnBCm"}}},
     }},
    // vectors/seed-derivation-v1.json, cases[3]
    {"army van defense carry jealous true garbage claim echo media make crowd ring stick cluster select borrow suggest also destroy velvet awesome mass involve",
     {
         {"main", 1524, "m/44'/1524'/0'",
          "xpub6BhWKBvi9Qswpuhytgc2Q321nkPLuhW1GKNXzQM5wC3BWLpNWg6B49KdaXqqMoWYii7bTd64dMPPZk3jPpBHNLS3DRVDzxoj3KQQjS5aVFB",
          {{0, "TKT5dkX21hmvz3DSWDJjdggKFiV4xPP7Cj", "VgyM9nbpg8qo1SMBwj65eY9oYXo9VVa6Do3vUvvK1a6YZkUZufpq"},
           {1, "TRemNFTw1RhLHCKgvjCbJecLwBQEHy6oFk", "VgzVnVe1QQkMoL5a8vVKdhKTe5RaSv7oSp6dtCWNHVkWaaQXqLAo"},
           {2, "TV1cMstdTKtkRNypNNSDMdcFKzrn4bb5Ai", "VaKjRJVjXoEGb8PSNqjiRxyoji8paBVpfDT73MxxkBdmqZvd9dsC"}},
          {{0, "TStqtHq4U8gXNoeJQLwbBb72qHTxdDjNia", "VdcRSoQ7MP16V763zzw8PVZETgB2wxgiW2FP8DXRF3eyP7gGHm51"},
           {1, "TJQmTrUpJ1LonhTtPzLidkT5YuZpZtXFiK", "VgdRFzDcQ9YtNdhPizfTofTtbet3Az3SSQ2LvE7wHuAtpPvg4z5X"},
           {2, "TXUJ6LZ25msBMs8mDmLNGfLoEaTq8pdS6h", "VgrUPL1QTCBMAvgmMhGgSjTSSj3nFKM9A7UNJGKirJx7Puvz9K3L"}}},
         {"regtest", 1, "m/44'/1'/0'",
          "tpubDCPGBR1NbomUmprvf4dfoj5zy9PF59NT8geTgxWqnxzXm9QNoisiT4oQXTxY4x7fRccpwpzCx6mrTGWTPn7HmLknMfzFkLPaJBsnhvpiwuu",
          {{0, "mg58AGVwiX9ttymi1HgGaRBm4REnATEiMS", "cVTQ51XjcFYUyZM7EdNKamdgyY15wt1iKPfCGsB5A7mD23o9gcCV"},
           {1, "mwrjEjR7erEUcroEJDubNbWUJT9pcpoQDi", "cMwLk5s5m1C64MHza2ZmwPaWgF2bA34ddeKCCNkLE2tksM8D47bP"},
           {2, "n3SpEE4nmqWYmRTqjS6sEwLaxwkeKrSvgu", "cStzH4zjSfsx7yPeS3DbDHCXgCs4kDK6zvxv3T5ZnS8ETanj2rzJ"}},
          {{0, "muhyyKb6PeVDRorPhYxJ9gYahzkCN6fi3A", "cVhwBgZUZZHRoALzR4WNscJdLbgwTxr2BdSP5izRLy5WncY5PY2J"},
           {1, "n39Juom6dc4S3aKKWxpTYZp1xsBcV6apmX", "cSrTJArHhxStToBQqJkxMGuwenD9ec9XPin2t2H5AP6Vv2iREusX"},
           {2, "miTHJHpgkxumq7E1ScaN78mBRFaArfPrJt", "cQAFZFhVj5LFrVfhKXgr6NJihgnBYc9bpWHK3ZED9gpR2Wm7awnw"}}},
     }},
};

CExtKey AccountKeyFor(const char* mnemonic, uint32_t coin_type)
{
    unsigned char seed[bip39::SEED_BYTES];
    bip39::MnemonicToSeed(SecureString(mnemonic), SecureString(""), seed);

    CExtKey master;
    bip44::MasterKeyFromSeed(seed, sizeof(seed), master);

    CExtKey account;
    BOOST_REQUIRE(bip44::DeriveAccount(master, coin_type, bip44::DEFAULT_ACCOUNT, account));
    return account;
}

void CheckChain(const CExtKey& account, uint32_t chain, const Entry (&expected)[3])
{
    for (const Entry& e : expected) {
        CExtKey child;
        BOOST_REQUIRE(bip44::DeriveChild(account, chain, e.index, child));
        BOOST_CHECK_EQUAL(EncodeDestination(child.key.GetPubKey().GetID()), std::string(e.address));
        BOOST_CHECK_EQUAL(EncodeSecret(child.key), std::string(e.wif));
    }
}

} // namespace

//! The chain parameters must carry the registered coin type, or every wallet
//! created on that network derives from the wrong branch.
BOOST_AUTO_TEST_CASE(coin_type_per_network)
{
    SelectParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(Params().BIP44CoinType(), 1524U);
    SelectParams(CBaseChainParams::TESTNET);
    BOOST_CHECK_EQUAL(Params().BIP44CoinType(), 1U);
    SelectParams(CBaseChainParams::REGTEST);
    BOOST_CHECK_EQUAL(Params().BIP44CoinType(), 1U);
    SelectParams(CBaseChainParams::MAIN);
}

//! The whole point: the same phrase must produce the same addresses here as in
//! every other Taler wallet.
BOOST_AUTO_TEST_CASE(shared_vectors)
{
    for (const PhraseCase& phrase : VECTORS) {
        for (const NetworkCase& net : phrase.networks) {
            SelectParams(net.chain);
            BOOST_CHECK_EQUAL(Params().BIP44CoinType(), net.coin_type);

            const CExtKey account = AccountKeyFor(phrase.mnemonic, net.coin_type);
            BOOST_CHECK_EQUAL(EncodeExtPubKey(account.Neuter()), std::string(net.account_xpub));
            BOOST_CHECK_EQUAL(bip44::FormatAccountPath(net.coin_type, bip44::DEFAULT_ACCOUNT),
                              std::string(net.account_path));

            CheckChain(account, bip44::CHAIN_EXTERNAL, net.external);
            CheckChain(account, bip44::CHAIN_INTERNAL, net.internal);
        }
    }
    SelectParams(CBaseChainParams::MAIN);
}

//! An account xpub must be able to derive receive addresses by itself - that is
//! why change and index are non-hardened, and it is what the hot-wallet API
//! depends on.
BOOST_AUTO_TEST_CASE(xpub_derives_the_same_addresses)
{
    SelectParams(CBaseChainParams::MAIN);
    const NetworkCase& net = VECTORS[0].networks[0];
    const CExtKey account = AccountKeyFor(VECTORS[0].mnemonic, net.coin_type);
    const CExtPubKey account_pub = account.Neuter();

    CExtPubKey chain_pub;
    BOOST_REQUIRE(account_pub.Derive(chain_pub, bip44::CHAIN_EXTERNAL));
    for (const Entry& e : net.external) {
        CExtPubKey child_pub;
        BOOST_REQUIRE(chain_pub.Derive(child_pub, e.index));
        BOOST_CHECK_EQUAL(EncodeDestination(child_pub.pubkey.GetID()), std::string(e.address));
    }
}

//! Hardened levels must refuse to derive from a public key, and DeriveChild()
//! must refuse hardened arguments outright.
BOOST_AUTO_TEST_CASE(hardening_boundaries)
{
    SelectParams(CBaseChainParams::MAIN);
    const CExtKey account = AccountKeyFor(VECTORS[0].mnemonic, 1524);

    // DeriveChild must reject a hardened index before it ever reaches
    // CExtPubKey::Derive, which asserts on one rather than returning false.
    CExtKey unused;
    BOOST_CHECK(!bip44::DeriveChild(account, bip44::CHAIN_EXTERNAL, 0x80000000, unused));
    BOOST_CHECK(!bip44::DeriveChild(account, 0x80000000, 0, unused));
}

BOOST_AUTO_TEST_CASE(path_formatting)
{
    BOOST_CHECK_EQUAL(bip44::FormatAccountPath(1524, 0), "m/44'/1524'/0'");
    BOOST_CHECK_EQUAL(bip44::FormatPath(1524, 0, 0, 5), "m/44'/1524'/0'/0/5");
    BOOST_CHECK_EQUAL(bip44::FormatPath(1, 0, 1, 999), "m/44'/1'/0'/1/999");
}

BOOST_AUTO_TEST_SUITE_END()
