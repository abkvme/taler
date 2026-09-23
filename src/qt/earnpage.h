// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_EARNPAGE_H
#define BITCOIN_QT_EARNPAGE_H

#include <interfaces/node.h>
#include <interfaces/wallet.h>

#include <QWidget>

class WalletModel;

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;
QT_END_NAMESPACE

/**
 * Staking and mining for one wallet: staking on the left, mining on the right.
 *
 * The node stakes or mines with one wallet at a time, so this page is mostly about
 * saying why a button cannot be pressed: another wallet is already at it, the other
 * activity is running, the wallet is not encrypted, or its coins are too small or
 * too young to stake. Everything shown comes from the node, never from the page's
 * own idea of what it started.
 */
class EarnPage : public QWidget
{
    Q_OBJECT

public:
    explicit EarnPage(QWidget* parent = nullptr);

    void setWalletModel(WalletModel* model);

    //! Why staking cannot start, most important first. Empty when it can.
    struct Blocker
    {
        QString reason;
        QString go_to_wallet; //!< the wallet holding the node's attention, when it is another one
        bool has_go_to = false;
    };
    static Blocker stakingBlocker(bool syncing, bool encrypted, const interfaces::NodeGeneration& generation,
                                  const std::string& wallet, const interfaces::WalletStakeEligibility& eligibility,
                                  const QString& min_age, const QString& min_amount);
    static Blocker miningBlocker(bool syncing, bool encrypted, const interfaces::NodeGeneration& generation,
                                 const std::string& wallet);

Q_SIGNALS:
    //! The user asked to see the wallet that is staking or mining.
    void switchWalletRequested(const QString& name);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private Q_SLOTS:
    void refresh();
    void refreshEligibility();
    void onStartStaking();
    void onStartMining();
    void onStop();

private:
    WalletModel* m_model = nullptr;
    QTimer* m_tick = nullptr;
    int m_ticks = 0;
    interfaces::WalletStakeEligibility m_eligibility;

    QLabel* m_stake_status = nullptr;
    QLabel* m_stake_amounts = nullptr;
    QLabel* m_stake_reason = nullptr;
    QPushButton* m_stake_go_to = nullptr;
    QComboBox* m_stake_duration = nullptr;
    QPushButton* m_stake_start = nullptr;
    QPushButton* m_stake_stop = nullptr;
    QString m_stake_go_to_wallet;

    QLabel* m_mine_status = nullptr;
    QLabel* m_mine_reason = nullptr;
    QPushButton* m_mine_go_to = nullptr;
    QSpinBox* m_mine_threads = nullptr;
    QComboBox* m_mine_duration = nullptr;
    QPushButton* m_mine_start = nullptr;
    QPushButton* m_mine_stop = nullptr;
    QString m_mine_go_to_wallet;

    bool isSyncing() const;
    bool isEncrypted() const;
    QString statusText(const interfaces::NodeGeneration& generation, bool mining) const;
};

#endif // BITCOIN_QT_EARNPAGE_H
