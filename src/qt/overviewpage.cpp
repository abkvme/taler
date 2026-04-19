// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/askpassphrasedialog.h>
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

#include <QAbstractItemDelegate>
#include <QMessageBox>
#include <QPainter>
#include <QTimer>

#define DECORATION_SIZE 54
#define NUM_ITEMS 5

Q_DECLARE_METATYPE(interfaces::WalletBalances)

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
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
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
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
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

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
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

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    m_balances = balances;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balances.balance, false, BitcoinUnits::separatorAlways));
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

void OverviewPage::setWalletModel(WalletModel *model)
{
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
        updateStakingUi();
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
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
