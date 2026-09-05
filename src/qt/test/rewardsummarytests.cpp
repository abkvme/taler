// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/rewardsummarytests.h>

#include <qt/rewardsummary.h>
#include <qt/transactionrecord.h>

#include <QDate>
#include <QDateTime>
#include <QTime>

namespace {

//! A fixed "now" so the tests do not drift with the calendar: 15 August 2026, midday.
QDateTime referenceNow()
{
    return QDateTime(QDate(2026, 8, 15), QTime(12, 0));
}

qint64 secondsAt(int year, int month, int day, int hour = 12, int minute = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute)).toSecsSinceEpoch();
}

TransactionRecord reward(qint64 when, CAmount amount, bool proof_of_stake)
{
    TransactionRecord record(uint256(), when, TransactionRecord::Generated, "", 0, amount);
    record.isPoS = proof_of_stake;
    return record;
}

TransactionRecord payment(qint64 when, CAmount amount)
{
    return TransactionRecord(uint256(), when, TransactionRecord::RecvWithAddress, "", 0, amount);
}

} // namespace

void RewardSummaryTests::emptyWallet()
{
    const RewardSummary summary = SummariseRewards(QList<TransactionRecord>(), referenceNow());

    QCOMPARE(summary.monthly.size(), REWARD_MONTHS);
    QCOMPARE(summary.month_starts.size(), REWARD_MONTHS);
    QCOMPARE(summary.total, CAmount(0));
    QCOMPARE(summary.maximum(), CAmount(0));
    for (const CAmount& month : summary.monthly) QCOMPARE(month, CAmount(0));

    // Never earned, so nothing to report - and nothing to wait for either.
    QVERIFY(!summary.last_staking.isValid());
    QVERIFY(!summary.last_mining.isValid());
    QCOMPARE(summary.daysSinceStaking(referenceNow()), -1);
    QVERIFY(summary.readyToStake(referenceNow()));
}

void RewardSummaryTests::monthBuckets()
{
    // Twelve months ending with the current one: September 2025 through August 2026.
    QList<TransactionRecord> records;
    records << reward(secondsAt(2026, 8, 2), 100, true);    // current month
    records << reward(secondsAt(2026, 7, 20), 50, true);    // one back
    records << reward(secondsAt(2025, 9, 5), 7, true);      // oldest bucket

    const RewardSummary summary = SummariseRewards(records, referenceNow());

    QCOMPARE(summary.month_starts.first(), QDate(2025, 9, 1));
    QCOMPARE(summary.month_starts.last(), QDate(2026, 8, 1));
    QCOMPARE(summary.monthly.last(), CAmount(100));
    QCOMPARE(summary.monthly[REWARD_MONTHS - 2], CAmount(50));
    QCOMPARE(summary.monthly.first(), CAmount(7));
    QCOMPARE(summary.maximum(), CAmount(100));
}

void RewardSummaryTests::monthEdges()
{
    QList<TransactionRecord> records;
    records << reward(secondsAt(2026, 7, 1, 0, 0), 1, true);      // first second of July
    records << reward(secondsAt(2026, 7, 31, 23, 59) + 59, 2, true); // last second of July
    records << reward(secondsAt(2025, 8, 31, 23, 59), 999, true);    // one day too old
    records << reward(secondsAt(2025, 9, 1, 0, 0), 3, true);         // first second in range

    const RewardSummary summary = SummariseRewards(records, referenceNow());

    QCOMPARE(summary.monthly[REWARD_MONTHS - 2], CAmount(3)); // both July rewards
    QCOMPARE(summary.monthly.first(), CAmount(3));            // the 1 September one
    QCOMPARE(summary.total, CAmount(6));                      // the older one excluded
}

void RewardSummaryTests::onlyRewardsCount()
{
    QList<TransactionRecord> records;
    records << reward(secondsAt(2026, 8, 3), 40, true);
    records << reward(secondsAt(2026, 8, 4), 60, false);              // mined, still income
    records << payment(secondsAt(2026, 8, 5), 1000);                  // someone paid us
    records << TransactionRecord(uint256(), secondsAt(2026, 8, 6),
                                 TransactionRecord::SendToSelf, "", -5, 5); // coinstake principal

    const RewardSummary summary = SummariseRewards(records, referenceNow());

    // Only the two block rewards: a payment is not income the wallet produced, and
    // counting the coinstake would count the staked principal as earnings.
    QCOMPARE(summary.monthly.last(), CAmount(100));
    QCOMPARE(summary.total, CAmount(100));
}

void RewardSummaryTests::lastSeenDates()
{
    QList<TransactionRecord> records;
    records << reward(secondsAt(2026, 8, 1), 1, true);
    records << reward(secondsAt(2026, 8, 10), 1, true);   // the most recent stake
    records << reward(secondsAt(2026, 6, 1), 1, false);
    records << reward(secondsAt(2026, 7, 4), 1, false);   // the most recent mine
    // Older than the chart, but still the answer to "when did this last happen"
    records << reward(secondsAt(2020, 1, 1), 1, true);

    const RewardSummary summary = SummariseRewards(records, referenceNow());

    QCOMPARE(summary.last_staking.toSecsSinceEpoch(), secondsAt(2026, 8, 10));
    QCOMPARE(summary.last_mining.toSecsSinceEpoch(), secondsAt(2026, 7, 4));
}

void RewardSummaryTests::dayThresholds()
{
    const QDateTime now = referenceNow();
    const qint64 day = 24 * 60 * 60;

    QList<TransactionRecord> six;
    six << reward(now.toSecsSinceEpoch() - 6 * day, 1, true);
    QCOMPARE(SummariseRewards(six, now).daysSinceStaking(now), 6);

    QList<TransactionRecord> seven;
    seven << reward(now.toSecsSinceEpoch() - 7 * day, 1, true);
    QCOMPARE(SummariseRewards(seven, now).daysSinceStaking(now), 7);

    QList<TransactionRecord> thirty_one;
    thirty_one << reward(now.toSecsSinceEpoch() - 31 * day, 1, true);
    QCOMPARE(SummariseRewards(thirty_one, now).daysSinceStaking(now), 31);

    // A reward stamped a little ahead of us - block times may run ahead - is today,
    // never a negative number of days.
    QList<TransactionRecord> ahead;
    ahead << reward(now.toSecsSinceEpoch() + 600, 1, true);
    QCOMPARE(SummariseRewards(ahead, now).daysSinceStaking(now), 0);
}

void RewardSummaryTests::stakingReadiness()
{
    const QDateTime now = referenceNow();
    const qint64 day = 24 * 60 * 60;

    QList<TransactionRecord> just_now;
    just_now << reward(now.toSecsSinceEpoch() - 1 * day, 1, true);
    RewardSummary summary = SummariseRewards(just_now, now);
    QVERIFY(!summary.readyToStake(now));
    QCOMPARE(summary.secondsUntilReady(now), 6 * day);

    // Exactly seven days is ready, not "one second short".
    QList<TransactionRecord> exactly;
    exactly << reward(now.toSecsSinceEpoch() - STAKING_ROUND_SECONDS, 1, true);
    summary = SummariseRewards(exactly, now);
    QVERIFY(summary.readyToStake(now));
    QCOMPARE(summary.secondsUntilReady(now), qint64(0));

    // Mining says nothing about staking readiness.
    QList<TransactionRecord> mined_only;
    mined_only << reward(now.toSecsSinceEpoch() - 1 * day, 1, false);
    QVERIFY(SummariseRewards(mined_only, now).readyToStake(now));
}

void RewardSummaryTests::totalMatchesBars()
{
    QList<TransactionRecord> records;
    records << reward(secondsAt(2026, 8, 2), 100, true);
    records << reward(secondsAt(2026, 3, 2), 250, true);
    records << reward(secondsAt(2025, 12, 24), 33, false);
    records << reward(secondsAt(2019, 1, 1), 99999, true); // outside the chart

    const RewardSummary summary = SummariseRewards(records, referenceNow());

    // The headline total is the sum of the bars, so the two can never disagree.
    CAmount summed = 0;
    for (const CAmount& month : summary.monthly) summed += month;
    QCOMPARE(summary.total, summed);
    QCOMPARE(summary.total, CAmount(383));
}
