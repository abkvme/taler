// Copyright (c) 2024 The Taler developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/infopage.h>
#include <qt/clientmodel.h>
#include <qt/platformstyle.h>

#include <chainparams.h>
#include <chainparamsseeds.h>
#include <net.h>
#include <netbase.h>

#include <QDesktopServices>
#include <QHeaderView>
#include <QMessageBox>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif

// ConnectivityChecker implementation
bool ConnectivityChecker::tcpConnect(const std::string& host, int port, int timeoutSec)
{
    struct addrinfo hints{}, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0)
        return false;

    bool connected = false;
    for (struct addrinfo *rp = res; rp != nullptr; rp = rp->ai_next) {
        int sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;

#ifdef WIN32
        unsigned long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

        int ret = ::connect(sock, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0) {
            connected = true;
        } else {
#ifdef WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(sock, &wfds);
                struct timeval tv;
                tv.tv_sec = timeoutSec;
                tv.tv_usec = 0;
                if (select(sock + 1, nullptr, &wfds, nullptr, &tv) > 0) {
                    int err = 0;
                    int len = sizeof(err);
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
                    connected = (err == 0);
                }
            }
#else
            if (errno == EINPROGRESS) {
                struct pollfd pfd;
                pfd.fd = sock;
                pfd.events = POLLOUT;
                if (poll(&pfd, 1, timeoutSec * 1000) > 0) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
                    connected = (err == 0);
                }
            }
#endif
        }

#ifdef WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        if (connected) break;
    }

    freeaddrinfo(res);
    return connected;
}

void ConnectivityChecker::run()
{
    for (const auto& item : m_items) {
        if (isInterruptionRequested()) return;
        bool ok = tcpConnect(item.host.toStdString(), item.port, 3);
        Q_EMIT checkResult(item.tableIndex, item.row, ok);
    }
}

// InfoPage implementation
InfoPage::InfoPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    clientModel(nullptr),
    platformStyle(_platformStyle),
    checker(nullptr)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Top bar: refresh button only
    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->addStretch();
    refreshButton = new QPushButton(tr("Refresh"));
    connect(refreshButton, &QPushButton::clicked, this, &InfoPage::refreshData);
    topBar->addWidget(refreshButton);
    mainLayout->addLayout(topBar);

    // Main splitter: left (seeds) | right (discovered)
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Left side: hardcoded + github
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // Hardcoded seeds group
    QGroupBox *hardcodedGroup = new QGroupBox(tr("Hardcoded Seeds"));
    QVBoxLayout *hcLayout = new QVBoxLayout(hardcodedGroup);
    hardcodedTable = new QTableWidget();
    setupTable(hardcodedTable);
    hcLayout->addWidget(hardcodedTable);
    leftLayout->addWidget(hardcodedGroup);

    // GitHub seeds group
    QGroupBox *githubGroup = new QGroupBox(tr("Community Seeds"));
    QVBoxLayout *ghLayout = new QVBoxLayout(githubGroup);

    // Info button next to community seeds header
    QHBoxLayout *communityHeader = new QHBoxLayout();
    contributeButton = new QPushButton();
    contributeButton->setIcon(platformStyle->SingleColorIcon(":/icons/info"));
    contributeButton->setFixedSize(24, 24);
    contributeButton->setFlat(true);
    contributeButton->setToolTip(tr("Learn how to add your node to the seed list"));
    connect(contributeButton, &QPushButton::clicked, this, &InfoPage::showContributeInfo);

    // GitHub clickable link
    githubLink = new QLabel(
        "<a href=\"https://github.com/abkvme/taler-seeds\">"
        "github.com/abkvme/taler-seeds</a>");
    githubLink->setTextFormat(Qt::RichText);
    githubLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    githubLink->setOpenExternalLinks(true);
    communityHeader->addWidget(githubLink);
    communityHeader->addWidget(contributeButton);
    communityHeader->addStretch();
    ghLayout->addLayout(communityHeader);

    githubStatusLabel = new QLabel();
    githubStatusLabel->hide();
    ghLayout->addWidget(githubStatusLabel);
    githubTable = new QTableWidget();
    setupTable(githubTable);
    ghLayout->addWidget(githubTable);
    leftLayout->addWidget(githubGroup);

    splitter->addWidget(leftWidget);

    // Right side: discovered peers
    QGroupBox *discoveredGroup = new QGroupBox(tr("Discovered Peers"));
    QVBoxLayout *discLayout = new QVBoxLayout(discoveredGroup);
    discoveredTable = new QTableWidget();
    setupTable(discoveredTable);
    // Add Version column for discovered peers
    discoveredTable->setColumnCount(3);
    discoveredTable->setHorizontalHeaderLabels({tr("Node"), tr("Version"), tr("Status")});
    discoveredTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    discoveredTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    discoveredTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    discLayout->addWidget(discoveredTable);
    splitter->addWidget(discoveredGroup);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter, 1);

    mainLayout->addStretch(0);

    // Maintenance footer pinned to bottom
    QLabel *footerLabel = new QLabel(tr("Maintained by abkvme, 2025-2026"));
    footerLabel->setStyleSheet("font-size: 11px; color: #888; padding: 4px 0;");
    footerLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(footerLabel);

    setLayout(mainLayout);
}

InfoPage::~InfoPage()
{
    if (checker) {
        checker->requestInterruption();
        checker->wait(5000);
        delete checker;
    }
}

void InfoPage::showContributeInfo()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Contribute Your Node"));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(tr(
        "If you maintain a Taler node on a static IP address with reliable uptime, "
        "we encourage you to add your node to the community seed list on GitHub.\n\n"
        "This helps other Taler nodes discover peers and strengthens the network.\n\n"
        "Visit the repository to learn how to contribute:"));
    msgBox.setInformativeText(
        "<a href=\"https://github.com/abkvme/taler-seeds\">"
        "https://github.com/abkvme/taler-seeds</a>");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.exec();
}

void InfoPage::setupTable(QTableWidget *table)
{
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({tr("Node"), tr("Status")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
}

void InfoPage::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
    if (clientModel) {
        populateTables();
    }
}

void InfoPage::populateTables()
{
    hardcodedTable->setRowCount(0);
    githubTable->setRowCount(0);
    discoveredTable->setRowCount(0);

    // Collect GitHub seed hosts for dedup
    std::set<std::string> githubHosts;
    if (g_connman) {
        for (const auto& seed : g_connman->m_remote_seeds) {
            githubHosts.insert(seed.first);
        }
    }

    // Hardcoded DNS seeds (excluding those in GitHub list)
    const std::vector<std::string>& dnsSeeds = Params().DNSSeeds();
    for (const auto& seed : dnsSeeds) {
        if (githubHosts.count(seed)) continue;
        int row = hardcodedTable->rowCount();
        hardcodedTable->insertRow(row);
        hardcodedTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(seed)));
        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromUtf8("\xe2\x97\x8f"));
        statusItem->setForeground(Qt::gray);
        statusItem->setTextAlignment(Qt::AlignCenter);
        hardcodedTable->setItem(row, 1, statusItem);
    }

    // Hardcoded fixed IP seeds (excluding those in GitHub list)
    const std::vector<SeedSpec6>& fixedSeeds = Params().FixedSeeds();
    for (const auto& seed : fixedSeeds) {
        struct in6_addr ip;
        memcpy(&ip, seed.addr, sizeof(ip));
        CService service(ip, seed.port);
        std::string addrStr = service.ToString();
        if (githubHosts.count(addrStr)) continue;
        int row = hardcodedTable->rowCount();
        hardcodedTable->insertRow(row);
        hardcodedTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(addrStr)));
        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromUtf8("\xe2\x97\x8f"));
        statusItem->setForeground(Qt::gray);
        statusItem->setTextAlignment(Qt::AlignCenter);
        hardcodedTable->setItem(row, 1, statusItem);
    }

    // GitHub seeds
    if (g_connman && g_connman->m_remote_seeds_available) {
        githubStatusLabel->hide();
        for (const auto& seed : g_connman->m_remote_seeds) {
            int row = githubTable->rowCount();
            githubTable->insertRow(row);
            QString nodeStr = QString::fromStdString(seed.first);
            if (seed.second != Params().GetDefaultPort()) {
                nodeStr += ":" + QString::number(seed.second);
            }
            githubTable->setItem(row, 0, new QTableWidgetItem(nodeStr));
            QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromUtf8("\xe2\x97\x8f"));
            statusItem->setForeground(Qt::gray);
            statusItem->setTextAlignment(Qt::AlignCenter);
            githubTable->setItem(row, 1, statusItem);
        }
    } else {
        githubStatusLabel->setText(tr("Unavailable - remote seed list could not be fetched"));
        githubStatusLabel->setStyleSheet("QLabel { color: #cc0000; }");
        githubStatusLabel->show();
    }

    // Discovered peers from connected nodes
    if (g_connman) {
        std::vector<CNodeStats> vstats;
        g_connman->GetNodeStats(vstats);
        for (const auto& stats : vstats) {
            int row = discoveredTable->rowCount();
            discoveredTable->insertRow(row);
            discoveredTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(stats.addrName)));

            // Extract version: cleanSubVer is like "/Taler:0.18.44.7/" - strip name prefix and slashes
            QString subVer = QString::fromStdString(stats.cleanSubVer);
            subVer.remove('/');
            int colonPos = subVer.indexOf(':');
            if (colonPos >= 0)
                subVer = subVer.mid(colonPos + 1);
            discoveredTable->setItem(row, 1, new QTableWidgetItem(subVer));

            // Connected peers are green by default
            QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromUtf8("\xe2\x97\x8f"));
            statusItem->setForeground(QColor(0, 180, 0));
            statusItem->setTextAlignment(Qt::AlignCenter);
            discoveredTable->setItem(row, 2, statusItem);
        }
    }
}

void InfoPage::refreshData()
{
    // Stop any running checker
    if (checker) {
        checker->requestInterruption();
        checker->wait(5000);
        delete checker;
        checker = nullptr;
    }

    populateTables();

    // Start connectivity checks for hardcoded and github seeds
    std::vector<ConnectivityChecker::CheckItem> items;

    for (int i = 0; i < hardcodedTable->rowCount(); i++) {
        ConnectivityChecker::CheckItem item;
        item.host = hardcodedTable->item(i, 0)->text();
        // Extract port if present in host:port format
        if (item.host.contains(":")) {
            QStringList parts = item.host.split(":");
            item.host = parts[0];
            item.port = parts[1].toInt();
        } else {
            item.port = Params().GetDefaultPort();
        }
        item.tableIndex = 0;
        item.row = i;
        items.push_back(item);
    }

    for (int i = 0; i < githubTable->rowCount(); i++) {
        ConnectivityChecker::CheckItem item;
        item.host = githubTable->item(i, 0)->text();
        if (item.host.contains(":")) {
            QStringList parts = item.host.split(":");
            item.host = parts[0];
            item.port = parts[1].toInt();
        } else {
            item.port = Params().GetDefaultPort();
        }
        item.tableIndex = 1;
        item.row = i;
        items.push_back(item);
    }

    if (!items.empty()) {
        checker = new ConnectivityChecker(this);
        checker->setItems(items);
        connect(checker, &ConnectivityChecker::checkResult, this, &InfoPage::onCheckResult);
        connect(checker, &QThread::finished, checker, &QObject::deleteLater);
        refreshButton->setEnabled(false);
        connect(checker, &QThread::finished, this, [this]() {
            refreshButton->setEnabled(true);
            checker = nullptr;
        });
        checker->start();
    }
}

void InfoPage::onCheckResult(int tableIndex, int row, bool reachable)
{
    QTableWidget *table = nullptr;
    if (tableIndex == 0) table = hardcodedTable;
    else if (tableIndex == 1) table = githubTable;
    else if (tableIndex == 2) table = discoveredTable;

    if (!table || row >= table->rowCount()) return;

    int statusCol = table->columnCount() - 1;
    QTableWidgetItem *statusItem = table->item(row, statusCol);
    if (statusItem) {
        statusItem->setForeground(reachable ? QColor(0, 180, 0) : QColor(220, 0, 0));
    }
}
