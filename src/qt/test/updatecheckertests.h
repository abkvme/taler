// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_UPDATECHECKERTESTS_H
#define BITCOIN_QT_TEST_UPDATECHECKERTESTS_H

#include <QObject>
#include <QTest>

/** Reading release tags and deciding whether one is newer than this build. */
class UpdateCheckerTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parseVersion();
    void compareVersions();
    void parseLatestRelease();
    void currentVersionParses();
};

#endif // BITCOIN_QT_TEST_UPDATECHECKERTESTS_H
