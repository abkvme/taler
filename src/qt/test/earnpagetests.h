// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_EARNPAGETESTS_H
#define BITCOIN_QT_TEST_EARNPAGETESTS_H

#include <QObject>
#include <QTest>

/** Which reason the Earn page gives for a greyed-out Start, and in what order. */
class EarnPageTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stakingAllowed();
    void stakingReasonOrder();
    void stakingCoins();
    void stakingOtherWallet();
    void miningAllowed();
    void miningReasonOrder();
    void miningOtherWallet();
};

#endif // BITCOIN_QT_TEST_EARNPAGETESTS_H
