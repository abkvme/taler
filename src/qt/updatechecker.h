// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_UPDATECHECKER_H
#define BITCOIN_QT_UPDATECHECKER_H

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

class OptionsModel;

QT_BEGIN_NAMESPACE
class QByteArray;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
QT_END_NAMESPACE

namespace updatecheck {

//! "v0.20.1" or "0.19.6.8" as numbers. Empty when the tag is not a plain release
//! version - a pre-release suffix, a stray word, more than four components - so
//! anything unexpected on the release page is ignored rather than guessed at.
QList<int> ParseVersion(const QString& tag);

//! Negative, zero or positive as a is older than, the same as, or newer than b.
//! Missing components count as zero: 0.20 and 0.20.0.0 are the same release.
int CompareVersions(const QList<int>& a, const QList<int>& b);

//! Pull the release tag out of GitHub's "latest release" answer. False for a
//! draft, a pre-release, malformed JSON or a tag ParseVersion rejects.
bool ParseLatestRelease(const QByteArray& json, QString& tag);

//! This build's version, in the same shape as a release tag.
QString CurrentVersion();

} // namespace updatecheck

/** Asks the project's release page, now and then, whether a newer version exists.
 *
 * Nothing is downloaded or installed: the only outcome is a flag the home screen
 * shows as a badge with a link to the release. One per application, owned by the
 * ClientModel; every wallet's overview page listens to the same instance.
 *
 * The check is a single HTTPS request that tells GitHub this address runs a Taler
 * wallet, so it can be switched off in the options, and it goes through the same
 * proxy as the node when one is configured.
 */
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(OptionsModel* options_model, QObject* parent = nullptr);

    bool isUpdateAvailable() const { return !m_latest.isEmpty(); }
    //! The newer release's tag, e.g. "v0.20.1"; empty when there is none.
    QString latestVersion() const { return m_latest; }
    //! That release's page, where the notes and the downloads are.
    QUrl releaseUrl() const;

public Q_SLOTS:
    void setEnabled(bool enabled);
    void checkNow();

Q_SIGNALS:
    //! The answer changed: an update appeared, or went away because checking was
    //! switched off.
    void statusChanged();

private Q_SLOTS:
    void onFinished(QNetworkReply* reply);

private:
    OptionsModel* m_options_model;
    QNetworkAccessManager* m_network = nullptr;
    QTimer* m_first_check;
    QTimer* m_interval;
    bool m_enabled = false;
    bool m_in_flight = false;
    QString m_latest;

    void setLatest(const QString& tag);
};

#endif // BITCOIN_QT_UPDATECHECKER_H
