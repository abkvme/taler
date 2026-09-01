// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019-2021 Uladzimir (https://t.me/vovanchik_net)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>

//pos: find last block index up to pindex
const CBlockIndex *GetLastBlockIndex(const CBlockIndex *pindex, const Consensus::Params &params, bool fProofOfStake) {
    if (fProofOfStake) {
        while (pindex && pindex->pprev && !pindex->IsProofOfStake()) {
            if (params.forkNumber(pindex->nHeight) < 2) 
                return nullptr;
            pindex = pindex->pprev;
        }
    } else {
        while (pindex && pindex->pprev && pindex->IsProofOfStake())
            pindex = pindex->pprev;
    }

    return pindex;
}

uint32_t
GetNextWorkRequiredForPos(const CBlockIndex *pindexLast, const CBlockHeader *pblock, const Consensus::Params &params) {
    assert(params.forkNumber(pindexLast->nHeight + 1) >= 2);

    const CBlockIndex *pindexPrev = GetLastBlockIndex(pindexLast, params, true);
    if (pindexPrev == nullptr)
        return params.posLimit.GetCompact();

    const CBlockIndex *pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, params, true);
    if (pindexPrevPrev == nullptr || pindexPrevPrev->pprev == nullptr)
        return params.posLimit.GetCompact();

    const int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();
    const int64_t nInterval = params.DifficultyAdjustmentIntervalPos();
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    bnNew *= ((nInterval - 1) * params.nPosTargetSpacing + nActualSpacing + nActualSpacing);
    //overflow fix
    if((pindexLast->nHeight + 1)>=730000)
    {
        arith_uint256 powLimit = UintToArith256(uint256S("dfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
        if (bnNew > powLimit)
            bnNew = powLimit;
    }
    //    
    bnNew /= ((nInterval + 1) * params.nPosTargetSpacing);
    if (bnNew > params.powLimit) bnNew = params.powLimit;
    return bnNew.GetCompact();
}

unsigned int
GetNextWorkRequiredBTC(const CBlockIndex *pindexLast, const CBlockHeader *pblock, const Consensus::Params &params) {
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = params.powLimitLegacy.GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight + 1) % params.DifficultyAdjustmentIntervalPow(pindexLast->nHeight + 1) != 0) {
        if (params.fPowAllowMinDifficultyBlocks) {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() +
                                         params.nPowTargetSpacing * 5 * 2)
                return nProofOfWorkLimit;
            else {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex *pindex = pindexLast;
                while (pindex->pprev &&
                       pindex->nHeight % params.DifficultyAdjustmentIntervalPow(pindex->nHeight) != 0 &&
                       pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    // Taler: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int64_t blockstogoback = params.DifficultyAdjustmentIntervalPow(pindexLast->nHeight + 1) - 1;
    if ((pindexLast->nHeight + 1) != params.DifficultyAdjustmentIntervalPow(pindexLast->nHeight + 1))
        blockstogoback = params.DifficultyAdjustmentIntervalPow(pindexLast->nHeight + 1);

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex *pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;

    assert(pindexFirst);

    if (params.fPowNoRetargeting) return pindexLast->nBits;
    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    if (nActualTimespan < params.nPowTargetTimespan / 4) nActualTimespan = params.nPowTargetTimespan / 4;
    if (nActualTimespan > params.nPowTargetTimespan * 4) nActualTimespan = params.nPowTargetTimespan * 4;
    // Retarget
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bool fShift = bnNew.bits() > params.powLimitLegacy.bits() - 1;
    if (fShift) bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;
    if (fShift) bnNew <<= 1;
    if (bnNew > params.powLimitLegacy) bnNew = params.powLimitLegacy;
    return bnNew.GetCompact();
}

unsigned int DarkGravityWaveOld(const CBlockIndex *pindexLast, const Consensus::Params &params) {
    const CBlockIndex* pLast = pindexLast;
    int64_t nActualTimespan = 0;
    int64_t nBlocks = params.nPowAveragingWindowv1;
    arith_uint256 bnNew;
    if (pLast == nullptr || pLast->nHeight < nBlocks) return params.powLimit.GetCompact();
    for (int i = 0; i < nBlocks; i++) {
        if (i == 0) bnNew = arith_uint256().SetCompact(pLast->nBits);
        bnNew += arith_uint256().SetCompact(pLast->nBits);
        const CBlockIndex* pCur = pLast->pprev;            
        if (i < nBlocks-1) nActualTimespan += (int64_t) pLast->GetBlockTime() - pCur->GetBlockTime();
        pLast = pCur;
    }
    bnNew /= (nBlocks+1);
    bnNew -= nBlocks+1;
    int64_t nTargetTimespan = nBlocks * params.nPowTargetSpacing;
    if (pindexLast->nHeight < params.TLRHeight) nTargetTimespan *= 5;
    if (nActualTimespan < nTargetTimespan / 3) nActualTimespan = nTargetTimespan / 3;
    if (nActualTimespan > nTargetTimespan * 3) nActualTimespan = nTargetTimespan * 3;
    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    if (bnNew > params.powLimit) bnNew = params.powLimit;
    return bnNew.GetCompact();
}

unsigned int DarkGravityWave(const CBlockIndex *pindexLast, const Consensus::Params &params) {
    const CBlockIndex* pLast = GetLastBlockIndex(pindexLast, params, false);
    int64_t nActualTimespan = 0;
    int64_t nBlocks = params.nPowAveragingWindowv2;
    arith_uint256 bnNew;
    if (pLast == nullptr || pLast->nHeight < nBlocks) return params.powLimit.GetCompact();
    for (int i = 0; i < nBlocks; i++) {
        if (i == 0) bnNew = arith_uint256().SetCompact(pLast->nBits);
        bnNew += arith_uint256().SetCompact(pLast->nBits);
        const CBlockIndex* pCur = GetLastBlockIndex(pLast->pprev, params, false);            
        if (i < nBlocks-1) nActualTimespan += (int64_t) pLast->GetBlockTime() - pCur->GetBlockTime();
        pLast = pCur;
    }
    bnNew /= (nBlocks+1);
    bnNew -= nBlocks+1;
    int64_t nTargetTimespan = nBlocks * params.nPowTargetSpacing;
    if (nActualTimespan < nTargetTimespan / 3) nActualTimespan = nTargetTimespan / 3;
    if (nActualTimespan > nTargetTimespan * 3) nActualTimespan = nTargetTimespan * 3;
    // Retarget
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    if (bnNew > params.powLimit) bnNew = params.powLimit;
    return bnNew.GetCompact();
}

unsigned int
GetNextWorkRequiredForPow(const CBlockIndex *pindexLast, const CBlockHeader *pblock, const Consensus::Params &params) {
    int nNextHeight = pindexLast->nHeight + 1;

    if (nNextHeight < params.nLyra2ZHeight) {
        return GetNextWorkRequiredBTC(pindexLast, pblock, params);
    } else if (nNextHeight < params.nLyra2ZHeight + params.nPowAveragingWindowv1) {
        return params.powLimit.GetCompact();
    } else if (nNextHeight < params.nNewDiffAdjustmentAlgorithmHeight) {
        return DarkGravityWaveOld(pindexLast, params);
    } else if (nNextHeight < params.nNewDiffAdjustmentAlgorithmHeight + params.nPowAveragingWindowv2) {
        return 0x1c08b5b1;  // 29.39
    }
    return DarkGravityWave(pindexLast, params);
}

uint32_t GetNextWorkRequired(const CBlockIndex *pindexLast, const CBlockHeader *pblock, const Consensus::Params &params) {
    assert(pindexLast != nullptr);

    // Test chains hold a fixed target. Blocks there are minted as fast as they are
    // asked for, so every averaging window sees a timespan of almost zero and
    // clamps the target to a third of the previous one - difficulty triples per
    // block and the chain is unmineable within a dozen. Only regtest sets this.
    if (params.fPowNoRetargeting) return pindexLast->nBits;

    bool fProofOfStake = pblock->IsProofOfStake();
    if (params.forkNumber(pindexLast->nHeight + 1) < 3)
        return fProofOfStake ? GetNextWorkRequiredForPos(pindexLast, pblock, params) :
                               GetNextWorkRequiredForPow(pindexLast, pblock, params);
    arith_uint256 bnOld, bnNew, bnLimit = params.powLimitLegacy;
    const CBlockIndex* pPrev = pindexLast;
    while (pPrev && (pPrev->IsProofOfStake() != fProofOfStake)) pPrev = pPrev->pprev;
    int64_t srcTimes = 0;
    int destBlockCount = 12;
    if (pPrev && (pindexLast->nHeight - pPrev->nHeight > destBlockCount)) {
        bnNew = arith_uint256().SetCompact(pPrev->nBits) << 3;
        if (bnNew > bnLimit) bnNew = bnLimit;
        return bnNew.GetCompact();
    }
    for (int i = 0; i < destBlockCount; i++) {
        if (pPrev == nullptr) return bnLimit.GetCompact();
        if (i == 0) { bnOld.SetCompact (pPrev->nBits); }
        bnNew += arith_uint256().SetCompact(pPrev->nBits);
        const CBlockIndex* pPrevPrev = pPrev;
        while (pPrevPrev && (pPrevPrev->IsProofOfStake() != fProofOfStake)) pPrevPrev = pPrevPrev->pprev;
        if (pPrevPrev == nullptr) return bnLimit.GetCompact();
        srcTimes += std::max(pPrev->GetBlockTime() - pPrevPrev->GetBlockTime(), (int64_t)0);
        pPrev = pPrevPrev;
    }
    bnNew /= destBlockCount * 2 * params.nPowTargetSpacing;
    bnNew *= srcTimes;
    if (bnNew > (bnOld << 2)) bnNew = bnOld << 2;
    if (bnNew < (bnOld >> 3)) bnNew = bnOld >> 3;
    if (bnNew > bnLimit) bnNew = bnLimit;
    return bnNew.GetCompact();
}