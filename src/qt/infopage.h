// Copyright (c) 2024 The Taler developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_INFOPAGE_H
#define BITCOIN_QT_INFOPAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
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

    explicit ConnectivityChecker(QObject *parent = nullptr) : QThread(parent) {}
    void setItems(const std::vector<CheckItem>& items) { m_items = items; }
    //! Identifies the run a result belongs to, so results from a superseded run can
    //! be dropped instead of being painted onto rows that have since been rebuilt.
    void setGeneration(int generation) { m_generation = generation; }

Q_SIGNALS:
    void checkResult(int generation, int tableIndex, int row, bool reachable);

protected:
    void run() override;

private:
    std::vector<CheckItem> m_items;
    int m_generation = 0;
    bool tcpConnect(const std::string& host, int port, int timeoutSec);
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
    void onCheckResult(int generation, int tableIndex, int row, bool reachable);
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

    void populateTables();
    void setupTable(QTableWidget *table);
};

#endif // BITCOIN_QT_INFOPAGE_H
