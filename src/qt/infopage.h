// Copyright (c) 2024 The Taler developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_INFOPAGE_H
#define BITCOIN_QT_INFOPAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QSet>
#include <QString>
#include <QThread>
#include <vector>
#include <string>

class ClientModel;
class PlatformStyle;

class ConnectivityChecker : public QThread
{
    Q_OBJECT
public:
    struct CheckItem {
        QString host;
        int port;
        int tableIndex; // which table: 0=hardcoded, 1=github, 2=discovered
        int row;
    };

    /** What a check found.
     *
     * Resolution and reachability are reported separately because they fail for
     * different reasons and the difference is what a seed operator needs: a name
     * that does not resolve is a DNS problem, while a name that resolves to an
     * unreachable address is a host or firewall problem. Collapsing both into one
     * red dot tells nobody anything.
     */
    struct CheckOutcome {
        bool resolved = false;
        bool reachable = false;
        QString address; //!< the address actually dialled, empty when unresolved
    };

    explicit ConnectivityChecker(QObject *parent = nullptr) : QThread(parent) {}
    void setItems(const std::vector<CheckItem>& items) { m_items = items; }
    //! Identifies the run a result belongs to, so results from a superseded run can
    //! be dropped instead of being painted onto rows that have since been rebuilt.
    void setGeneration(int generation) { m_generation = generation; }

Q_SIGNALS:
    /** Outcome of one check.
     *
     * Resolution and reachability are separate because they fail for different
     * reasons, and the difference is the useful part: a name that does not
     * resolve is a DNS problem, a name that resolves to an unreachable address
     * is a host or firewall problem. One red dot for both tells nobody anything.
     *
     * Plain parameters rather than a struct on purpose - this crosses threads
     * through a queued connection, where a custom type would need
     * qRegisterMetaType or the signal silently does nothing at runtime.
     */
    void checkResult(int generation, int tableIndex, int row,
                     bool resolved, bool reachable, QString address, QString version);

protected:
    void run() override;

private:
    std::vector<CheckItem> m_items;
    int m_generation = 0;
    //! Resolve, then dial. `address` is the address actually dialled, so the
    //! page can show what a hostname resolved to rather than only whether it
    //! worked; empty when the name did not resolve at all. `subVersion` is the
    //! user agent the seed announced, empty when it announced nothing usable.
    bool tcpConnect(const std::string& host, int port, int timeoutSec,
                    bool& resolved, std::string& address, std::string& subVersion);
    //! Exchange version messages on an already-connected socket and keep what the
    //! remote said its user agent is.
    //!
    //! This is a probe, not a peering: the connection is dropped straight
    //! afterwards without a verack, nothing is requested and nothing is relayed,
    //! so the remote sees an ordinary short-lived connection. Every read is
    //! bounded and sliced so an interrupted sweep stops promptly, and anything
    //! unparseable leaves the version blank rather than guessed at.
    bool probeSubVersion(int sock, int timeoutSec, std::string& subVersion);
};

class InfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit InfoPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~InfoPage();

    void setClientModel(ClientModel *clientModel);

public Q_SLOTS:
    void refreshData();

private Q_SLOTS:
    void onCheckResult(int generation, int tableIndex, int row,
                       bool resolved, bool reachable, QString address, QString version);
    void showContributeInfo();

private:
    ClientModel *clientModel;
    const PlatformStyle *platformStyle;

    QTableWidget *hardcodedTable;
    QTableWidget *githubTable;
    QTableWidget *discoveredTable;
    QLabel *githubStatusLabel;
    QLabel *githubLink;
    QPushButton *refreshButton;
    QPushButton *contributeButton;

    //! The running check, or nullptr. Never owned by InfoPage: a check can outlive
    //! the page and frees itself when it stops. See refreshData().
    ConnectivityChecker *checker;
    int m_generation;

    //! Addresses the seed lists resolved to on the current sweep, so a peer in the
    //! discovered table can be recognised as one this wallet was told about.
    //!
    //! Filled from the checks rather than from the seed lists directly, because a
    //! seed is usually a hostname while a peer is always an address - the only
    //! thing that makes them comparable is what the name actually resolved to.
    //! Both are cleared whenever the tables are rebuilt, so a marker never
    //! survives the seed list it came from.
    QSet<QString> m_embedded_addresses;
    QSet<QString> m_community_addresses;

    void populateTables();
    void setupTable(QTableWidget *table);
    //! Re-mark the discovered peers that came from a seed list. Cheap and
    //! idempotent; run again after every check result, since a seed's address is
    //! only known once its check comes back.
    void applySeedMarkers();
};

#endif // BITCOIN_QT_INFOPAGE_H
