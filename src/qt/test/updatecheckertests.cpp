// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/updatecheckertests.h>

#include <qt/updatechecker.h>

using namespace updatecheck;

void UpdateCheckerTests::parseVersion()
{
    QCOMPARE(ParseVersion("v0.20.1"), (QList<int>{0, 20, 1}));
    QCOMPARE(ParseVersion("0.19.6.8"), (QList<int>{0, 19, 6, 8}));
    QCOMPARE(ParseVersion("v1.0"), (QList<int>{1, 0}));

    // Anything that is not a plain release number is ignored, not guessed at.
    QVERIFY(ParseVersion("").isEmpty());
    QVERIFY(ParseVersion("v1").isEmpty());
    QVERIFY(ParseVersion("v0.20.1-rc1").isEmpty());
    QVERIFY(ParseVersion("v0.20.1.2.3").isEmpty());
    QVERIFY(ParseVersion("latest").isEmpty());
    QVERIFY(ParseVersion("v0.20.1/../../evil").isEmpty());
    QVERIFY(ParseVersion(" v0.20.1").isEmpty());
}

void UpdateCheckerTests::compareVersions()
{
    QVERIFY(CompareVersions(ParseVersion("v0.20.1"), ParseVersion("v0.20.0")) > 0);
    QVERIFY(CompareVersions(ParseVersion("v0.20.0"), ParseVersion("v0.20.1")) < 0);
    // Numbers, not text: 10 comes after 9.
    QVERIFY(CompareVersions(ParseVersion("v0.20.10"), ParseVersion("v0.20.9")) > 0);
    // The old four-part tags sort below the three-part ones that replaced them.
    QVERIFY(CompareVersions(ParseVersion("v0.20.0"), ParseVersion("v0.19.6.8")) > 0);
    // Missing components are zero.
    QCOMPARE(CompareVersions(ParseVersion("v0.20.0"), ParseVersion("v0.20.0.0")), 0);
    QVERIFY(CompareVersions(ParseVersion("v0.20.0.1"), ParseVersion("v0.20.0")) > 0);
}

void UpdateCheckerTests::parseLatestRelease()
{
    QString tag;
    QVERIFY(ParseLatestRelease(R"({"tag_name":"v0.20.1","draft":false,"prerelease":false})", tag));
    QCOMPARE(tag, QString("v0.20.1"));

    tag.clear();
    QVERIFY(!ParseLatestRelease(R"({"tag_name":"v0.20.2","prerelease":true})", tag));
    QVERIFY(!ParseLatestRelease(R"({"tag_name":"v0.20.2","draft":true})", tag));
    QVERIFY(!ParseLatestRelease(R"({"tag_name":"nightly"})", tag));
    QVERIFY(!ParseLatestRelease(R"({"message":"API rate limit exceeded"})", tag));
    QVERIFY(!ParseLatestRelease(R"([{"tag_name":"v0.20.1"}])", tag));
    QVERIFY(!ParseLatestRelease("not json", tag));
    QVERIFY(tag.isEmpty());
}

void UpdateCheckerTests::currentVersionParses()
{
    // If this build's own version could not be read, every release would look new.
    QVERIFY(!ParseVersion(CurrentVersion()).isEmpty());
}
