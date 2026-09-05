// Copyright (c) 2024 The Taler developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/infopage.h>
#include <qt/clientmodel.h>
#include <qt/platformstyle.h>

#include <chainparams.h>
#include <chainparamsseeds.h>
#include <hash.h>
#include <net.h>
#include <netbase.h>
#include <protocol.h>
#include <streams.h>
#include <utilstrencodings.h>
#include <utiltime.h>
#include <version.h>

#include <QDesktopServices>
#include <QHeaderView>
#include <QMessageBox>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

#include <algorithm>
#include <functional>

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

namespace {

//! Largest version payload this probe will read.
//!
//! A real version message is a couple of hundred bytes. The cap exists so a
//! hostile or broken seed cannot make the page allocate against a length field
//! it made up; anything larger is treated as garbage and the probe gives up.
static const unsigned int MAX_PROBE_PAYLOAD = 32 * 1024;

//! How many messages to step over while looking for the peer's version.
//! In practice it is the first one; the budget is only so a peer that opens with
//! something else cannot hold the sweep on one host.
static const int MAX_PROBE_MESSAGES = 4;

//! Read exactly `length` bytes, or fail before `deadline_ms`.
//!
//! Sliced rather than one long blocking read so that leaving the page stops the
//! sweep promptly instead of after the full timeout on every remaining host.
bool RecvExact(int sock, char* buffer, size_t length, int64_t deadline_ms,
               const std::function<bool()>& interrupted)
{
    size_t received = 0;
    while (received < length) {
        if (interrupted()) return false;
        const int64_t remaining = deadline_ms - GetTimeMillis();
        if (remaining <= 0) return false;

        const int slice = static_cast<int>(std::min<int64_t>(remaining, 100));
#ifdef WIN32
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = slice * 1000;
        const int ready = select(sock + 1, &rfds, nullptr, nullptr, &tv);
#else
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;
        const int ready = poll(&pfd, 1, slice);
#endif
        if (ready < 0) return false;
        if (ready == 0) continue; // nothing yet; the deadline decides when to stop

        const int got = recv(sock, buffer + received, static_cast<int>(length - received), 0);
        if (got <= 0) return false; // closed or errored
        received += static_cast<size_t>(got);
    }
    return true;
}

//! The version message this probe introduces itself with.
//!
//! It announces no services, no height and no transaction relay, and carries
//! this node's real user agent rather than a made-up one: the seed operator
//! reading their own log should see who actually asked.
std::vector<char> BuildVersionMessage()
{
    CDataStream payload(SER_NETWORK, INIT_PROTO_VERSION);
    payload << static_cast<int32_t>(PROTOCOL_VERSION)
            << static_cast<uint64_t>(NODE_NONE)
            << static_cast<int64_t>(GetTime())
            << CAddress(CService(), NODE_NONE)  // the address of the peer, unknown to us
            << CAddress(CService(), NODE_NONE)  // our own, deliberately not advertised
            << static_cast<uint64_t>(0)         // nonce, unused: we never keep the link
            << GetSubVersion()
            << static_cast<int32_t>(0)          // start height
            << false;                           // do not send us transactions

    CMessageHeader header(Params().MessageStart(), NetMsgType::VERSION,
                          static_cast<unsigned int>(payload.size()));
    const uint256 checksum = Hash(payload.begin(), payload.end());
    memcpy(header.pchChecksum, checksum.begin(), CMessageHeader::CHECKSUM_SIZE);

    CDataStream message(SER_NETWORK, INIT_PROTO_VERSION);
    message << header;
    message.write(payload.data(), payload.size());
    return std::vector<char>(message.begin(), message.end());
}

} // namespace

// ConnectivityChecker implementation
bool ConnectivityChecker::probeSubVersion(int sock, int timeoutSec, std::string& subVersion)
{
    const int64_t deadline = GetTimeMillis() + static_cast<int64_t>(timeoutSec) * 1000;
    const auto interrupted = [this]() { return isInterruptionRequested(); };

    const std::vector<char> greeting = BuildVersionMessage();
    size_t sent = 0;
    while (sent < greeting.size()) {
        if (interrupted() || GetTimeMillis() >= deadline) return false;
        const int wrote = send(sock, greeting.data() + sent,
                               static_cast<int>(greeting.size() - sent), 0);
        if (wrote <= 0) return false;
        sent += static_cast<size_t>(wrote);
    }

    for (int message = 0; message < MAX_PROBE_MESSAGES; ++message) {
        std::vector<char> raw_header(CMessageHeader::HEADER_SIZE);
        if (!RecvExact(sock, raw_header.data(), raw_header.size(), deadline, interrupted))
            return false;

        CMessageHeader header(Params().MessageStart());
        std::string command;
        unsigned int size = 0;
        try {
            CDataStream stream(raw_header, SER_NETWORK, INIT_PROTO_VERSION);
            stream >> header;
            if (!header.IsValid(Params().MessageStart())) return false;
            command = header.GetCommand();
            size = header.nMessageSize;
        } catch (const std::exception&) {
            return false; // not something this network speaks
        }
        if (size > MAX_PROBE_PAYLOAD) return false;

        std::vector<char> body(size);
        if (size > 0 && !RecvExact(sock, body.data(), body.size(), deadline, interrupted))
            return false;
        if (command != NetMsgType::VERSION) continue;

        try {
            CDataStream stream(body, SER_NETWORK, INIT_PROTO_VERSION);
            int32_t remote_version = 0;
            uint64_t services = 0;
            int64_t timestamp = 0;
            CAddress addr_me, addr_from;
            uint64_t nonce = 0;
            std::string user_agent;
            stream >> remote_version >> services >> timestamp >> addr_me;
            // Everything past this point is optional in the protocol. A peer old
            // enough to omit it simply has no user agent to show.
            if (stream.empty()) return false;
            stream >> addr_from >> nonce >> LIMITED_STRING(user_agent, MAX_SUBVERSION_LENGTH);
            subVersion = SanitizeString(user_agent);
            return !subVersion.empty();
        } catch (const std::exception&) {
            return false;
        }
    }
    return false;
}

bool ConnectivityChecker::tcpConnect(const std::string& host, int port, int timeoutSec,
                                     bool& resolved, std::string& address,
                                     std::string& subVersion)
{
    resolved = false;
    address.clear();
    subVersion.clear();

    struct addrinfo hints{}, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0)
        return false;

    // It resolved, whatever happens next. Recording the first address here rather
    // than only on success is the point: a name that resolves but does not answer
    // is a different problem from one that does not resolve, and the page can
    // only say so if it knows what the name pointed at.
    resolved = (res != nullptr);
    if (res != nullptr) {
        char text[NI_MAXHOST] = {0};
        if (getnameinfo(res->ai_addr, res->ai_addrlen, text, sizeof(text),
                        nullptr, 0, NI_NUMERICHOST) == 0) {
            address = text;
        }
    }

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
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                bool done = false;
                for (int waited = 0; waited < timeoutSec * 1000 && !done; waited += 100) {
                    if (isInterruptionRequested()) break;
                    FD_ZERO(&wfds);
                    FD_SET(sock, &wfds);
                    tv.tv_sec = 0;
                    tv.tv_usec = 100000;
                    if (select(sock + 1, nullptr, &wfds, nullptr, &tv) > 0) done = true;
                }
                if (done) {
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
                // Waited in slices so that an interruption is noticed promptly:
                // a single blocking poll of several seconds per host made stopping
                // a sweep take far longer than a user switching tabs will wait.
                for (int waited = 0; waited < timeoutSec * 1000; waited += 100) {
                    if (isInterruptionRequested()) break;
                    int ready = poll(&pfd, 1, 100);
                    if (ready < 0) break;
                    if (ready > 0) {
                        int err = 0;
                        socklen_t len = sizeof(err);
                        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
                        connected = (err == 0);
                        break;
                    }
                }
            }
#endif
        }

        // Ask while the socket is still open. A failed or refused handshake is not
        // a failed check: the host answered, which is what the status dot claims.
        if (connected) probeSubVersion(sock, timeoutSec, subVersion);

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
        bool resolved = false;
        std::string address;
        std::string subVersion;
        const bool reachable = tcpConnect(item.host.toStdString(), item.port, 3, resolved,
                                          address, subVersion);
        if (isInterruptionRequested()) return;
        Q_EMIT checkResult(m_generation, item.tableIndex, item.row, resolved, reachable,
                           QString::fromStdString(address),
                           QString::fromStdString(subVersion));
    }
}

// InfoPage implementation
namespace {

//! The three things worth pulling out of a peer's BIP-14 user agent.
struct PeerVersion
{
    QString version;
    QString mode;
    QString api;
};

//! Split "/Taler:0.20.0(SERV; api:1.2.0; eu-1)/" into its parts.
//!
//! Tolerant by design: peers predating this run older clients that send no
//! comments at all, and any peer may send something unexpected. Anything not
//! recognised is simply left out rather than guessed at.
PeerVersion ParsePeerSubVersion(QString subVersion)
{
    PeerVersion parsed;
    subVersion.remove(QChar('/'));

    const int colon = subVersion.indexOf(QChar(':'));
    if (colon >= 0) subVersion = subVersion.mid(colon + 1);

    const int open_paren = subVersion.indexOf(QChar('('));
    if (open_paren < 0) {
        parsed.version = subVersion.trimmed();
        return parsed;
    }

    parsed.version = subVersion.left(open_paren).trimmed();
    QString comments = subVersion.mid(open_paren + 1);
    const int close_paren = comments.lastIndexOf(QChar(')'));
    if (close_paren >= 0) comments = comments.left(close_paren);

    for (const QString& raw : comments.split(QChar(';'))) {
        const QString comment = raw.trimmed();
        if (comment == QLatin1String("GUI") || comment == QLatin1String("SERV")) {
            parsed.mode = comment;
        } else if (comment.startsWith(QLatin1String("api:"))) {
            parsed.api = comment.mid(4);
        }
    }
    return parsed;
}

} // namespace

InfoPage::InfoPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    clientModel(nullptr),
    platformStyle(_platformStyle),
    checker(nullptr),
    m_generation(0)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Top bar: what this page is, then the one action it offers. The left of this
    // row was empty, which left the page opening on an unexplained pair of tables.
    QHBoxLayout *topBar = new QHBoxLayout();

    QVBoxLayout *heading = new QVBoxLayout();
    heading->setSpacing(2);
    QLabel *pageTitle = new QLabel(tr("Nodes"));
    pageTitle->setProperty("class", "pageTitle");
    heading->addWidget(pageTitle);
    QLabel *pageSubtitle = new QLabel(
        tr("Seed nodes this wallet starts from, and the peers it has found on the network."));
    pageSubtitle->setProperty("class", "pageSubtitle");
    pageSubtitle->setWordWrap(true);
    heading->addWidget(pageSubtitle);
    topBar->addLayout(heading, 1);

    refreshButton = new QPushButton(tr("Refresh"));
    connect(refreshButton, &QPushButton::clicked, this, &InfoPage::refreshData);
    // Level with the title rather than centred against two lines of text.
    topBar->addWidget(refreshButton, 0, Qt::AlignTop);
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
    // Mode and API come out of the peer's BIP-14 comment list, which older peers
    // do not fill in - their cells stay blank rather than claiming anything.
    //
    // "From" sits immediately left of the status dot and says whether this peer
    // is one of the seeds on the left or one the node found on its own. Status
    // stays last so columnCount() - 1 keeps finding it.
    discoveredTable->setColumnCount(6);
    discoveredTable->setHorizontalHeaderLabels(
        {tr("Node"), tr("Version"), tr("Mode"), tr("API"), tr("From"), tr("Status")});
    discoveredTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column <= 5; ++column) {
        discoveredTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    discLayout->addWidget(discoveredTable);

    // The two markers mean nothing on their own, so they are spelled out once
    // under the table rather than hidden in a tooltip nobody hovers.
    QLabel *legend = new QLabel(tr("%1 embedded seed    %2 community seed    "
                                   "no marker: found by the node itself")
                                   .arg(QString::fromUtf8("\xe2\x97\x86"))
                                   .arg(QString::fromUtf8("\xe2\x98\x85")));
    legend->setProperty("class", "pageSubtitle");
    legend->setWordWrap(true);
    discLayout->addWidget(legend);
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
        // Ask it to stop and let go of it. It is parentless and deletes itself when
        // it returns, so it is never deleted here even if it is still inside a
        // blocking name lookup - deleting a running QThread aborts the process.
        disconnect(checker, nullptr, this, nullptr);
        checker->requestInterruption();
        checker->wait(2000);
        checker = nullptr;
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

namespace {

//! A seed row before its check has come back: nothing is claimed yet.
QTableWidgetItem* MakePendingAddressItem()
{
    QTableWidgetItem* item = new QTableWidgetItem(QString::fromUtf8("\xe2\x80\xa6"));
    item->setForeground(Qt::gray);
    return item;
}

QTableWidgetItem* MakePendingVersionItem()
{
    QTableWidgetItem* item = new QTableWidgetItem(QString::fromUtf8("\xe2\x80\xa6"));
    item->setForeground(Qt::gray);
    return item;
}

QTableWidgetItem* MakePendingStatusItem()
{
    QTableWidgetItem* item = new QTableWidgetItem(QString::fromUtf8("\xe2\x97\x8f"));
    item->setForeground(Qt::gray);
    item->setTextAlignment(Qt::AlignCenter);
    item->setToolTip(QObject::tr("Checking\xe2\x80\xa6"));
    return item;
}

} // namespace

void InfoPage::setupTable(QTableWidget *table)
{
    // Status stays last: onCheckResult finds it with columnCount() - 1, which
    // keeps working for the peer table that has five columns of its own.
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(
        {tr("Node"), tr("Resolved IP"), tr("Version"), tr("Status")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column <= 3; ++column) {
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
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

    // The rows these described are gone, so the addresses they resolved to must
    // go with them: a marker left over from the previous sweep would claim a peer
    // came from a seed list that no longer says so.
    m_embedded_addresses.clear();
    m_community_addresses.clear();

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
        hardcodedTable->setItem(row, 1, MakePendingAddressItem());
        hardcodedTable->setItem(row, 2, MakePendingVersionItem());
        hardcodedTable->setItem(row, 3, MakePendingStatusItem());
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
        // Filled in from the check like every other row: a compiled-in seed is
        // already an address, but showing what was actually dialled says the
        // entry was read and used rather than merely listed.
        hardcodedTable->setItem(row, 1, MakePendingAddressItem());
        hardcodedTable->setItem(row, 2, MakePendingVersionItem());
        hardcodedTable->setItem(row, 3, MakePendingStatusItem());
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
            githubTable->setItem(row, 1, MakePendingAddressItem());
            githubTable->setItem(row, 2, MakePendingVersionItem());
            githubTable->setItem(row, 3, MakePendingStatusItem());
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

            // cleanSubVer looks like "/Taler:0.20.0(SERV; api:1.2.0; eu-1)/". Split it
            // into the number, the run mode and the API version.
            //
            // Everything here is a claim made by a remote peer, not a fact this
            // node verified. It is shown as plain table text and can do nothing,
            // but it should not be read as anything more than what the peer said.
            PeerVersion peer = ParsePeerSubVersion(QString::fromStdString(stats.cleanSubVer));
            discoveredTable->setItem(row, 1, new QTableWidgetItem(peer.version));
            discoveredTable->setItem(row, 2, new QTableWidgetItem(peer.mode));
            discoveredTable->setItem(row, 3, new QTableWidgetItem(peer.api));

            // Filled in by applySeedMarkers() once the seed checks report what
            // the names resolved to. Blank until then, and blank for good for a
            // peer the node found by itself.
            QTableWidgetItem *sourceItem = new QTableWidgetItem();
            sourceItem->setTextAlignment(Qt::AlignCenter);
            discoveredTable->setItem(row, 4, sourceItem);

            // Connected peers are green by default
            QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromUtf8("\xe2\x97\x8f"));
            statusItem->setForeground(QColor(0, 180, 0));
            statusItem->setTextAlignment(Qt::AlignCenter);
            discoveredTable->setItem(row, 5, statusItem);
        }
    }

    applySeedMarkers();
}

//! The host part of a peer address, without the port.
//!
//! Peer addresses arrive as "1.2.3.4:23153" or "[2001:db8::1]:23153", while a
//! resolved seed address is bare. A bare IPv6 address is full of colons, so the
//! port is only stripped when there is a single colon or a bracketed host -
//! otherwise the address is returned untouched rather than truncated.
static QString HostOfPeerAddress(const QString& addr)
{
    const QString trimmed = addr.trimmed();
    if (trimmed.startsWith(QLatin1Char('['))) {
        const int close = trimmed.indexOf(QLatin1Char(']'));
        if (close > 0) return trimmed.mid(1, close - 1);
        return trimmed;
    }
    if (trimmed.count(QLatin1Char(':')) == 1) {
        return trimmed.section(QLatin1Char(':'), 0, 0);
    }
    return trimmed;
}

void InfoPage::applySeedMarkers()
{
    for (int row = 0; row < discoveredTable->rowCount(); ++row) {
        QTableWidgetItem *sourceItem = discoveredTable->item(row, 4);
        QTableWidgetItem *nodeItem = discoveredTable->item(row, 0);
        if (!sourceItem || !nodeItem) continue;

        const QString host = HostOfPeerAddress(nodeItem->text());
        const bool embedded = m_embedded_addresses.contains(host);
        const bool community = m_community_addresses.contains(host);

        // Embedded wins when an address is on both lists: being compiled into the
        // wallet is the stronger statement, and the tooltip says the other half
        // rather than dropping it.
        if (embedded) {
            sourceItem->setText(QString::fromUtf8("\xe2\x97\x86"));
            sourceItem->setToolTip(community
                ? tr("An embedded seed, also listed as a community seed")
                : tr("An embedded seed - compiled into this wallet"));
        } else if (community) {
            sourceItem->setText(QString::fromUtf8("\xe2\x98\x85"));
            sourceItem->setToolTip(tr("A community seed - from the seed list on GitHub"));
        } else {
            sourceItem->setText(QString());
            sourceItem->setToolTip(tr("Found by the node itself, not from a seed list"));
        }
        sourceItem->setForeground(QColor(150, 150, 150));
    }
}

void InfoPage::refreshData()
{
    // Retire any running sweep without waiting for it and without deleting it.
    //
    // This used to be requestInterruption() + wait(5000) + delete. A sweep over
    // unreachable seeds regularly takes longer than five seconds, so wait() timed
    // out and the thread was deleted while it was still running, which aborts the
    // process - the crash seen when returning to this page before the checks had
    // finished. Leaving the page for long enough that the sweep ended first hit
    // none of this, which is why waiting made the crash disappear.
    ++m_generation;
    if (checker) {
        disconnect(checker, nullptr, this, nullptr);
        checker->requestInterruption();
        checker = nullptr; // owns itself; freed by the finished/deleteLater pair
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

    refreshButton->setEnabled(!items.empty());

    if (!items.empty()) {
        const int generation = m_generation;
        // Deliberately parentless: as a child, the page's destructor would delete it
        // mid-run. It is freed by finished -> deleteLater instead.
        checker = new ConnectivityChecker();
        checker->setItems(items);
        checker->setGeneration(generation);
        connect(checker, &ConnectivityChecker::checkResult, this, &InfoPage::onCheckResult);
        connect(checker, &QThread::finished, checker, &QObject::deleteLater);
        refreshButton->setEnabled(false);
        connect(checker, &QThread::finished, this, [this, generation]() {
            if (generation != m_generation) return; // superseded by a newer sweep
            refreshButton->setEnabled(true);
            checker = nullptr;
        });
        checker->start();
    }
}

void InfoPage::onCheckResult(int generation, int tableIndex, int row,
                             bool resolved, bool reachable, QString address, QString version)
{
    // Results already queued when a sweep was retired refer to rows that
    // populateTables() has since rebuilt, so they must not be painted.
    if (generation != m_generation) return;

    QTableWidget *table = nullptr;
    if (tableIndex == 0) table = hardcodedTable;
    else if (tableIndex == 1) table = githubTable;
    else if (tableIndex == 2) table = discoveredTable;

    if (!table || row >= table->rowCount()) return;

    // The seed tables carry the address and the version between name and status;
    // the peer table does not, because its first column is already an address and
    // its version comes from the live connection instead.
    if (tableIndex == 0 || tableIndex == 1) {
        if (QTableWidgetItem *addressItem = table->item(row, 1)) {
            addressItem->setText(resolved && !address.isEmpty()
                                     ? address
                                     : QString::fromUtf8("\xe2\x80\x94"));
            addressItem->setForeground(resolved ? QColor(150, 150, 150) : QColor(220, 0, 0));
        }
        // Now that this seed's address is known, any connected peer at that
        // address can be shown as having come from this list.
        if (resolved && !address.isEmpty()) {
            if (tableIndex == 0) m_embedded_addresses.insert(address);
            else m_community_addresses.insert(address);
            applySeedMarkers();
        }
        if (QTableWidgetItem *versionItem = table->item(row, 2)) {
            // The version is what the seed said about itself during the handshake,
            // so a seed that did not answer has none to show - an em dash, not a
            // guess from the last time or from some other row.
            const PeerVersion peer = ParsePeerSubVersion(version);
            const bool known = !peer.version.isEmpty();
            versionItem->setText(known ? peer.version : QString::fromUtf8("\xe2\x80\x94"));
            versionItem->setForeground(QColor(150, 150, 150));
            if (known) {
                QString detail = tr("Reported by the seed itself: %1").arg(version);
                if (!peer.mode.isEmpty()) {
                    detail += QLatin1Char('\n') + tr("Mode: %1").arg(peer.mode);
                }
                if (!peer.api.isEmpty()) {
                    detail += QLatin1Char('\n') + tr("API: %1").arg(peer.api);
                }
                versionItem->setToolTip(detail);
            } else {
                versionItem->setToolTip(tr("The seed did not complete a version handshake"));
            }
        }
    }

    int statusCol = table->columnCount() - 1;
    QTableWidgetItem *statusItem = table->item(row, statusCol);
    if (!statusItem) return;

    // Green has to mean the thing works, so it needs both halves. A name that
    // resolves but does not answer used to show the same red as one that does
    // not resolve at all, and a name that answered showed green even when the
    // node could never have used it - both told the operator the wrong thing.
    if (resolved && reachable) {
        statusItem->setForeground(QColor(0, 180, 0));
        statusItem->setToolTip(tr("Resolves to %1 and is answering").arg(address));
    } else if (resolved) {
        statusItem->setForeground(QColor(214, 154, 46));
        statusItem->setToolTip(tr("Resolves to %1 but is not answering on the "
                                  "peer-to-peer port").arg(address));
    } else {
        statusItem->setForeground(QColor(220, 0, 0));
        statusItem->setToolTip(tr("Does not resolve - this seed cannot be used"));
    }
}
