// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2023 Uladzimir (https://t.me/cryptadev)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>
#include <uint256.h>
#include <arith_uint256.h>

#include <assert.h>

#include <chainparamsseeds.h>

static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward) {
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) <<
        ParseHex("54616c6572207065727368616a612062656c617275736b616a61206b727970746176616c697574612062792044656e6973204c2069205365726765204c20");
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey
        << ParseHex("04f360606cf909ce34d4276ce40a5dd6a844a4a72473086e0fc635f3c4195d77df513b7541dc5f6f6d01ec39e4b729893c6d42dd5e248379a32b5259f38f6bfbae") 
        << OP_CHECKSIG;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 210000 * 5;
        consensus.BIP34Height = 0;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 32256;
        consensus.BIP66Height = 32256;
        consensus.CSVHeight = 32256;
        consensus.WitnessHeight = 32256;

        consensus.powLimit = (~arith_uint256 (0)) >> 8;
        consensus.powLimitLegacy = (~arith_uint256 (0)) >> 20;
        consensus.nPowTargetTimespan = 10 * 60;
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nPosTargetTimespan = 14 * 24 * 60 * 60;       // two weeks
        consensus.nPosTargetSpacing = 60 * 7 / 3;
        consensus.nCoinAgeTick = 60 * 60 * 24;
        consensus.nStakeMinAge = 60 * 60 * 24 * 2;              // minimum age for coin age  
        consensus.nStakeMaxAge = 60 * 60 * 24 * 90;             // stake age of full weight
        consensus.nStakeModifierInterval = 6 * 60 * 60;         // time to elapse before new modifier is computed
        consensus.posLimit = (~arith_uint256 (0)) >> 32;

        consensus.nLyra2ZHeight = 10000;
        consensus.nPowAveragingWindowv1 = 24;
        consensus.TLRHeight = 130000;
        consensus.nNewDiffAdjustmentAlgorithmHeight = 250000;
        consensus.nPowAveragingWindowv2 = 120;
        
        // The best chain should have at least this much work.
        consensus.nMinimumChainWork =
            uint256S("0x000000000000000000000000000000000000000000000000044082fb8920e95e"); // 2500000

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid =
            uint256S("0xe0190589986f5ae78585bb5307a910183ad379da3327e9dfd215eb3bc956ce3a"); // 2500000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x64;
        pchMessageStart[1] = 0xb1;
        pchMessageStart[2] = 0x73;
        pchMessageStart[3] = 0xd8;
        nDefaultPort = 23153;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock (1505338813, 725170, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("0xc079fd1ae86223e1522928776899d46e329da7919ca1e11be23643c67dd05d5f"));
        assert(genesis.hashMerkleRoot ==
               uint256S("0x985fae483ebbef9cde04a259282cb7465d52bf56824caf1a8132395e90488b12"));
        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.clear();
        vSeeds.emplace_back("talerseed01.taler.tech");
        vSeeds.emplace_back("talerseed02.taler.tech");
        vSeeds.emplace_back("dnsseed.talercrypto.com");
        vSeeds.emplace_back("talerseed1.vovanchik.net");
        vSeeds.emplace_back("talerseed2.vovanchik.net");
        vSeeds.emplace_back("talerseed3.vovanchik.net");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 65);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 50);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 193);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "tlr";

        vFixedSeeds.clear();
        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                {2048, uint256S("0xc4838cab89b16915d813f424198a999af82b3dce2afed5d82cab1fe9df08d701")},
                {20000, uint256S("0x82ad64f451be0375683efbdc7d94c1b970431b02a6a3e5057dd6cd0fb2022e70")},
                {100000, uint256S("0xa6c3e93e8ac7b4af077a78c6ce27a1b2b8b7793a7737403bcb9e6f420a928547")},
                {250000, uint256S("0xe599ff322e9e285b524f2bcd7617a4a0a1b4a8d4d0f5279ce9c706d1ca3036b7")},
                {500000, uint256S("0xdbd781e1a5c96e38c6f37e85ddc79f808696ff38a107334b1d2aa0d1f3c54886")},
                {728634, uint256S("0x33e82f201a0b4074af53080d26c4092e6284bdead512b8c189b9c53526078d77")},
                {734864, uint256S("0x11367f424327987636e48a1d8d81b34d92c397a48171a53ead697ac3037ec6f3")},
                {1000000, uint256S("0x76928d1d18bc7e68fba6fdb66ad6145ee7312c98de76d95e353afc5d34e94750")},
                {1500000, uint256S("0x514039919915738142da223ff83f131f0f7ac75c123d6ffa0cd9c23e40aeb328")},
                {2500000, uint256S("0xe0190589986f5ae78585bb5307a910183ad379da3327e9dfd215eb3bc956ce3a")},
            }
        };

        /* disable fallback fee on mainnet */
        m_fallback_fee_enabled = false;
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 20000;
        consensus.BIP34Height = 0;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 5;
        consensus.BIP66Height = 5;
        consensus.CSVHeight = 5;
        consensus.WitnessHeight = 5;

        consensus.powLimit = (~arith_uint256 (0)) >> 16;
        consensus.powLimitLegacy = (~arith_uint256 (0)) >> 16;
        consensus.nPowTargetTimespan = 30 * 60;
        consensus.nPowTargetSpacing = 0.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nPosTargetTimespan = 14 * 24 * 60 * 60;       // two weeks
        consensus.nPosTargetSpacing = 60 * 7 / 3;
        consensus.nCoinAgeTick = 10 * 60;
        consensus.nStakeMinAge = 10 * 60 * 2;                   // minimum age for coin age  
        consensus.nStakeMaxAge = 10 * 60 * 90;                  // stake age of full weight
        consensus.nStakeModifierInterval = 6 * 60;              // time to elapse before new modifier is computed
        consensus.posLimit = (~arith_uint256 (0)) >> 24;

        consensus.nLyra2ZHeight = 10;
        consensus.nPowAveragingWindowv1 = 5;
        consensus.TLRHeight = 20;
        consensus.nNewDiffAdjustmentAlgorithmHeight = 21000;
        consensus.nPowAveragingWindowv2 = 120;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfd;
        pchMessageStart[1] = 0xd2;
        pchMessageStart[2] = 0xc8;
        pchMessageStart[3] = 0x07;
        nDefaultPort = 18333;
        nPruneAfterHeight = 10000;
 
        genesis = CreateGenesisBlock (1505338823, 61468, 0x1f00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("0xf3b7fe4392b75efcaeb9fe5a617912a763572c69d088896bfe3cf796b1a6a866"));
        assert(genesis.hashMerkleRoot ==
               uint256S("0x985fae483ebbef9cde04a259282cb7465d52bf56824caf1a8132395e90488b12")); 

        vSeeds.clear();
        vSeeds.emplace_back("talerseed1.vovanchik.net");
        vSeeds.emplace_back("talerseed2.vovanchik.net");
        vSeeds.emplace_back("talerseed3.vovanchik.net");
        vFixedSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 196);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94}; 
        
        bech32_hrp = "test";

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                {0, uint256S("0xf3b7fe4392b75efcaeb9fe5a617912a763572c69d088896bfe3cf796b1a6a866")},
            }
        };

        /* enable fallback fee on testnet */
        m_fallback_fee_enabled = true;
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.CSVHeight = 1400;
        consensus.WitnessHeight = 1400;
        consensus.powLimit = (~arith_uint256 (0)) >> 4;
        consensus.powLimitLegacy = (~arith_uint256 (0)) >> 16;
        consensus.posLimit = (~arith_uint256 (0)) >> 8;
        consensus.nPowTargetTimespan = 24 * 60 * 60;
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18444;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1296688602, 2, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")},
            }
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bcrt";

        /* enable fallback fee on regtest */
        m_fallback_fee_enabled = true;
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
