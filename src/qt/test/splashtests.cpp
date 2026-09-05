// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/splashtests.h>

#include <QImageReader>
#include <QPixmap>
#include <QStringList>

namespace {
//! Every language that ships its own start-up image.
const QStringList kLanguages = {"en", "ru", "be"};
} // namespace

void SplashTests::artworkDecodes()
{
    // The point of this test. The artwork is JPEG, and Qt decodes JPEG through a
    // plugin that a static build cannot load from disk - it has to be linked in.
    // When it is missing nothing fails loudly: QPixmap just hands back a null
    // image and the splash comes up as a plain panel, which is easy to ship
    // without noticing. Naming the format explicitly says what is actually being
    // checked here, which is the decoder rather than the file.
    QVERIFY2(QImageReader::supportedImageFormats().contains("jpeg"),
             "this build cannot decode JPEG: the Qt image-format plugin is not linked in, "
             "so the start-up artwork would silently fall back to a plain panel");

    for (const QString& language : kLanguages) {
        const QString resource = QString(":/images/splash_%1").arg(language);
        QPixmap artwork(resource);
        QVERIFY2(!artwork.isNull(), qPrintable(QString("could not load %1").arg(resource)));
        QVERIFY(artwork.width() > 0 && artwork.height() > 0);
    }
}

void SplashTests::englishIsAlwaysPresent()
{
    // Any language without artwork of its own falls back to this one, so it is
    // the only image whose absence would leave a language with nothing.
    QPixmap fallback(":/images/splash_en");
    QVERIFY(!fallback.isNull());
}

void SplashTests::everyImageSharesTheSameShape()
{
    // The splash sizes itself from the artwork's aspect ratio, so images that
    // disagree would give each language a differently shaped window.
    QPixmap reference(":/images/splash_en");
    QVERIFY(!reference.isNull());
    for (const QString& language : kLanguages) {
        QPixmap artwork(QString(":/images/splash_%1").arg(language));
        QVERIFY(!artwork.isNull());
        QCOMPARE(artwork.size(), reference.size());
    }
}
