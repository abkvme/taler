// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_REWARDSUMMARY_H
#define BITCOIN_QT_REWARDSUMMARY_H

#include <amount.h>

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QVector>

class TransactionRecord;

/** Months of history the overview chart shows, the current month last. */
static const int REWARD_MONTHS = 12;

/** How long after a reward the wallet is treated as able to start a new round.
 *
 * A product rule rather than a consensus one: the chain's minimum coin age is
 * Consensus::Params::nStakeMinAge, two days on mainnet. Reading true coin age needs
 * the unspent set and its block times; until then this is the single place to change.
 */
static const qint64 STAKING_ROUND_SECONDS = 7 * 24 * 60 * 60;

/** Where a wallet's block rewards stand: how recent, and how much per month.
 *
 * Only rewards count. A proof-of-stake block pays its staker through the block's
 * coinbase, so both staking and mining arrive as TransactionRecord::Generated and
 * are told apart by isPoS. The coinstake transaction that returns the staked
 * principal is a send-to-self and is deliberately not income.
 */
struct RewardSummary
{
    //! Invalid when this wallet has never earned that kind of reward.
    QDateTime last_staking;
    QDateTime last_mining;

    //! REWARD_MONTHS entries, oldest first, the current month last.
    QVector<CAmount> monthly;
    //! First day of each bucket, for labelling. Same length as monthly.
    QVector<QDate> month_starts;

    CAmount total = 0;

    //! Largest month, for scaling the bars. Zero when nothing was earned.
    CAmount maximum() const;

    //! Whole days since that reward, or -1 when it never happened.
    static int daysSince(const QDateTime& stamp, const QDateTime& now);
    int daysSinceStaking(const QDateTime& now) const { return daysSince(last_staking, now); }
    int daysSinceMining(const QDateTime& now) const { return daysSince(last_mining, now); }

    //! True once a new staking round can be started - including for a wallet that
    //! has never staked, which has nothing to wait for.
    bool readyToStake(const QDateTime& now) const;

    //! Seconds still to wait, or 0 when ready.
    qint64 secondsUntilReady(const QDateTime& now) const;
};

/** Summarise records into the shape above.
 *
 * Free function over plain records: no model, no wallet, no chain, so the month
 * boundaries and the day thresholds can be tested directly.
 */
RewardSummary SummariseRewards(const QList<TransactionRecord>& records, const QDateTime& now);

#endif // BITCOIN_QT_REWARDSUMMARY_H
