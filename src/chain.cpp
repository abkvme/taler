// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2023 Uladzimir (t.me/cryptadev)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <validation.h>
#include <util.h>
#include <txdb.h>

void CBlockIndex::SetStakeModifier (uint64_t nStakeModifier, bool fGeneratedStakeModifier) {
    if (fGeneratedStakeModifier)
        nStatus |= 0x00020000; //nFlags |= BLOCK_STAKE_MODIFIER;
    if (fGeneratedStakeModifier)
        pblocktree->WriteStakeModifier (*phashBlock, nStakeModifier);
}

uint64_t CBlockIndex::GetStakeModifier () const {
    const CBlockIndex* index = this;
    while (index->pprev && !GeneratedStakeModifier()) index = index->pprev;
    uint64_t ret = 0;
    static std::map<uint256, std::pair<uint64_t, uint64_t> > cache;
    static uint64_t cacheIndex = 1;
    const uint256& hash = index->GetBlockHash();
    if (cache.count(hash) > 0) {
        auto& item = cache[hash];
        item.second = cacheIndex++;
        return item.first;
    }
    pblocktree->ReadStakeModifier (hash, ret);
    if (cache.size() > 1000) {
        uint64_t cIndex = cacheIndex - 300;
        auto it1 = cache.begin();
        while (it1 != cache.end()) {
            if (it1->second.second > cIndex) { ++it1; } else { it1 = cache.erase (it1); }
        }
    }
    cache[hash] = std::make_pair(ret, cacheIndex++);
    return ret;
}

void CBlockIndex::SetStakeHash (uint256 hashProofOfStake) {
    if (IsProofOfStake()) pblocktree->WriteStakeHash (*phashBlock, hashProofOfStake);
}

uint256 CBlockIndex::GetStakeHash () const {
    uint256 ret = {};
    if (!IsProofOfStake()) return ret;
    static std::map<uint256, std::pair<uint256, uint64_t> > cache;
    static uint64_t cacheIndex = 1;
    const uint256& hash = *phashBlock;
    if (cache.count(hash) > 0) {
        auto& item = cache[hash];
        item.second = cacheIndex++;
        return item.first;
    }
    pblocktree->ReadStakeHash (hash, ret);
    if (cache.size() > 1000) {
        uint64_t cIndex = cacheIndex - 300;
        auto it1 = cache.begin();
        while (it1 != cache.end()) {
            if (it1->second.second > cIndex) { ++it1; } else { it1 = cache.erase (it1); }
        }
    }
    cache[hash] = std::make_pair(ret, cacheIndex++);
    return ret;    
}

/**
 * CChain implementation
 */

CBlockLocator CChain::GetLocator(const CBlockIndex *pindex) const {
    int nStep = 1;
    std::vector<uint256> vHave;
    vHave.reserve(32);

    if (!pindex)
        pindex = Tip();
    while (pindex) {
        vHave.push_back(pindex->GetBlockHash());
        // Stop when we have added the genesis block.
        if (pindex->nHeight == 0)
            break;
        // Exponentially larger steps back, plus the genesis block.
        int nHeight = std::max(pindex->nHeight - nStep, 0);
        pindex = pindex->GetAncestor(nHeight);
        if (vHave.size() > 10)
            nStep *= 2;
    }

    return CBlockLocator(vHave);
}

const CBlockIndex *CChain::FindFork(const CBlockIndex *pindex) const {
    if (pindex == nullptr) {
        return nullptr;
    }
    if (pindex->nHeight > Height())
        pindex = pindex->GetAncestor(Height());
    while (pindex && !Contains(pindex))
        pindex = pindex->pprev;
    return pindex;
}

CBlockIndex* CChain::FindFork(const CBlockLocator& locator) const {
    // Find the latest block common to locator and chain - we expect that
    // locator.vHave is sorted descending by height.
    for (const uint256& hash : locator.vHave) {
        CBlockIndex* pindex = LookupBlockIndex(hash);
        if (pindex) {
            if (Contains(pindex))
                return pindex;
            if (pindex->GetAncestor(Height()) == pBestBlock) {
                return pBestBlock;
            }
        }
    }
    return Genesis();
}

CBlockIndex* CChain::FindEarliestAtLeast(int64_t nTime) const
{
    if ((Genesis() == nullptr) || (Genesis()->GetBlockTime() > nTime)) return nullptr;
    if (Tip()->GetBlockTime() < nTime) return nullptr;
    // Lower bound over block times. The low end must move past the midpoint: with
    // loHeight = mHeight the search stops making progress as soon as the window
    // narrows to two blocks and the lower one is older than nTime, and spins there
    // forever - which is every call that does not land exactly on the first block.
    //
    // Block times are only required to exceed the median of the last eleven, so a
    // single block may sit slightly earlier than its parent. Every caller applies a
    // TIMESTAMP_WINDOW of slack before calling, which covers that.
    int loHeight = 0;
    int hiHeight = Height();
    while (loHeight < hiHeight) {
        int mHeight = loHeight + (hiHeight - loHeight) / 2;
        if ((*this)[mHeight]->GetBlockTime() < nTime) { loHeight = mHeight + 1; } else { hiHeight = mHeight; }
    }
    return (*this)[hiHeight];
}

/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

/** Compute what height to jump back to with the CBlockIndex::pskip pointer. */
int static inline GetSkipHeight(int height) {
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}

const CBlockIndex* CBlockIndex::GetAncestor(int height) const
{
    if (height > nHeight || height < 0) {
        return nullptr;
    }

    const CBlockIndex* pindexWalk = this;
    int heightWalk = nHeight;
    while (heightWalk > height) {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (pindexWalk->pskip != nullptr &&
            (heightSkip == height ||
             (heightSkip > height && !(heightSkipPrev < heightSkip - 2 &&
                                       heightSkipPrev >= height)))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexWalk = pindexWalk->pskip;
            heightWalk = heightSkip;
        } else {
            assert(pindexWalk->pprev);
            pindexWalk = pindexWalk->pprev;
            heightWalk--;
        }
    }
    return pindexWalk;
}

CBlockIndex* CBlockIndex::GetAncestor(int height)
{
    return const_cast<CBlockIndex*>(static_cast<const CBlockIndex*>(this)->GetAncestor(height));
}

void CBlockIndex::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}

arith_uint256 GetBlockProof(const CBlockIndex& block)
{
    arith_uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for an arith_uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (bnTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}

int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.nChainWork() > from.nChainWork()) {
        r = to.nChainWork() - from.nChainWork();
    } else {
        r = from.nChainWork() - to.nChainWork();
        sign = -1;
    }
    r = r * arith_uint256(params.nPowTargetSpacing) / GetBlockProof(tip);
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.GetLow64();
}

/** Find the last common ancestor two blocks have.
 *  Both pa and pb must be non-nullptr. */
const CBlockIndex* LastCommonAncestor(const CBlockIndex* pa, const CBlockIndex* pb) {
    if (pa->nHeight > pb->nHeight) {
        pa = pa->GetAncestor(pb->nHeight);
    } else if (pb->nHeight > pa->nHeight) {
        pb = pb->GetAncestor(pa->nHeight);
    }

    while (pa != pb && pa && pb) {
        pa = pa->pprev;
        pb = pb->pprev;
    }

    // Eventually all chain branches meet at the genesis block.
    assert(pa == pb);
    return pa;
}
