// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OVERVIEWPAGE_H
#define BITCOIN_QT_OVERVIEWPAGE_H

#include <interfaces/wallet.h>

#include <qt/rewardsummary.h>

#include <cstdint>

#include <QFrame>
#include <QLabel>
#include <QWidget>

class QPushButton;
#include <memory>

class ClientModel;
class TransactionFilterProxy;
class TxViewDelegate;
class PlatformStyle;
class RewardsChart;
class WalletModel;

namespace Ui {
    class OverviewPage;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
class QTimer;
QT_END_NAMESPACE

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);

    //! Host the application's wallet selector bar at the top of this page.
    void setWalletBar(QWidget* bar);

public Q_SLOTS:
    void setBalance(const interfaces::WalletBalances& balances);

Q_SIGNALS:
    void transactionClicked(const QModelIndex &index);
    void outOfSyncWarningClicked();

private:
    void updateTransactionsPlaceholder();
    void buildRecoveryRow();

private Q_SLOTS:
    /** Full rescan with a widening address window - see recoverwallet. */
    void recoverBalance();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QLabel* m_empty_transactions = nullptr;
    QLabel* m_wallet_name_label = nullptr;
    bool m_wallet_bar_shown = false;
    QFrame* m_balance_badge = nullptr;
    QLabel* m_badge_caption = nullptr;
    QLabel* m_badge_value = nullptr;

    //! Bottom of the Balances card: the way out when the balance looks wrong.
    QWidget* m_recovery_row = nullptr;
    QPushButton* m_recovery_button = nullptr;
    QLabel* m_recovery_hint = nullptr;

    //! Right column: how this wallet is earning, and whether it can start again.
    QFrame* m_staking_badge = nullptr;
    QLabel* m_staking_badge_title = nullptr;
    QLabel* m_staking_badge_value = nullptr;
    QLabel* m_staking_badge_hint = nullptr;
    QFrame* m_ready_badge = nullptr;
    QLabel* m_ready_badge_title = nullptr;
    QLabel* m_ready_badge_value = nullptr;
    QFrame* m_rewards_card = nullptr;
    QLabel* m_rewards_total = nullptr;
    RewardsChart* m_rewards_chart = nullptr;
    //! Coalesces model churn - a rescan inserts rows by the thousand.
    QTimer* m_rewards_timer = nullptr;
    RewardSummary m_rewards;

    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    interfaces::WalletBalances m_balances;

    TxViewDelegate *txdelegate;
    std::unique_ptr<TransactionFilterProxy> filter;

    QTimer *stakingTickTimer;
    int64_t stakingDurationSeconds;

    QString formatStakingRemaining(int64_t seconds) const;
    void buildRewardsColumn();
    void applyBadgeState(QWidget* badge, const char* state);

private Q_SLOTS:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void updateAlerts(const QString &warnings);
    void updateWatchOnlyLabels(bool showWatchOnly);
    void handleOutOfSyncWarningClicks();
    void onStartStakingClicked();
    void onStopStakingClicked();
    void updateStakingUi();
    void tickStakingTimer();
    //! Recompute the reward badges and chart from the transaction model.
    void refreshRewards();
    //! Ask for a recompute once the model has stopped changing.
    void scheduleRewardsRefresh();
};

#endif // BITCOIN_QT_OVERVIEWPAGE_H
