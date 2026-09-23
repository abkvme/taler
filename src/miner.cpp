// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2023 Uladzimir (t.me/cryptadev)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <miner.h>

#include <amount.h>
#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/tx_verify.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <hash.h>
#include <net.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <timedata.h>
#include <util.h>
#include <utilmoneystr.h>
#include <validationinterface.h>
#include <key_io.h>
#include <shutdown.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include <wallet/wallet.h>

// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest fee rate of a transaction combined with all
// its ancestors.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockWeight = 0;
int64_t nLastCoinStakeSearchInterval = 0;

// End of the last coin-stake search, so the next one covers only the seconds since.
// 0 until the first search, which then starts from now.
static std::atomic<int64_t> g_last_coinstake_search_time{0};

void ResetCoinStakeSearch()
{
    g_last_coinstake_search_time = GetAdjustedTime();
}

BlockAssembler::Options::Options() {
    blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
}

BlockAssembler::BlockAssembler(const CChainParams& params, const Options& options) : chainparams(params)
{
    blockMinFeeRate = options.blockMinFeeRate;
    // Limit weight to between 4K and MAX_BLOCK_WEIGHT-4K for sanity:
    nBlockMaxWeight = std::max<size_t>(4000, std::min<size_t>(MAX_BLOCK_WEIGHT - 4000, options.nBlockMaxWeight));
}

static BlockAssembler::Options DefaultOptions()
{
    // Block resource limits
    // If -blockmaxweight is not given, limit to DEFAULT_BLOCK_MAX_WEIGHT
    BlockAssembler::Options options;
    options.nBlockMaxWeight = gArgs.GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
    if (gArgs.IsArgSet("-blockmintxfee")) {
        CAmount n = 0;
        ParseMoney(gArgs.GetArg("-blockmintxfee", ""), n);
        options.blockMinFeeRate = CFeeRate(n);
    } else {
        options.blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }
    return options;
}

BlockAssembler::BlockAssembler(const CChainParams& params) : BlockAssembler(params, DefaultOptions()) {}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;
    fIncludeWitness = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, bool fMineWitnessTx)
{
    bool fPosCancel = false;
    return CreateNewBlock(scriptPubKeyIn, fMineWitnessTx, false, fPosCancel, nullptr);
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewPoSBlock(bool& fPoSCancel, std::shared_ptr<CWallet> pwallet, bool fMineWitnessTx)
{
    CScript scriptDummy = CScript() << OP_TRUE;
    return CreateNewBlock(scriptDummy, fMineWitnessTx, true, fPoSCancel, pwallet);
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, bool fMineWitnessTx, bool fAddProofOfStake, bool& fPoSCancel, std::shared_ptr<CWallet> pwallet)
{
    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK2(cs_main, mempool.cs);
    CBlockIndex* pindexPrev = chainActive.Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;
    if (chainparams.forkNumber(nHeight) >= 3) {
        pblock->SetVersion(3);
    } else if (chainparams.forkNumber(nHeight) >= 2) {
        pblock->SetNewFormatBlock();
        pblock->nVersion = 0x20000000;
    } else {
        pblock->nVersion = 0x20000000;
    }
    pblock->hashPrevBlock = pindexPrev->GetBlockHash();
    if (fAddProofOfStake) {
        if (chainparams.forkNumber(nHeight) < 2) {
            return nullptr;
        }
        pblock->SetProofOfStake();
    }
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
    pblock->nNonce = 0;

    // pos: if coinstake available add coinstake tx
    if (g_last_coinstake_search_time == 0) g_last_coinstake_search_time = GetAdjustedTime();
    int64_t nLastCoinStakeSearchTime = g_last_coinstake_search_time;
    uint32_t nCoinStakeTime;
    CAmount nPosReward;
    if (fAddProofOfStake) {
        fPoSCancel = true;
        CMutableTransaction txCoinStake;
        nCoinStakeTime = GetAdjustedTime();
        int64_t nSearchTime = nCoinStakeTime;
        if (nSearchTime > nLastCoinStakeSearchTime) {
            CBlockHeader header = pblock->GetBlockHeader();
            header.nTime = nCoinStakeTime;
            if (pwallet->CreateCoinStake(header, nSearchTime-nLastCoinStakeSearchTime, txCoinStake, nPosReward)) {
                if (header.nTime >= std::max(pindexPrev->GetMedianTimePast()+1, pindexPrev->GetBlockTime() - MAX_FUTURE_BLOCK_TIME)) {
                    pblock->vtx.push_back(MakeTransactionRef(std::move(txCoinStake)));
                    fPoSCancel = false;
                }
            }
            nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
            g_last_coinstake_search_time = nSearchTime;
            nCoinStakeTime = header.nTime;            
        }
        if (fPoSCancel)
            return nullptr;
    }

    pblock->nTime = pblock->IsProofOfStake() ? nCoinStakeTime : std::max(pindexPrev->GetBlockTime()+1, GetAdjustedTime());
    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    // Decide whether to include witness transactions
    // This is only needed in case the witness softfork activation is reverted
    // (which would require a very deep reorganization).
    // Note that the mempool would accept transactions with witness data before
    // IsWitnessEnabled, but we would only ever mine blocks after IsWitnessEnabled
    // unless there is a massive block reorganization with the witness softfork
    // not activated.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
    fIncludeWitness = IsWitnessEnabled(pindexPrev, chainparams.GetConsensus()) && fMineWitnessTx;

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated);

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockWeight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    if (pblock->IsProofOfStake()) {
        coinbaseTx.vout[0].scriptPubKey = pblock->vtx[1]->vout[1].scriptPubKey;
        coinbaseTx.vout[0].nValue = nFees + nPosReward;
    } else {
        coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
        coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(pindexPrev->nPowHeight + 1, chainparams.GetConsensus());
    }
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;
    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    if (pblock->IsProofOfStake() && pblock->IsNewestFormat())  {
        CMutableTransaction staketx(*pblock->vtx[1]);
        staketx.vout[0].nValue = 0;
        staketx.vout[0].scriptPubKey = MakeCheckStakeScript (*pblock);
        if (!pwallet->SignTransaction(staketx)) {
            LogPrintf("CreateCoinStake : failed to sign coinstake");
            return nullptr;
        }    
        pblock->vtx[1] = MakeTransactionRef(std::move(staketx));
    }
    bool fHaveWitness = false;
    for (const auto& tx : pblock->vtx) {
        if (tx->HasWitness()) { fHaveWitness = true; break; }
    }
    if (fHaveWitness)
        pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());
    pblocktemplate->vTxFees[0] = -nFees;
    if (pblock->IsNewestFormat()) pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);

    LogPrintf("CreateNewBlock(): block weight: %u txs: %u fees: %ld sigops %d\n", GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost);

    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
    }
    int64_t nTime2 = GetTimeMicros();

    LogPrint(BCLog::BENCH, "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), validity: %.2fms (total %.2fms)\n", 0.001 * (nTime1 - nTimeStart), nPackagesSelected, nDescendantsUpdated, 0.001 * (nTime2 - nTime1), 0.001 * (nTime2 - nTimeStart));

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature witness (in case segwit transactions are added to mempool before
//   segwit activation)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    for (CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && it->GetTx().HasWitness())
            return false;
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
        indexed_modified_transaction_set &mapModifiedTx)
{
    int nDescendantsUpdated = 0;
    for (CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc))
                continue;
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCostWithAncestors -= it->GetSigOpCost();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
    return nDescendantsUpdated;
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    return mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it);
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty())
    {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
                SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, sortedEntries);

        for (size_t i=0; i<sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    uint64_t nExtra;
    GetRandBytes((unsigned char*)&nExtra, sizeof(nExtra));
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << nExtra << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

#ifdef ENABLE_WALLET

// Block production is one switch for the whole node: off, staking with one
// wallet, or mining to one wallet. Staking and mining are exclusive because there
// is only one state to hold them, not because anything has to police it.
//
// Staking names its wallet rather than holding it. The supervisor looks the wallet
// up on each pass and lets go of it before sleeping, so a staking wallet can be
// unloaded like any other - holding it for the whole session made UnloadWallet
// wait forever.
//
// A state with an end time is a session: it stops when the time is up, and a
// staking session also stops when its wallet is locked, and locks the wallet when
// it ends. A state without an end time is what -stakegen and -gen start, and what
// the RPCs start without a duration: it runs until stopped, and staking waits out
// a locked wallet rather than giving up, as it always has.

static std::mutex g_generation_mutex;
static GenerationState g_generation;            // guarded by g_generation_mutex
static CScript g_mining_script;                 // guarded by g_generation_mutex
static uint64_t g_generation_session = 0;       // guarded by g_generation_mutex
static std::atomic<uint64_t> g_mining_epoch{0}; // miner threads of an older epoch stop
static std::atomic<bool> g_supervisor_started{false};
static std::atomic<bool> isready{false};
static std::atomic<int> POWTries{0x0000FFFF};

GenerationState GetGenerationState()
{
    std::lock_guard<std::mutex> lock(g_generation_mutex);
    return g_generation;
}

int MaxMiningThreads()
{
    // Leave one core to the node itself.
    const int cores = static_cast<int>(std::thread::hardware_concurrency());
    return std::max(1, cores - 1);
}

//! A staking session unlocked its wallet for a set time; once the session is over,
//! so is the unlock.
static void LockEndedSession(const GenerationState& ended)
{
    if (ended.mode != GenerationMode::STAKING || ended.until == 0) return;
    if (std::shared_ptr<CWallet> pwallet = GetWallet(ended.wallet)) {
        if (pwallet->IsCrypted() && !pwallet->IsLocked()) pwallet->Lock();
    }
}

//! End the state `session` (any state when 0). Returns the state that ended.
static GenerationState EndGeneration(uint64_t session, const std::string& reason)
{
    GenerationState ended;
    {
        std::lock_guard<std::mutex> lock(g_generation_mutex);
        if (g_generation.mode == GenerationMode::OFF) return ended;
        if (session != 0 && g_generation.session != session) return ended;
        ended = g_generation;
        g_generation = GenerationState();
        g_generation.session = ++g_generation_session;
        g_mining_script.clear();
        ++g_mining_epoch;
    }
    LogPrintf("Block production: %s for wallet \"%s\" stopped (%s)\n",
              ended.mode == GenerationMode::STAKING ? "staking" : "mining", ended.wallet, reason);

    LockEndedSession(ended);
    return ended;
}

void StopGeneration()
{
    EndGeneration(0, "stopped");
}

void StopGenerationForWallet(const std::string& wallet_name)
{
    uint64_t session = 0;
    {
        std::lock_guard<std::mutex> lock(g_generation_mutex);
        if (g_generation.mode == GenerationMode::OFF || g_generation.wallet != wallet_name) return;
        session = g_generation.session;
    }
    EndGeneration(session, "the wallet is being unloaded");
}

void StartStaking(const std::string& wallet_name, int64_t until, bool from_config)
{
    GenerationState replaced;
    {
        std::lock_guard<std::mutex> lock(g_generation_mutex);
        replaced = g_generation;
        g_generation = GenerationState();
        g_generation.mode = GenerationMode::STAKING;
        g_generation.wallet = wallet_name;
        g_generation.until = until;
        g_generation.from_config = from_config;
        g_generation.session = ++g_generation_session;
        g_mining_script.clear();
        ++g_mining_epoch;
    }
    LogPrintf("Block production: staking with wallet \"%s\"%s\n", wallet_name,
              until > 0 ? strprintf(" until %s", FormatISO8601DateTime(until)) : std::string());
    // Staking the same wallet again keeps it unlocked; it was unlocked for this.
    if (replaced.wallet != wallet_name) LockEndedSession(replaced);
}

static void POWMinerThread(uint64_t epoch, int index)
{
    LogPrintf("POWMinerThread %d started\n", index);
    RenameThread("coin-pow-miner");
    CScript script;
    {
        std::lock_guard<std::mutex> lock(g_generation_mutex);
        script = g_mining_script;
    }
    unsigned int extra = 0;
    try {
        while (!ShutdownRequested() && g_mining_epoch == epoch) {
            if (!isready) {
                if (IsInitialBlockDownload()) { MilliSleep(1000); continue; } else { isready = true; }
            }
            int64_t nTime = GetTimeMillis();
            std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewBlock(script));
            if (!pblocktemplate.get()) { MilliSleep(1000); continue; }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, chainActive.Tip(), extra);
            const int nTries = POWTries;
            int nMaxTries = nTries;
            bool fNegative, fOverflow;
            arith_uint256 bnTarget;
            bnTarget.SetCompact (pblock->nBits, &fNegative, &fOverflow);
            const Consensus::Params& consensus = Params().GetConsensus();
            int nHeight = chainActive.Height() + 1;
            // A round takes about 30 seconds, so check for a stop inside it too.
            while ((nMaxTries > 0) && g_mining_epoch == epoch && (UintToArith256(pblock->GetPoWHash(nHeight, consensus)) >= bnTarget)) {
                pblock->nNonce++;
                nMaxTries--;
            }
            if (g_mining_epoch != epoch) break;
            nTime = GetTimeMillis() - nTime; if (nTime < 1) nTime = 1;
            LogPrintf("POWMinerThread %d speed is %d kb\n", index, (nTries-nMaxTries) / nTime);
            if ((nTime < 15000) || (nTime > 45000)) {
                int tries = ((nTries-nMaxTries) / nTime) * 30000;
                if (tries < 0x00000FFF) tries = 0x00000FFF;
                POWTries = tries;
            }
            if (nMaxTries == 0) { continue; }
            if (UintToArith256(pblock->GetPoWHash(nHeight, consensus)) >= bnTarget) { continue; }
            std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
            if (!ProcessNewBlock(Params(), shared_pblock, true, nullptr))
                LogPrintf("POWMinerThread: ProcessNewBlock, block not accepted...\n");
        };
    }
    catch (const boost::thread_interrupted&) {
        LogPrintf("POWMinerThread terminated\n");
        return;
    }
    catch (const std::runtime_error &e) {
        LogPrintf("POWMinerThread runtime error: %s\n", e.what());
        return;
    } catch (...) {
        PrintExceptionContinue(NULL, "POWMinerThread()");
    }
    LogPrintf("POWMinerThread %d stopped\n", index);
}

bool StartMining(CWallet& wallet, int threads, int64_t until, bool from_config, std::string& error)
{
    threads = std::max(1, std::min(threads, MaxMiningThreads()));

    // The reward address is taken now, so the wallet can stay locked while mining.
    std::shared_ptr<CReserveScript> coinbase;
    wallet.GetScriptForMining(coinbase);
    if (!coinbase || coinbase->reserveScript.empty()) {
        error = "Keypool ran out, please call keypoolrefill first";
        return false;
    }
    coinbase->KeepScript();

    uint64_t epoch;
    GenerationState replaced;
    {
        std::lock_guard<std::mutex> lock(g_generation_mutex);
        replaced = g_generation;
        epoch = ++g_mining_epoch;
        g_mining_script = coinbase->reserveScript;
        g_generation = GenerationState();
        g_generation.mode = GenerationMode::MINING;
        g_generation.wallet = wallet.GetName();
        g_generation.until = until;
        g_generation.threads = threads;
        g_generation.from_config = from_config;
        g_generation.session = ++g_generation_session;
    }
    CTxDestination dest;
    if (ExtractDestination(coinbase->reserveScript, dest)) {
        LogPrintf("Block production: mining to %s (wallet \"%s\", %d threads)\n", EncodeDestination(dest), wallet.GetName(), threads);
    }
    LockEndedSession(replaced);
    for (int i = 1; i <= threads; i++) {
        std::thread(POWMinerThread, epoch, i).detach();
    }
    return true;
}

//! One attempt at a proof-of-stake block. Returns the seconds to wait before the next.
static int StakeOnce(const std::shared_ptr<CWallet>& pwallet, unsigned int& extra)
{
    bool fPoSCancel = false;
    std::unique_ptr<CBlockTemplate> pblocktemplate(BlockAssembler(Params()).CreateNewPoSBlock(fPoSCancel, pwallet));
    if (fPoSCancel) return 4;
    if (!pblocktemplate.get()) return 1;
    CBlock *pblock = &pblocktemplate->block;
    if (!pblock->IsNewestFormat()) IncrementExtraNonce(pblock, chainActive.Tip(), extra);
    if (!pblock->IsProofOfStake()) return 1;
    if (!pblock->IsNewestFormat()) {
        if (!SignBlock(*pblock, *pwallet)) return 1;
    }
    LogPrintf("POSMinerThread: proof-of-stake block found %s\n", pblock->GetHash().ToString());
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
    try {
        if (ProcessNewBlock(Params(), shared_pblock, true, nullptr)) return 60;
    } catch (...) {
        PrintExceptionContinue(NULL, "POSMinerThread()");
    }
    return 1;
}

static void GenerationSupervisorThread()
{
    LogPrintf("POSMinerThread started\n");
    RenameThread("coin-pos-miner");
    unsigned int extra = 0;
    std::string staking_wallet;
    while (!ShutdownRequested()) {
        const GenerationState state = GetGenerationState();
        if (state.mode == GenerationMode::OFF) { MilliSleep(1000); continue; }
        if (state.until > 0 && GetTime() >= state.until) {
            EndGeneration(state.session, "its time is up");
            continue;
        }
        if (state.mode == GenerationMode::MINING) { MilliSleep(1000); continue; }

        if (state.wallet != staking_wallet) {
            // The search window belongs to the wallet that was searched.
            ResetCoinStakeSearch();
            staking_wallet = state.wallet;
        }

        int wait = 1;
        bool locked_session = false;
        {
            std::shared_ptr<CWallet> pwallet = GetWallet(state.wallet);
            if (!pwallet) {
                wait = 5; // not loaded (yet): -stakewallet may name a wallet the GUI loads later
            } else if (!isready && IsInitialBlockDownload()) {
                wait = 1;
            } else if (pwallet->IsLocked()) {
                locked_session = state.until > 0;
                wait = 3;
            } else {
                isready = true;
                try {
                    wait = StakeOnce(pwallet, extra);
                } catch (const std::exception& e) {
                    LogPrintf("POSMinerThread: %s\n", e.what());
                    wait = 1;
                }
            }
        } // the wallet is let go here, before any sleep
        if (locked_session) {
            EndGeneration(state.session, "the wallet was locked");
            continue;
        }
        for (int i = 0; i < wait && !ShutdownRequested(); ++i) {
            MilliSleep(1000);
            if (GetGenerationState().session != state.session) break;
        }
    }
    LogPrintf("POSMinerThread stopped\n");
}

void StartGenerationSupervisor()
{
    if (g_supervisor_started.exchange(true)) return;
    std::thread(GenerationSupervisorThread).detach();
}

#endif // ENABLE_WALLET
