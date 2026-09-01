// Copyright (c) 2014-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <util.h>
#include <test/test_bitcoin.h>

#include <vector>

#include <boost/test/unit_test.hpp>

#define SKIPLIST_LENGTH 300000

BOOST_FIXTURE_TEST_SUITE(skiplist_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(skiplist_test)
{
    std::vector<CBlockIndex> vIndex(SKIPLIST_LENGTH);

    for (int i=0; i<SKIPLIST_LENGTH; i++) {
        vIndex[i].nHeight = i;
        vIndex[i].pprev = (i == 0) ? nullptr : &vIndex[i - 1];
        vIndex[i].BuildSkip();
    }

    for (int i=0; i<SKIPLIST_LENGTH; i++) {
        if (i > 0) {
            BOOST_CHECK(vIndex[i].pskip == &vIndex[vIndex[i].pskip->nHeight]);
            BOOST_CHECK(vIndex[i].pskip->nHeight < i);
        } else {
            BOOST_CHECK(vIndex[i].pskip == nullptr);
        }
    }

    for (int i=0; i < 1000; i++) {
        int from = InsecureRandRange(SKIPLIST_LENGTH - 1);
        int to = InsecureRandRange(from + 1);

        BOOST_CHECK(vIndex[SKIPLIST_LENGTH - 1].GetAncestor(from) == &vIndex[from]);
        BOOST_CHECK(vIndex[from].GetAncestor(to) == &vIndex[to]);
        BOOST_CHECK(vIndex[from].GetAncestor(0) == vIndex.data());
    }
}

BOOST_AUTO_TEST_CASE(getlocator_test)
{
    // Build a main chain 100000 blocks long.
    std::vector<uint256> vHashMain(100000);
    std::vector<CBlockIndex> vBlocksMain(100000);
    for (unsigned int i=0; i<vBlocksMain.size(); i++) {
        vHashMain[i] = ArithToUint256(i); // Set the hash equal to the height, so we can quickly check the distances.
        vBlocksMain[i].nHeight = i;
        vBlocksMain[i].pprev = i ? &vBlocksMain[i - 1] : nullptr;
        vBlocksMain[i].phashBlock = &vHashMain[i];
        vBlocksMain[i].BuildSkip();
        BOOST_CHECK_EQUAL((int)UintToArith256(vBlocksMain[i].GetBlockHash()).GetLow64(), vBlocksMain[i].nHeight);
        BOOST_CHECK(vBlocksMain[i].pprev == nullptr || vBlocksMain[i].nHeight == vBlocksMain[i].pprev->nHeight + 1);
    }

    // Build a branch that splits off at block 49999, 50000 blocks long.
    std::vector<uint256> vHashSide(50000);
    std::vector<CBlockIndex> vBlocksSide(50000);
    for (unsigned int i=0; i<vBlocksSide.size(); i++) {
        vHashSide[i] = ArithToUint256(i + 50000 + (arith_uint256(1) << 128)); // Add 1<<128 to the hashes, so GetLow64() still returns the height.
        vBlocksSide[i].nHeight = i + 50000;
        vBlocksSide[i].pprev = i ? &vBlocksSide[i - 1] : (vBlocksMain.data()+49999);
        vBlocksSide[i].phashBlock = &vHashSide[i];
        vBlocksSide[i].BuildSkip();
        BOOST_CHECK_EQUAL((int)UintToArith256(vBlocksSide[i].GetBlockHash()).GetLow64(), vBlocksSide[i].nHeight);
        BOOST_CHECK(vBlocksSide[i].pprev == nullptr || vBlocksSide[i].nHeight == vBlocksSide[i].pprev->nHeight + 1);
    }

    // Build a CChain for the main branch.
    CChain chain;
    chain.SetTip(&vBlocksMain.back());

    // Test 100 random starting points for locators.
    for (int n=0; n<100; n++) {
        int r = InsecureRandRange(150000);
        CBlockIndex* tip = (r < 100000) ? &vBlocksMain[r] : &vBlocksSide[r - 100000];
        CBlockLocator locator = chain.GetLocator(tip);

        // The first result must be the block itself, the last one must be genesis.
        BOOST_CHECK(locator.vHave.front() == tip->GetBlockHash());
        BOOST_CHECK(locator.vHave.back() == vBlocksMain[0].GetBlockHash());

        // Entries 1 through 11 (inclusive) go back one step each.
        for (unsigned int i = 1; i < 12 && i < locator.vHave.size() - 1; i++) {
            BOOST_CHECK_EQUAL(UintToArith256(locator.vHave[i]).GetLow64(), tip->nHeight - i);
        }

        // The further ones (excluding the last one) go back with exponential steps.
        unsigned int dist = 2;
        for (unsigned int i = 12; i < locator.vHave.size() - 1; i++) {
            BOOST_CHECK_EQUAL(UintToArith256(locator.vHave[i - 1]).GetLow64() - UintToArith256(locator.vHave[i]).GetLow64(), dist);
            dist *= 2;
        }
    }
}

BOOST_AUTO_TEST_CASE(findearliestatleast_test)
{
    // Taler's CBlockIndex carries no nTimeMax, so FindEarliestAtLeast is a lower
    // bound over the block times themselves. Times here rise by the target spacing,
    // as they do on a healthy chain, which is the assumption the search makes.
    std::vector<uint256> vHashMain(100000);
    std::vector<CBlockIndex> vBlocksMain(100000);
    for (unsigned int i = 0; i < vBlocksMain.size(); i++) {
        vHashMain[i] = ArithToUint256(i); // Set the hash equal to the height
        vBlocksMain[i].nHeight = i;
        vBlocksMain[i].pprev = i ? &vBlocksMain[i - 1] : nullptr;
        vBlocksMain[i].phashBlock = &vHashMain[i];
        vBlocksMain[i].BuildSkip();
        vBlocksMain[i].nTime = 1500000000 + i * 140; // nPosTargetSpacing
    }

    CChain chain;
    chain.SetTip(&vBlocksMain.back());

    for (unsigned int i = 0; i < 10000; ++i) {
        const int r = InsecureRandRange(vBlocksMain.size());
        const int64_t test_time = vBlocksMain[r].nTime;

        // Asking for a block's own time returns that block, not the one after it.
        CBlockIndex* ret = chain.FindEarliestAtLeast(test_time);
        BOOST_CHECK(ret != nullptr);
        BOOST_CHECK_EQUAL(ret->nHeight, r);
        BOOST_CHECK(ret->GetBlockTime() >= test_time);
        BOOST_CHECK((ret->pprev == nullptr) || ret->pprev->GetBlockTime() < test_time);
        BOOST_CHECK(vBlocksMain[r].GetAncestor(ret->nHeight) == ret);

        // A time between two blocks returns the later one - the first block that
        // could hold a transaction made at that moment.
        CBlockIndex* between = chain.FindEarliestAtLeast(test_time - 1);
        BOOST_CHECK(between != nullptr);
        BOOST_CHECK_EQUAL(between->nHeight, r);
    }

    // Before the first block and after the last.
    BOOST_CHECK(!chain.FindEarliestAtLeast(vBlocksMain.front().nTime - 1));
    BOOST_CHECK(!chain.FindEarliestAtLeast(vBlocksMain.back().nTime + 1));
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(vBlocksMain.front().nTime)->nHeight, 0);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(vBlocksMain.back().nTime)->nHeight,
                      (int)vBlocksMain.size() - 1);
}

BOOST_AUTO_TEST_CASE(findearliestatleast_edge_test)
{
    // Repeated timestamps: the answer must be the *first* block of each group.
    // This is also the regression test for a lower bound that failed to advance
    // its low end, which spun forever once the window narrowed to two blocks.
    std::list<CBlockIndex> blocks;
    for (unsigned int time : {100, 100, 100, 200, 200, 200, 300, 300, 300}) {
        CBlockIndex* prev = blocks.empty() ? nullptr : &blocks.back();
        blocks.emplace_back();
        blocks.back().nHeight = prev ? prev->nHeight + 1 : 0;
        blocks.back().pprev = prev;
        blocks.back().BuildSkip();
        blocks.back().nTime = time;
    }

    CChain chain;
    chain.SetTip(&blocks.back());

    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(100)->nHeight, 0);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(150)->nHeight, 3);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(200)->nHeight, 3);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(250)->nHeight, 6);
    BOOST_CHECK_EQUAL(chain.FindEarliestAtLeast(300)->nHeight, 6);
    BOOST_CHECK(!chain.FindEarliestAtLeast(350));

    // Unlike upstream, a time older than the first block finds nothing rather than
    // the genesis block: callers that want the whole chain pass the genesis block
    // themselves. restorewallet relies on this and falls back to genesis.
    BOOST_CHECK(!chain.FindEarliestAtLeast(50));
    BOOST_CHECK(!chain.FindEarliestAtLeast(0));
    BOOST_CHECK(!chain.FindEarliestAtLeast(-1));
    BOOST_CHECK(!chain.FindEarliestAtLeast(std::numeric_limits<int64_t>::min()));
    BOOST_CHECK(!chain.FindEarliestAtLeast(std::numeric_limits<int64_t>::max()));
    BOOST_CHECK(!chain.FindEarliestAtLeast(std::numeric_limits<unsigned int>::max()));
}

BOOST_AUTO_TEST_SUITE_END()
