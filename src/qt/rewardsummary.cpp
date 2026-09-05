// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rewardsummary.h>

#include <qt/transactionrecord.h>

#include <QTime>

CAmount RewardSummary::maximum() const
{
    CAmount best = 0;
    for (const CAmount& amount : monthly) {
        if (amount > best) best = amount;
    }
    return best;
}

int RewardSummary::daysSince(const QDateTime& stamp, const QDateTime& now)
{
    if (!stamp.isValid()) return -1;
    const qint64 seconds = stamp.secsTo(now);
    if (seconds < 0) return 0; // a reward timestamped slightly ahead of us is "today"
    return static_cast<int>(seconds / (24 * 60 * 60));
}

bool RewardSummary::readyToStake(const QDateTime& now) const
{
    if (!last_staking.isValid()) return true; // nothing to wait for
    return last_staking.secsTo(now) >= STAKING_ROUND_SECONDS;
}

qint64 RewardSummary::secondsUntilReady(const QDateTime& now) const
{
    if (readyToStake(now)) return 0;
    return STAKING_ROUND_SECONDS - last_staking.secsTo(now);
}

RewardSummary SummariseRewards(const QList<TransactionRecord>& records, const QDateTime& now)
{
    RewardSummary summary;
    summary.monthly.fill(0, REWARD_MONTHS);
    summary.month_starts.resize(REWARD_MONTHS);

    // Bucket boundaries are computed once, as epoch seconds. The loop below then
    // compares integers: building a QDateTime per transaction would cost more than
    // everything else here put together, on a wallet with a long history.
    QVector<qint64> bounds(REWARD_MONTHS + 1);
    const QDate first = QDate(now.date().year(), now.date().month(), 1)
                            .addMonths(-(REWARD_MONTHS - 1));
    for (int i = 0; i < REWARD_MONTHS; ++i) {
        const QDate start = first.addMonths(i);
        summary.month_starts[i] = start;
        bounds[i] = QDateTime(start, QTime(0, 0)).toSecsSinceEpoch();
    }
    bounds[REWARD_MONTHS] = QDateTime(first.addMonths(REWARD_MONTHS), QTime(0, 0)).toSecsSinceEpoch();

    qint64 latest_staking = 0;
    qint64 latest_mining = 0;

    for (const TransactionRecord& record : records) {
        if (record.type != TransactionRecord::Generated) continue;

        // "Last seen" spans the whole history, not just the charted year: a wallet
        // that last staked two years ago should say so, not say "never".
        if (record.isPoS) {
            if (record.time > latest_staking) latest_staking = record.time;
        } else {
            if (record.time > latest_mining) latest_mining = record.time;
        }

        if (record.time < bounds[0] || record.time >= bounds[REWARD_MONTHS]) continue;

        // Which month it fell in. Months are uneven, so this is a search rather
        // than a division.
        int low = 0;
        int high = REWARD_MONTHS;
        while (low < high - 1) {
            const int mid = (low + high) / 2;
            if (record.time >= bounds[mid]) low = mid; else high = mid;
        }
        summary.monthly[low] += record.credit;
        summary.total += record.credit;
    }

    if (latest_staking > 0) summary.last_staking = QDateTime::fromSecsSinceEpoch(latest_staking);
    if (latest_mining > 0) summary.last_mining = QDateTime::fromSecsSinceEpoch(latest_mining);

    return summary;
}
