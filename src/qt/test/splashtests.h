// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_SPLASHTESTS_H
#define BITCOIN_QT_TEST_SPLASHTESTS_H

#include <QObject>
#include <QTest>

/** The start-up artwork: that it is present, and that this build can decode it. */
class SplashTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void artworkDecodes();
    void englishIsAlwaysPresent();
    void everyImageSharesTheSameShape();
};

#endif // BITCOIN_QT_TEST_SPLASHTESTS_H
