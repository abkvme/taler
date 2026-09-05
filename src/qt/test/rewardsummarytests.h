// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_REWARDSUMMARYTESTS_H
#define BITCOIN_QT_TEST_REWARDSUMMARYTESTS_H

#include <QObject>
#include <QTest>

/** The arithmetic behind the overview page's reward badges and chart. */
class RewardSummaryTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyWallet();
    void monthBuckets();
    void monthEdges();
    void onlyRewardsCount();
    void lastSeenDates();
    void dayThresholds();
    void stakingReadiness();
    void totalMatchesBars();
};

#endif // BITCOIN_QT_TEST_REWARDSUMMARYTESTS_H
