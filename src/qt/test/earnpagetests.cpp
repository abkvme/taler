// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/earnpagetests.h>

#include <amount.h>
#include <qt/earnpage.h>

namespace {

using Mode = interfaces::NodeGeneration::Mode;

interfaces::NodeGeneration generation(Mode mode, const std::string& wallet = std::string())
{
    interfaces::NodeGeneration result;
    result.mode = mode;
    result.wallet = wallet;
    return result;
}

interfaces::WalletStakeEligibility eligibility(CAmount stakeable, CAmount maturing = 0, int64_t next = 0)
{
    interfaces::WalletStakeEligibility result;
    result.stakeable = stakeable;
    result.maturing = maturing;
    result.next_stakeable_time = next;
    return result;
}

EarnPage::Blocker stake(bool syncing, bool encrypted, const interfaces::NodeGeneration& gen,
                        const interfaces::WalletStakeEligibility& coins, const std::string& wallet = "a")
{
    return EarnPage::stakingBlocker(syncing, encrypted, gen, wallet, coins, "2 days", "10 TLR");
}

} // namespace

void EarnPageTests::stakingAllowed()
{
    const EarnPage::Blocker blocker = stake(false, true, generation(Mode::OFF), eligibility(50 * COIN));
    QVERIFY(blocker.reason.isEmpty());
    QVERIFY(!blocker.has_go_to);
}

void EarnPageTests::stakingReasonOrder()
{
    // Everything wrong at once: syncing is what the user has to wait out first.
    const interfaces::WalletStakeEligibility none = eligibility(0);
    QVERIFY(stake(true, false, generation(Mode::MINING, "b"), none).reason.contains("syncing"));
    // Then encryption, before anything about other wallets or coins.
    QVERIFY(stake(false, false, generation(Mode::MINING, "b"), none).reason.contains("Encrypt"));
    // Then what the node is busy with, before coins.
    QVERIFY(stake(false, true, generation(Mode::MINING, "b"), none).reason.contains("mining"));
    QVERIFY(stake(false, true, generation(Mode::MINING, "a"), none).reason.contains("This wallet is mining"));
    QVERIFY(!stake(false, true, generation(Mode::MINING, "a"), none).has_go_to);
}

void EarnPageTests::stakingCoins()
{
    QVERIFY(stake(false, true, generation(Mode::OFF), eligibility(0)).reason.contains("No coins"));
    const EarnPage::Blocker young = stake(false, true, generation(Mode::OFF), eligibility(0, 20 * COIN, 1735689600));
    QVERIFY(young.reason.contains("stakeable on"));
    QVERIFY(young.reason.contains("2 days"));
    // Some coins ready and some still maturing: staking can start.
    QVERIFY(stake(false, true, generation(Mode::OFF), eligibility(10 * COIN, 20 * COIN, 1735689600)).reason.isEmpty());
}

void EarnPageTests::stakingOtherWallet()
{
    const EarnPage::Blocker other = stake(false, true, generation(Mode::STAKING, "b"), eligibility(50 * COIN));
    QVERIFY(other.reason.contains("\"b\" is staking"));
    QVERIFY(other.has_go_to);
    QCOMPARE(other.go_to_wallet, QString("b"));
    // The legacy wallet has an empty name and is shown as "legacy".
    const EarnPage::Blocker legacy = stake(false, true, generation(Mode::STAKING, ""), eligibility(50 * COIN));
    QVERIFY(legacy.reason.contains("\"legacy\""));
    QCOMPARE(legacy.go_to_wallet, QString(""));
    // This wallet staking is not a blocker: the page shows Stop instead.
    QVERIFY(stake(false, true, generation(Mode::STAKING, "a"), eligibility(50 * COIN)).reason.isEmpty());
}

void EarnPageTests::miningAllowed()
{
    QVERIFY(EarnPage::miningBlocker(false, true, generation(Mode::OFF), "a").reason.isEmpty());
    // Mining needs no stakeable coins.
    QVERIFY(EarnPage::miningBlocker(false, true, generation(Mode::MINING, "a"), "a").reason.isEmpty());
}

void EarnPageTests::miningReasonOrder()
{
    QVERIFY(EarnPage::miningBlocker(true, false, generation(Mode::STAKING, "b"), "a").reason.contains("syncing"));
    QVERIFY(EarnPage::miningBlocker(false, false, generation(Mode::STAKING, "b"), "a").reason.contains("Encrypt"));
    QVERIFY(EarnPage::miningBlocker(false, true, generation(Mode::STAKING, "a"), "a").reason.contains("This wallet is staking"));
}

void EarnPageTests::miningOtherWallet()
{
    const EarnPage::Blocker staking = EarnPage::miningBlocker(false, true, generation(Mode::STAKING, "b"), "a");
    QVERIFY(staking.has_go_to);
    QCOMPARE(staking.go_to_wallet, QString("b"));
    const EarnPage::Blocker mining = EarnPage::miningBlocker(false, true, generation(Mode::MINING, "b"), "a");
    QVERIFY(mining.reason.contains("Only one wallet can mine"));
    QVERIFY(mining.has_go_to);
}
