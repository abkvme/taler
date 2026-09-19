// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/updatechecker.h>

#include <qt/optionsmodel.h>

#include <logging.h>

#include <algorithm>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>

//! Where releases are published. The API answers with the newest release that is
//! neither a draft nor marked as a pre-release.
static const char* RELEASES_API = "https://api.github.com/repos/abkvme/taler/releases/latest";
static const char* RELEASE_PAGE = "https://github.com/abkvme/taler/releases/tag/";

//! Late enough not to compete with startup, which is busy loading the chain.
static const int FIRST_CHECK_DELAY_MS = 15 * 1000;
static const int CHECK_INTERVAL_MS = 24 * 60 * 60 * 1000;
static const int REQUEST_TIMEOUT_MS = 30 * 1000;
//! A release answer is a few kilobytes; anything much larger is not one.
static const qint64 MAX_REPLY_BYTES = 1024 * 1024;

namespace updatecheck {

QList<int> ParseVersion(const QString& tag)
{
    static const QRegularExpression pattern(QStringLiteral("^v?(\\d{1,5})(?:\\.(\\d{1,5})){1,3}$"));
    QList<int> parts;
    if (!pattern.match(tag).hasMatch()) return parts;

    const QString digits = tag.startsWith(QChar('v')) ? tag.mid(1) : tag;
    for (const QString& part : digits.split(QChar('.'))) {
        parts.append(part.toInt());
    }
    return parts;
}

int CompareVersions(const QList<int>& a, const QList<int>& b)
{
    const int length = std::max(a.size(), b.size());
    for (int i = 0; i < length; ++i) {
        const int left = i < a.size() ? a.at(i) : 0;
        const int right = i < b.size() ? b.at(i) : 0;
        if (left != right) return left < right ? -1 : 1;
    }
    return 0;
}

bool ParseLatestRelease(const QByteArray& json, QString& tag)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;

    const QJsonObject release = doc.object();
    // The endpoint already skips both, but a wallet telling people to install a
    // draft would be a bad way to find out that ever changed.
    if (release.value(QStringLiteral("draft")).toBool() ||
        release.value(QStringLiteral("prerelease")).toBool()) {
        return false;
    }

    const QString candidate = release.value(QStringLiteral("tag_name")).toString().trimmed();
    if (ParseVersion(candidate).isEmpty()) return false;
    tag = candidate;
    return true;
}

QString CurrentVersion()
{
    // Written the way releases are tagged: the fourth component only when it is
    // not zero, so this build reads as v0.20.0 and not v0.20.0.0.
    QString version = QStringLiteral("v%1.%2.%3")
        .arg(CLIENT_VERSION_MAJOR)
        .arg(CLIENT_VERSION_MINOR)
        .arg(CLIENT_VERSION_REVISION);
    if (CLIENT_VERSION_BUILD != 0) version += QStringLiteral(".%1").arg(CLIENT_VERSION_BUILD);
    return version;
}

} // namespace updatecheck

UpdateChecker::UpdateChecker(OptionsModel* options_model, QObject* parent) :
    QObject(parent),
    m_options_model(options_model)
{
    m_first_check = new QTimer(this);
    m_first_check->setSingleShot(true);
    m_first_check->setInterval(FIRST_CHECK_DELAY_MS);
    connect(m_first_check, &QTimer::timeout, this, &UpdateChecker::checkNow);

    m_interval = new QTimer(this);
    m_interval->setInterval(CHECK_INTERVAL_MS);
    connect(m_interval, &QTimer::timeout, this, &UpdateChecker::checkNow);

    if (m_options_model) {
        connect(m_options_model, &OptionsModel::checkForUpdatesChanged, this, &UpdateChecker::setEnabled);
        setEnabled(m_options_model->getCheckForUpdates());
    }
}

QUrl UpdateChecker::releaseUrl() const
{
    // Built from the tag rather than taken from the answer: the tag has already
    // been checked to be nothing but a version number, so this can only ever
    // point at the project's own release page.
    if (m_latest.isEmpty()) return QUrl();
    return QUrl(QString::fromLatin1(RELEASE_PAGE) + m_latest);
}

void UpdateChecker::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;

    if (enabled) {
        m_first_check->start();
        m_interval->start();
    } else {
        m_first_check->stop();
        m_interval->stop();
        // Switched off means the badge goes too, not that it stays up with an
        // answer nothing will ever refresh.
        setLatest(QString());
    }
}

void UpdateChecker::checkNow()
{
    if (!m_enabled || m_in_flight) return;

    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
        connect(m_network, &QNetworkAccessManager::finished, this, &UpdateChecker::onFinished);
    }
    // Asked each time: the proxy can change in the options while the wallet runs.
    // Someone who sends the node through Tor expects this to go the same way.
    QNetworkProxy proxy;
    if (m_options_model && m_options_model->getProxySettings(proxy)) {
        m_network->setProxy(proxy);
    } else {
        m_network->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    }

    QNetworkRequest request{QUrl(QString::fromLatin1(RELEASES_API))};
    // GitHub refuses requests without a user agent.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Taler-Qt/%1").arg(updatecheck::CurrentVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(REQUEST_TIMEOUT_MS);

    m_in_flight = true;
    m_network->get(request);
}

void UpdateChecker::onFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    m_in_flight = false;
    // Switched off while the request was out: the answer is no longer wanted.
    if (!m_enabled) return;

    // Failures are silent. Being offline, or GitHub having a bad hour, is not
    // something to put in front of the user; the next check tries again.
    if (reply->error() != QNetworkReply::NoError) {
        LogPrint(BCLog::QT, "Update check failed: %s\n", reply->errorString().toStdString());
        return;
    }
    if (reply->bytesAvailable() > MAX_REPLY_BYTES) {
        LogPrint(BCLog::QT, "Update check: answer too large, ignored\n");
        return;
    }

    QString tag;
    if (!updatecheck::ParseLatestRelease(reply->readAll(), tag)) {
        LogPrint(BCLog::QT, "Update check: no usable release in the answer\n");
        return;
    }

    const bool newer = updatecheck::CompareVersions(updatecheck::ParseVersion(tag),
                                                    updatecheck::ParseVersion(updatecheck::CurrentVersion())) > 0;
    LogPrint(BCLog::QT, "Update check: latest release %s, running %s\n",
             tag.toStdString(), updatecheck::CurrentVersion().toStdString());
    setLatest(newer ? tag : QString());
}

void UpdateChecker::setLatest(const QString& tag)
{
    if (m_latest == tag) return;
    m_latest = tag;
    Q_EMIT statusChanged();
}
