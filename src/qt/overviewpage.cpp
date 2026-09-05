// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/rewardschart.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/askpassphrasedialog.h>
#include <interfaces/node.h>

#include <qt/asyncrpc.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <algorithm>

#include <QStyle>
#include <QDateTime>
#include <QEvent>
#include <QAbstractItemDelegate>
#include <QMessageBox>
#include <QBoxLayout>
#include <QFrame>
#include <QApplication>
#include <QPainter>
#include <QDateTime>
#include <QUrl>
#include <QPushButton>
#include <QTimer>

#define DECORATION_SIZE 24
//! Row height is independent of the icon: the row holds two lines of text, and tying
//! its height to the icon is what clipped them when the icon shrank.
#define ROW_HEIGHT 46
//! Breathing room so the icon does not sit against the panel border.
#define ROW_PADDING 10
#define NUM_ITEMS 5

Q_DECLARE_METATYPE(interfaces::WalletBalances)

//! The Taler blue from the project mark. Transaction icons are drawn in it rather
//! than in the text colour: white icons vanish on a light background, and at this
//! size the accent reads as part of the brand instead of as decoration.
static const QColor TALER_ACCENT(0x00, 0x88, 0xcc);

//! Recolour an icon, keeping its alpha. QIcon has no tint of its own.
static QIcon ColorizeIcon(const QIcon& icon, const QColor& colour)
{
    QPixmap pixmap = icon.pixmap(QSize(DECORATION_SIZE, DECORATION_SIZE) * 2);
    if (pixmap.isNull()) return icon;
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), colour);
    painter.end();
    return QIcon(pixmap);
}

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(BitcoinUnits::BTC),
        platformStyle(_platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        // Icon padded from the left edge and centred against the two text lines.
        QRect decorationRect(mainRect.left() + ROW_PADDING,
                             mainRect.top() + (mainRect.height() - DECORATION_SIZE) / 2,
                             DECORATION_SIZE, DECORATION_SIZE);
        int xspace = ROW_PADDING + DECORATION_SIZE + 12;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        const int textWidth = mainRect.width() - xspace - ROW_PADDING; // keep off the right edge too
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, textWidth, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, textWidth, halfheight);
        icon = ColorizeIcon(icon, TALER_ACCENT);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top()+ypad+halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::separatorAlways);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, ROW_HEIGHT);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include <qt/overviewpage.moc>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    txdelegate(new TxViewDelegate(platformStyle, this)),
    stakingTickTimer(nullptr),
    stakingDurationSeconds(0)
{
    ui->setupUi(this);

    m_balances.balance = -1;

    // ---- visual hierarchy -------------------------------------------------
    // Card titles get one treatment everywhere instead of ad-hoc bold labels, and
    // the balance is promoted to the hero it should be: it is the number the whole
    // screen exists to show, and it was rendered the same size as its own caption.
    // Headline balance in a badge of its own, above the Balances card. The card
    // keeps its plain rows: enlarging the number in place overlapped its caption,
    // because the form's grid was laid out for text of one size.
    m_balance_badge = new QFrame(this);
    m_balance_badge->setObjectName("balanceBadge");
    QVBoxLayout* badgeLayout = new QVBoxLayout(m_balance_badge);
    badgeLayout->setContentsMargins(18, 12, 18, 14);
    badgeLayout->setSpacing(2);
    m_badge_caption = new QLabel(tr("Available balance"), m_balance_badge);
    m_badge_caption->setObjectName("balanceBadgeCaption");
    m_badge_value = new QLabel(QString(), m_balance_badge);
    m_badge_value->setObjectName("balanceBadgeValue");
    m_badge_value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    badgeLayout->addWidget(m_badge_caption);
    badgeLayout->addWidget(m_badge_value);

    // Which wallet is on screen, stated plainly on the main screen rather than only
    // in the toolbar selector.
    // Reserves the row the wallet bar is placed into; it names the wallet only
    // until the bar arrives, which is the case before any wallet is loaded.
    m_wallet_name_label = new QLabel(this);
    m_wallet_name_label->setProperty("class", "walletName");
    m_wallet_name_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (QBoxLayout* page = qobject_cast<QBoxLayout*>(layout())) {
        page->insertWidget(0, m_wallet_name_label);
    }
    // Labels on the left, figures on the right: a column of amounts is read down
    // its last digit, which only works if they share a right edge.
    ui->gridLayout->setColumnStretch(1, 1);
    buildRecoveryRow();

    buildRewardsColumn();

    // Inside the left column, directly above the Balances card. At page level it
    // spanned both columns and pushed the transaction list down with it.
    ui->verticalLayout_2->insertWidget(0, m_balance_badge);

    ui->label_5->setProperty("class", "cardTitle");          // Balances
    ui->label_4->setProperty("class", "cardTitle");          // Recent transactions
    ui->labelStakingTitle->setProperty("class", "cardTitle");
    ui->labelStakingSubtitle->setProperty("class", "cardHint");


    // Empty state: a blank panel reads as broken on a fresh wallet.
    // Parented to the list itself: as a child of the page, its geometry was in the
    // page's coordinates and the message landed on top of the Balances card.
    m_empty_transactions = new QLabel(tr("No transactions yet.\nIncoming and outgoing payments will appear here."), ui->listTransactions);
    m_empty_transactions->setAlignment(Qt::AlignCenter);
    m_empty_transactions->setProperty("class", "emptyState");
    m_empty_transactions->setWordWrap(true);
    m_empty_transactions->hide();
    // The list resizes with the column without the page resizing, and the message
    // is positioned by hand, so it has to hear about the list's own resizes too -
    // otherwise it keeps whatever width it had when it was first placed.
    ui->listTransactions->installEventFilter(this);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (ROW_HEIGHT + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));

    // Staking controls: seconds stored as userData on each combo entry.
    ui->stakingDurationCombo->setItemData(0, static_cast<qlonglong>(3600));
    ui->stakingDurationCombo->setItemData(1, static_cast<qlonglong>(6 * 3600));
    ui->stakingDurationCombo->setItemData(2, static_cast<qlonglong>(24 * 3600));
    ui->stakingDurationCombo->setItemData(3, static_cast<qlonglong>(7 * 24 * 3600));
    ui->stakingDurationCombo->setItemData(4, static_cast<qlonglong>(30 * 24 * 3600));
    ui->stakingDurationCombo->setCurrentIndex(2); // default 24 hours

    stakingTickTimer = new QTimer(this);
    stakingTickTimer->setInterval(1000);
    connect(stakingTickTimer, SIGNAL(timeout()), this, SLOT(tickStakingTimer()));

    connect(ui->startStakingButton, SIGNAL(clicked()), this, SLOT(onStartStakingClicked()));
    connect(ui->stopStakingButton, SIGNAL(clicked()), this, SLOT(onStopStakingClicked()));
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

//! Headline amount, cut to two decimals. The badge is for reading at a glance; the
//! exact figure to the last unit stays on the Balances rows underneath.
static QString FormatBadgeAmount(int unit, const CAmount& amount)
{
    QString text = BitcoinUnits::format(unit, amount, false, BitcoinUnits::separatorAlways);
    const int point = text.indexOf(QChar('.'));
    if (point >= 0) text.truncate(point + 3);
    return text + QString(" ") + BitcoinUnits::shortName(unit);
}

namespace {
//! One status badge: a caption, a headline, and an optional quieter line under it.
QFrame* MakeStatusBadge(QWidget* parent, QLabel** title, QLabel** value, QLabel** hint)
{
    QFrame* badge = new QFrame(parent);
    badge->setProperty("badge", true);
    badge->setProperty("badgeState", "idle");
    QVBoxLayout* layout = new QVBoxLayout(badge);
    layout->setContentsMargins(14, 10, 14, 12);
    layout->setSpacing(1);

    *title = new QLabel(badge);
    (*title)->setProperty("badgeCaption", true);
    *value = new QLabel(badge);
    (*value)->setProperty("badgeValue", true);
    layout->addWidget(*title);
    layout->addWidget(*value);

    if (hint) {
        *hint = new QLabel(badge);
        (*hint)->setProperty("badgeCaption", true);
        layout->addWidget(*hint);
    }
    return badge;
}
} // namespace

//! The last row of the Balances card: what to do when the balance looks wrong.
//!
//! Deliberately quiet and deliberately permanent. Quiet because a wallet that
//! shouts about repairing itself does not inspire much confidence in the number
//! above it; permanent because the case it exists for - a wallet restored from
//! an older backup, which never knew about the addresses it used since - looks
//! exactly like a correct wallet from the inside, so there is nothing for the
//! application to detect and no moment at which to reveal a hidden button.
void OverviewPage::buildRecoveryRow()
{
    m_recovery_row = new QWidget(this);
    // Named so the stylesheet can stop it painting the window colour over the card.
    m_recovery_row->setObjectName("recoveryRow");
    QHBoxLayout* layout = new QHBoxLayout(m_recovery_row);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(8);

    m_recovery_hint = new QLabel(m_recovery_row);
    m_recovery_hint->setProperty("class", "cardHint");
    m_recovery_hint->setWordWrap(true);
    m_recovery_hint->setText(tr("Balance looks wrong?"));
    layout->addWidget(m_recovery_hint, 1);

    m_recovery_button = new QPushButton(tr("Rescan wallet"), m_recovery_row);
    m_recovery_button->setToolTip(tr("Search the whole chain again for this wallet's "
                                     "history, deriving addresses beyond the ones it "
                                     "already knows about"));
    m_recovery_button->setCursor(Qt::PointingHandCursor);
    connect(m_recovery_button, &QPushButton::clicked, this, &OverviewPage::recoverBalance);
    layout->addWidget(m_recovery_button, 0);

    ui->verticalLayout_4->addWidget(m_recovery_row);
}

void OverviewPage::recoverBalance()
{
    if (!walletModel) return;

    // A scan of a chain the node has not finished downloading reports a
    // plausible, wrong balance - the very thing the user came here to fix. The
    // node refuses it too, but being told why here beats a raw RPC error.
    //
    // "Up to date" is the same test the status bar uses: a tip less than ninety
    // minutes old. Two different answers to "am I synced?" in one window would be
    // worse than either.
    interfaces::Node& node = walletModel->node();
    const int64_t tip_age = QDateTime::currentDateTime().toSecsSinceEpoch() -
                            static_cast<int64_t>(node.getLastBlockTime());
    if (node.isInitialBlockDownload() || tip_age >= 90 * 60) {
        QMessageBox::information(
            this, tr("Rescan wallet"),
            tr("The node is still catching up with the network, so a rescan is not "
               "possible yet.\n\nSearching an incomplete chain would report a balance "
               "that looks believable but is wrong. Wait until the status bar shows "
               "the node is up to date, then try again."));
        return;
    }

    // Say the cost before asking, not after. On this chain a full scan reads
    // every block from disk, and the wallet is unusable while it runs.
    QMessageBox confirm(this);
    confirm.setWindowTitle(tr("Rescan wallet"));
    confirm.setIcon(QMessageBox::Question);
    confirm.setText(tr("Search the whole chain for this wallet's history?"));
    confirm.setInformativeText(tr(
        "Use this when the balance looks lower than it should - typically after "
        "restoring a wallet from an older backup, because the wallet does not know "
        "about the addresses it used after that backup was taken.\n\n"
        "The whole chain is read again and the address list is extended as history "
        "is found. This takes several minutes, the wallet cannot be used while it "
        "runs, and the wallet must be unlocked."));
    QPushButton* start = confirm.addButton(tr("Rescan"), QMessageBox::AcceptRole);
    QPushButton* cancel = confirm.addButton(tr("Cancel"), QMessageBox::RejectRole);
    confirm.setDefaultButton(cancel);
    confirm.setEscapeButton(cancel);
    confirm.exec();
    if (confirm.clickedButton() != start) return;

    // Deriving addresses needs the keys. Asking here rather than letting the scan
    // fail halfway is the difference between a prompt and a wasted ten minutes.
    std::unique_ptr<WalletModel::UnlockContext> unlock(
        new WalletModel::UnlockContext(walletModel->requestUnlock()));
    if (!unlock->isValid()) return;

    m_recovery_button->setEnabled(false);

    // Off the GUI thread, behind a modal progress dialog: this reads every block
    // on the chain, and a frozen window for that long looks like a crash.
    const QByteArray encoded = QUrl::toPercentEncoding(walletModel->getWalletName());
    const std::string uri = "/wallet/" + std::string(encoded.constData(), encoded.length());
    UniValue result;
    QString error;
    const bool ok = RunNodeRpc(walletModel->node(), QStringLiteral("recoverwallet"),
                               UniValue(UniValue::VARR), uri, result, error, this,
                               tr("Searching the chain for this wallet's history. "
                                  "This can take several minutes."));

    m_recovery_button->setEnabled(true);

    if (!ok) {
        QMessageBox::warning(this, tr("Rescan wallet"),
                             tr("The rescan could not be completed.\n\n%1").arg(error));
        return;
    }

    // Say what it found, so the answer to "did that do anything?" is on screen
    // rather than left to the user to work out from the balance.
    const int passes = result.exists("passes") ? result["passes"].get_int() : 0;
    QMessageBox::information(
        this, tr("Rescan wallet"),
        tr("The rescan finished after %n pass(es). The balance above is up to date.",
           "", qMax(1, passes)));
}

void OverviewPage::buildRewardsColumn()
{
    // Two badges rather than one: the left says whether this wallet is earning,
    // the right says what can be done next. They are allowed to disagree - a wallet
    // that staked yesterday is healthy and also cannot start a new round yet.
    m_staking_badge = MakeStatusBadge(this, &m_staking_badge_title, &m_staking_badge_value,
                                      &m_staking_badge_hint);
    m_ready_badge = MakeStatusBadge(this, &m_ready_badge_title, &m_ready_badge_value, nullptr);
    m_staking_badge_title->setText(tr("Staking"));
    m_ready_badge_title->setText(tr("New round"));

    QHBoxLayout* badges = new QHBoxLayout();
    badges->setContentsMargins(0, 0, 0, 0);
    badges->setSpacing(12);
    badges->addWidget(m_staking_badge, 1);
    badges->addWidget(m_ready_badge, 1);

    // The chart, in a card of the same shape as every other panel.
    m_rewards_card = new QFrame(this);
    m_rewards_card->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* card_layout = new QVBoxLayout(m_rewards_card);
    card_layout->setContentsMargins(14, 12, 14, 10);
    card_layout->setSpacing(6);

    QHBoxLayout* header = new QHBoxLayout();
    QLabel* title = new QLabel(tr("Rewards, last 12 months"), m_rewards_card);
    title->setProperty("class", "cardTitle");
    // The total of the twelve bars, stated next to them so the chart does not have
    // to be read to know what the year came to.
    m_rewards_total = new QLabel(m_rewards_card);
    m_rewards_total->setProperty("class", "cardTotal");
    m_rewards_total->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_rewards_total->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_rewards_total);

    m_rewards_chart = new RewardsChart(m_rewards_card);
    card_layout->addLayout(header);
    card_layout->addWidget(m_rewards_chart);

    ui->verticalLayout_3->insertLayout(0, badges);
    ui->verticalLayout_3->insertWidget(1, m_rewards_card);

    // Qt does not re-evaluate a stylesheet when a dynamic property changes, so the
    // refresh below repolishes; do the same here to pick up the initial state.
    m_rewards_timer = new QTimer(this);
    m_rewards_timer->setSingleShot(true);
    m_rewards_timer->setInterval(250);
    connect(m_rewards_timer, SIGNAL(timeout()), this, SLOT(refreshRewards()));
    refreshRewards();
}

void OverviewPage::applyBadgeState(QWidget* badge, const char* state)
{
    if (!badge) return;
    if (badge->property("badgeState").toString() == QLatin1String(state)) return;

    badge->setProperty("badgeState", state);
    // A dynamic property used by a selector needs an explicit repolish, including
    // for the labels inside, whose colour is chosen by the same selector.
    badge->style()->unpolish(badge);
    badge->style()->polish(badge);
    for (QLabel* label : badge->findChildren<QLabel*>()) {
        label->style()->unpolish(label);
        label->style()->polish(label);
    }
    badge->update();
}

void OverviewPage::scheduleRewardsRefresh()
{
    if (m_rewards_timer && !m_rewards_timer->isActive()) m_rewards_timer->start();
}

void OverviewPage::refreshRewards()
{
    const QDateTime now = QDateTime::currentDateTime();

    if (walletModel && walletModel->getTransactionTableModel()) {
        m_rewards = walletModel->getTransactionTableModel()->summariseRewards(now);
    } else {
        m_rewards = RewardSummary();
    }

    const int unit = walletModel && walletModel->getOptionsModel()
                         ? walletModel->getOptionsModel()->getDisplayUnit()
                         : BitcoinUnits::BTC;

    // --- how recently this wallet earned ---
    const int days = m_rewards.daysSinceStaking(now);
    if (days < 0) {
        m_staking_badge_value->setText(tr("Never"));
        applyBadgeState(m_staking_badge, "bad");
    } else {
        m_staking_badge_value->setText(days == 0 ? tr("Today")
                                                 : tr("%n day(s) ago", "", days));
        applyBadgeState(m_staking_badge, days < 7 ? "good" : (days <= 30 ? "warn" : "bad"));
    }

    // Mining is a footnote: on a proof-of-stake chain most wallets never mine, and
    // an always-empty line is noise.
    const int mined_days = m_rewards.daysSinceMining(now);
    m_staking_badge_hint->setVisible(mined_days >= 0);
    if (mined_days >= 0) {
        m_staking_badge_hint->setText(mined_days == 0 ? tr("Mined today")
                                                      : tr("Mined %n day(s) ago", "", mined_days));
    }

    // --- whether a new round can start ---
    if (m_rewards.readyToStake(now)) {
        m_ready_badge_value->setText(tr("Ready"));
        applyBadgeState(m_ready_badge, "good");
    } else {
        const int wait_days = int((m_rewards.secondsUntilReady(now) + 24 * 60 * 60 - 1) / (24 * 60 * 60));
        m_ready_badge_value->setText(tr("in %n day(s)", "", wait_days));
        // Not a fault, so never red: it is simply not time yet.
        applyBadgeState(m_ready_badge, "idle");
    }

    // --- the chart and its total ---
    if (m_rewards_chart) m_rewards_chart->setSummary(m_rewards, unit);
    if (m_rewards_total) {
        m_rewards_total->setText(m_rewards.total > 0
            ? BitcoinUnits::formatWithUnit(unit, m_rewards.total, false, BitcoinUnits::separatorAlways)
            : QString());
    }
}

bool OverviewPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->listTransactions && event->type() == QEvent::Resize) {
        updateTransactionsPlaceholder();
    }
    return QWidget::eventFilter(watched, event);
}

void OverviewPage::setWalletBar(QWidget* bar)
{
    if (!bar) return;
    // The bar is shared between wallet views, so it arrives here already parented to
    // another page. Inserting it into this layout reparents it and Qt drops it from
    // the previous one; the label it stands in for is hidden while it is present.
    if (QBoxLayout* page = qobject_cast<QBoxLayout*>(layout())) {
        page->insertWidget(0, bar);
        bar->show();
    }
    m_wallet_bar_shown = true;
    m_wallet_name_label->hide();
}

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    m_balances = balances;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balances.balance, false, BitcoinUnits::separatorAlways));
    if (m_badge_value) m_badge_value->setText(FormatBadgeAmount(unit, balances.balance));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, balances.unconfirmed_balance, false, BitcoinUnits::separatorAlways));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, balances.immature_balance, false, BitcoinUnits::separatorAlways));
    ui->labelTotal->setText(BitcoinUnits::formatWithUnit(unit, balances.balance + balances.unconfirmed_balance + balances.immature_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchAvailable->setText(BitcoinUnits::formatWithUnit(unit, balances.watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchPending->setText(BitcoinUnits::formatWithUnit(unit, balances.unconfirmed_watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchImmature->setText(BitcoinUnits::formatWithUnit(unit, balances.immature_watch_only_balance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchTotal->setText(BitcoinUnits::formatWithUnit(unit, balances.watch_only_balance + balances.unconfirmed_watch_only_balance + balances.immature_watch_only_balance, false, BitcoinUnits::separatorAlways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = balances.immature_balance != 0;
    bool showWatchOnlyImmature = balances.immature_watch_only_balance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance

    if (!showWatchOnly)
        ui->labelWatchImmature->hide();
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::updateTransactionsPlaceholder()
{
    if (!m_empty_transactions) return;
    const bool empty = !ui->listTransactions->model() || ui->listTransactions->model()->rowCount() == 0;
    m_empty_transactions->setVisible(empty);
    if (empty) {
        m_empty_transactions->setGeometry(ui->listTransactions->rect());
        m_empty_transactions->raise();
    }
}

void OverviewPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateTransactionsPlaceholder();
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    if (m_wallet_name_label) {
        m_wallet_name_label->setText(model ? tr("Wallet: %1").arg(GUIUtil::walletDisplayName(model->getWalletName()))
                                           : QString());
        m_wallet_name_label->setVisible(model != nullptr && !m_wallet_bar_shown);
    }
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        connect(filter.get(), &QAbstractItemModel::rowsInserted, this, &OverviewPage::updateTransactionsPlaceholder);
        connect(filter.get(), &QAbstractItemModel::rowsRemoved, this, &OverviewPage::updateTransactionsPlaceholder);
        connect(filter.get(), &QAbstractItemModel::modelReset, this, &OverviewPage::updateTransactionsPlaceholder);
        updateTransactionsPlaceholder();
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        interfaces::Wallet& wallet = model->wallet();
        interfaces::WalletBalances balances = wallet.getBalances();
        setBalance(balances);
        connect(model, SIGNAL(balanceChanged(interfaces::WalletBalances)), this, SLOT(setBalance(interfaces::WalletBalances)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(wallet.haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));

        connect(model, SIGNAL(encryptionStatusChanged()), this, SLOT(updateStakingUi()));

        // Rewards follow the transaction list. Every one of these can fire in bursts
        // during a rescan, so they only ask for a refresh; the timer decides when.
        if (TransactionTableModel* transactions = model->getTransactionTableModel()) {
            connect(transactions, SIGNAL(modelReset()), this, SLOT(scheduleRewardsRefresh()));
            connect(transactions, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(scheduleRewardsRefresh()));
            connect(transactions, SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(scheduleRewardsRefresh()));
            connect(transactions, SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(scheduleRewardsRefresh()));
        }
        updateStakingUi();
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
    refreshRewards();
}

QString OverviewPage::formatStakingRemaining(int64_t seconds) const
{
    if (seconds < 0) seconds = 0;
    const int days = seconds / 86400;
    const int hours = (seconds % 86400) / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;
    return QString("%1d %2h %3m %4s")
        .arg(days)
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

void OverviewPage::onStartStakingClicked()
{
    if (!walletModel) return;
    if (walletModel->getEncryptionStatus() == WalletModel::Unencrypted) return;

    const int64_t seconds = ui->stakingDurationCombo->currentData().toLongLong();
    if (seconds <= 0) return;

    AskPassphraseDialog dlg(AskPassphraseDialog::UnlockStaking, this);
    dlg.setModel(walletModel);
    dlg.setStakingDuration(seconds);
    if (dlg.exec() == QDialog::Accepted) {
        stakingDurationSeconds = seconds;
        stakingTickTimer->start();
        updateStakingUi();
    }
}

void OverviewPage::onStopStakingClicked()
{
    if (!walletModel) return;
    if (QMessageBox::question(this, tr("Stop staking?"),
            tr("Are you sure you want to stop staking?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    walletModel->stopStaking();
    updateStakingUi();
}

void OverviewPage::updateStakingUi()
{
    if (!walletModel) return;

    const bool unencrypted = walletModel->getEncryptionStatus() == WalletModel::Unencrypted;
    ui->stakingFrame->setVisible(!unencrypted);
    if (unencrypted) {
        stakingTickTimer->stop();
        stakingDurationSeconds = 0;
        return;
    }

    const int64_t remaining = walletModel->getStakingSecondsRemaining();
    const bool staking = remaining > 0 && !walletModel->wallet().isLocked();

    ui->startStakingButton->setVisible(!staking);
    ui->stakingDurationCombo->setEnabled(!staking);
    ui->stopStakingButton->setVisible(staking);

    if (staking) {
        ui->stakingStatusLabel->setText(tr("Staking — %1 remaining").arg(formatStakingRemaining(remaining)));
        if (stakingDurationSeconds > 0) {
            const int pct = static_cast<int>(100 * (stakingDurationSeconds - remaining) / stakingDurationSeconds);
            ui->stakingProgressBar->setValue(std::max(0, std::min(100, pct)));
        } else {
            ui->stakingProgressBar->setValue(0);
        }
        if (!stakingTickTimer->isActive()) stakingTickTimer->start();
    } else {
        ui->stakingStatusLabel->setText(tr("Not staking"));
        ui->stakingProgressBar->setValue(0);
        stakingTickTimer->stop();
        stakingDurationSeconds = 0;
    }
}

void OverviewPage::tickStakingTimer()
{
    updateStakingUi();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if (m_balances.balance != -1) {
            setBalance(m_balances);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();

        // The chart's tooltips and its twelve-month total are amounts too.
        refreshRewards();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}
