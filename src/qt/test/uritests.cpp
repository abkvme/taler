// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/uritests.h>

#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?req-message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("taler:TBa97KJ9VJKnGkZp72xYnFLqojKTvVGTDi?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));
}
