// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/earnpage.h>

#include <chainparams.h>
#include <qt/askpassphrasedialog.h>
#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>
#include <utiltime.h>
#include <wallet/wallet.h>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// Every so many ticks the stakeable amounts are recounted: that walks the wallet's
// coins under cs_main, which is too much to do every second.
const int ELIGIBILITY_EVERY_TICKS = 30;

QString displayName(const std::string& wallet)
{
    return GUIUtil::walletDisplayName(QString::fromStdString(wallet));
}

QString formatRemaining(int64_t seconds)
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

QString formatAge(int64_t seconds)
{
    if (seconds % 86400 == 0) return QObject::tr("%n day(s)", "", int(seconds / 86400));
    if (seconds % 3600 == 0) return QObject::tr("%n hour(s)", "", int(seconds / 3600));
    return QObject::tr("%n minute(s)", "", int((seconds + 59) / 60));
}

QFrame* makeCard(QWidget* parent, const QString& title, const QString& hint, QVBoxLayout*& layout)
{
    QFrame* card = new QFrame(parent);
    card->setFrameShape(QFrame::StyledPanel);
    layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);
    QLabel* title_label = new QLabel(title, card);
    title_label->setProperty("class", "cardTitle");
    layout->addWidget(title_label);
    QLabel* hint_label = new QLabel(hint, card);
    hint_label->setProperty("class", "cardHint");
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);
    return card;
}

QPushButton* makeGoToButton(QWidget* parent)
{
    QPushButton* button = new QPushButton(parent);
    button->setFlat(true);
    button->setCursor(Qt::PointingHandCursor);
    button->hide();
    return button;
}

void addDuration(QComboBox* combo, const QString& text, qlonglong seconds)
{
    combo->addItem(text, seconds);
}

} // namespace

EarnPage::EarnPage(QWidget* parent) :
    QWidget(parent)
{
    QVBoxLayout* page = new QVBoxLayout(this);

    QVBoxLayout* heading = new QVBoxLayout();
    heading->setSpacing(2);
    QLabel* title = new QLabel(tr("Earn"), this);
    title->setProperty("class", "pageTitle");
    heading->addWidget(title);
    QLabel* subtitle = new QLabel(
        tr("Stake or mine with one wallet at a time. Switching wallets does not stop either."), this);
    subtitle->setProperty("class", "pageSubtitle");
    subtitle->setWordWrap(true);
    heading->addWidget(subtitle);
    page->addLayout(heading);

    QHBoxLayout* columns = new QHBoxLayout();
    columns->setSpacing(12);

    // ---- staking ----------------------------------------------------------
    QVBoxLayout* stake = nullptr;
    QFrame* stake_card = makeCard(this, tr("Staking"),
        tr("Earn proof-of-stake rewards with this wallet's coins. The wallet stays unlocked "
           "for the time you choose, then locks again."), stake);

    m_stake_status = new QLabel(stake_card);
    m_stake_status->setWordWrap(true);
    stake->addWidget(m_stake_status);
    m_stake_amounts = new QLabel(stake_card);
    m_stake_amounts->setProperty("class", "cardHint");
    m_stake_amounts->setWordWrap(true);
    stake->addWidget(m_stake_amounts);
    m_stake_reason = new QLabel(stake_card);
    m_stake_reason->setWordWrap(true);
    stake->addWidget(m_stake_reason);
    m_stake_go_to = makeGoToButton(stake_card);
    stake->addWidget(m_stake_go_to, 0, Qt::AlignLeft);

    QHBoxLayout* stake_controls = new QHBoxLayout();
    stake_controls->addWidget(new QLabel(tr("Duration:"), stake_card));
    m_stake_duration = new QComboBox(stake_card);
    addDuration(m_stake_duration, tr("1 hour"), 3600);
    addDuration(m_stake_duration, tr("6 hours"), 6 * 3600);
    addDuration(m_stake_duration, tr("24 hours"), 24 * 3600);
    addDuration(m_stake_duration, tr("7 days"), 7 * 24 * 3600);
    addDuration(m_stake_duration, tr("30 days"), 30 * 24 * 3600);
    m_stake_duration->setCurrentIndex(2);
    stake_controls->addWidget(m_stake_duration);
    stake_controls->addStretch(1);
    m_stake_start = new QPushButton(tr("Start staking"), stake_card);
    m_stake_start->setObjectName("okButton");
    m_stake_stop = new QPushButton(tr("Stop staking"), stake_card);
    m_stake_stop->setObjectName("dangerButton");
    stake_controls->addWidget(m_stake_start);
    stake_controls->addWidget(m_stake_stop);
    stake->addLayout(stake_controls);
    stake->addStretch(1);
    columns->addWidget(stake_card, 1);

    // ---- mining -----------------------------------------------------------
    QVBoxLayout* mine = nullptr;
    QFrame* mine_card = makeCard(this, tr("Mining"),
        tr("Earn proof-of-work rewards with this computer's processor. Rewards go to a new "
           "address of this wallet, and the wallet stays locked. Mining keeps the chosen "
           "number of cores busy, which makes the computer run hot and loud."), mine);

    m_mine_status = new QLabel(mine_card);
    m_mine_status->setWordWrap(true);
    mine->addWidget(m_mine_status);
    m_mine_reason = new QLabel(mine_card);
    m_mine_reason->setWordWrap(true);
    mine->addWidget(m_mine_reason);
    m_mine_go_to = makeGoToButton(mine_card);
    mine->addWidget(m_mine_go_to, 0, Qt::AlignLeft);

    QHBoxLayout* mine_settings = new QHBoxLayout();
    mine_settings->addWidget(new QLabel(tr("Threads:"), mine_card));
    m_mine_threads = new QSpinBox(mine_card);
    m_mine_threads->setRange(1, 1);
    m_mine_threads->setValue(1);
    mine_settings->addWidget(m_mine_threads);
    mine_settings->addSpacing(12);
    mine_settings->addWidget(new QLabel(tr("Duration:"), mine_card));
    m_mine_duration = new QComboBox(mine_card);
    addDuration(m_mine_duration, tr("1 hour"), 3600);
    addDuration(m_mine_duration, tr("6 hours"), 6 * 3600);
    addDuration(m_mine_duration, tr("24 hours"), 24 * 3600);
    addDuration(m_mine_duration, tr("7 days"), 7 * 24 * 3600);
    addDuration(m_mine_duration, tr("30 days"), 30 * 24 * 3600);
    addDuration(m_mine_duration, tr("Until stopped"), 0);
    m_mine_duration->setCurrentIndex(0);
    mine_settings->addWidget(m_mine_duration);
    mine_settings->addStretch(1);
    mine->addLayout(mine_settings);

    QHBoxLayout* mine_controls = new QHBoxLayout();
    mine_controls->addStretch(1);
    m_mine_start = new QPushButton(tr("Start mining"), mine_card);
    m_mine_start->setObjectName("okButton");
    m_mine_stop = new QPushButton(tr("Stop mining"), mine_card);
    m_mine_stop->setObjectName("dangerButton");
    mine_controls->addWidget(m_mine_start);
    mine_controls->addWidget(m_mine_stop);
    mine->addLayout(mine_controls);
    mine->addStretch(1);
    columns->addWidget(mine_card, 1);

    page->addLayout(columns);
    page->addStretch(1);

    connect(m_stake_start, &QPushButton::clicked, this, &EarnPage::onStartStaking);
    connect(m_mine_start, &QPushButton::clicked, this, &EarnPage::onStartMining);
    connect(m_stake_stop, &QPushButton::clicked, this, &EarnPage::onStop);
    connect(m_mine_stop, &QPushButton::clicked, this, &EarnPage::onStop);
    connect(m_stake_go_to, &QPushButton::clicked, this, [this] { Q_EMIT switchWalletRequested(m_stake_go_to_wallet); });
    connect(m_mine_go_to, &QPushButton::clicked, this, [this] { Q_EMIT switchWalletRequested(m_mine_go_to_wallet); });

    m_tick = new QTimer(this);
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, &EarnPage::refresh);
}

void EarnPage::setWalletModel(WalletModel* model)
{
    m_model = model;
    if (!model) return;
    connect(model, &WalletModel::encryptionStatusChanged, this, &EarnPage::refresh);
    connect(model, &WalletModel::balanceChanged, this, &EarnPage::refreshEligibility);
    const int max_threads = model->node().getGeneration().max_threads;
    m_mine_threads->setRange(1, std::max(1, max_threads));
    refreshEligibility();
}

void EarnPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshEligibility();
    m_tick->start();
}

void EarnPage::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_tick->stop();
}

bool EarnPage::isSyncing() const
{
    return m_model && m_model->node().isInitialBlockDownload();
}

bool EarnPage::isEncrypted() const
{
    return m_model && m_model->getEncryptionStatus() != WalletModel::Unencrypted;
}

EarnPage::Blocker EarnPage::stakingBlocker(bool syncing, bool encrypted, const interfaces::NodeGeneration& generation,
                                           const std::string& wallet, const interfaces::WalletStakeEligibility& eligibility,
                                           const QString& min_age, const QString& min_amount)
{
    using Mode = interfaces::NodeGeneration::Mode;
    Blocker blocker;
    if (syncing) {
        blocker.reason = tr("Waiting for the node to finish syncing.");
    } else if (!encrypted) {
        blocker.reason = tr("Encrypt this wallet to stake. Staking unlocks it for a set time, which needs a passphrase.");
    } else if (generation.mode == Mode::MINING) {
        blocker.reason = generation.wallet == wallet
            ? tr("This wallet is mining. Stop mining to stake.")
            : tr("Wallet \"%1\" is mining. Stop mining there to stake.").arg(displayName(generation.wallet));
        blocker.has_go_to = generation.wallet != wallet;
    } else if (generation.mode == Mode::STAKING && generation.wallet != wallet) {
        blocker.reason = tr("Wallet \"%1\" is staking. Only one wallet can stake at a time.").arg(displayName(generation.wallet));
        blocker.has_go_to = true;
    } else if (eligibility.stakeable == 0 && eligibility.maturing == 0) {
        blocker.reason = tr("No coins to stake. A stake starts from a coin of at least %1.").arg(min_amount);
    } else if (eligibility.stakeable == 0) {
        blocker.reason = tr("Coins become stakeable on %1. A coin must be %2 old to stake.")
                             .arg(GUIUtil::dateTimeStr(eligibility.next_stakeable_time), min_age);
    }
    if (blocker.has_go_to) blocker.go_to_wallet = QString::fromStdString(generation.wallet);
    return blocker;
}

EarnPage::Blocker EarnPage::miningBlocker(bool syncing, bool encrypted, const interfaces::NodeGeneration& generation,
                                          const std::string& wallet)
{
    using Mode = interfaces::NodeGeneration::Mode;
    Blocker blocker;
    if (syncing) {
        blocker.reason = tr("Waiting for the node to finish syncing.");
    } else if (!encrypted) {
        blocker.reason = tr("Encrypt this wallet to mine. Starting mining asks for its passphrase.");
    } else if (generation.mode == Mode::STAKING) {
        blocker.reason = generation.wallet == wallet
            ? tr("This wallet is staking. Stop staking to mine.")
            : tr("Wallet \"%1\" is staking. Stop staking there to mine.").arg(displayName(generation.wallet));
        blocker.has_go_to = generation.wallet != wallet;
    } else if (generation.mode == Mode::MINING && generation.wallet != wallet) {
        blocker.reason = tr("Wallet \"%1\" is mining. Only one wallet can mine at a time.").arg(displayName(generation.wallet));
        blocker.has_go_to = true;
    }
    if (blocker.has_go_to) blocker.go_to_wallet = QString::fromStdString(generation.wallet);
    return blocker;
}

QString EarnPage::statusText(const interfaces::NodeGeneration& generation, bool mining) const
{
    const int64_t remaining = generation.until > 0 ? generation.until - GetTime() : 0;
    QString text;
    if (mining) {
        text = generation.until > 0
            ? tr("Mining with %n thread(s), %1 left.", "", generation.threads).arg(formatRemaining(remaining))
            : tr("Mining with %n thread(s) until stopped.", "", generation.threads);
    } else {
        text = generation.until > 0
            ? tr("Staking, %1 left.").arg(formatRemaining(remaining))
            : tr("Staking until stopped.");
        // A session ends when its wallet locks; a configured one waits for an unlock.
        if (generation.until == 0 && m_model && m_model->getEncryptionStatus() == WalletModel::Locked) {
            text += " " + tr("Waiting: the wallet is locked. Unlock it to stake.");
        }
    }
    if (generation.from_config) {
        text += " " + (mining ? tr("Started by gen=1 in taler.conf.") : tr("Started by stakegen=1 in taler.conf."));
    }
    return text;
}

void EarnPage::refreshEligibility()
{
    if (!m_model) return;
    m_eligibility = m_model->wallet().getStakeEligibility();
    m_ticks = 0;
    refresh();
}

void EarnPage::refresh()
{
    if (!m_model) return;
    if (++m_ticks >= ELIGIBILITY_EVERY_TICKS) {
        refreshEligibility(); // calls back into refresh
        return;
    }

    using Mode = interfaces::NodeGeneration::Mode;
    const interfaces::NodeGeneration generation = m_model->node().getGeneration();
    const std::string wallet = m_model->getWalletName().toStdString();
    const bool syncing = isSyncing();
    const bool encrypted = isEncrypted();
    const int unit = m_model->getOptionsModel() ? m_model->getOptionsModel()->getDisplayUnit() : BitcoinUnits::BTC;

    // ---- staking ----
    const bool staking_here = generation.mode == Mode::STAKING && generation.wallet == wallet;
    const Blocker stake_blocker = stakingBlocker(syncing, encrypted, generation, wallet, m_eligibility,
        formatAge(Params().GetConsensus().nStakeMinAge),
        BitcoinUnits::formatWithUnit(unit, MIN_STAKE_KERNEL_AMOUNT, false, BitcoinUnits::separatorAlways));
    m_stake_status->setText(staking_here ? statusText(generation, false) : tr("Not staking."));

    QStringList amounts;
    if (m_eligibility.stakeable > 0) {
        amounts << tr("Stakeable now: %1.").arg(BitcoinUnits::formatWithUnit(unit, m_eligibility.stakeable, false, BitcoinUnits::separatorAlways));
    }
    if (m_eligibility.maturing > 0 && m_eligibility.stakeable > 0) {
        amounts << tr("A further %1 becomes stakeable from %2.")
                       .arg(BitcoinUnits::formatWithUnit(unit, m_eligibility.maturing, false, BitcoinUnits::separatorAlways),
                            GUIUtil::dateTimeStr(m_eligibility.next_stakeable_time));
    }
    m_stake_amounts->setText(amounts.join(" "));
    m_stake_amounts->setVisible(!amounts.isEmpty());

    m_stake_reason->setText(stake_blocker.reason);
    m_stake_reason->setVisible(!staking_here && !stake_blocker.reason.isEmpty());
    m_stake_go_to_wallet = stake_blocker.go_to_wallet;
    m_stake_go_to->setText(tr("Go to \"%1\"").arg(GUIUtil::walletDisplayName(stake_blocker.go_to_wallet)));
    m_stake_go_to->setVisible(!staking_here && stake_blocker.has_go_to);
    m_stake_start->setVisible(!staking_here);
    m_stake_start->setEnabled(stake_blocker.reason.isEmpty());
    m_stake_duration->setEnabled(!staking_here && stake_blocker.reason.isEmpty());
    m_stake_stop->setVisible(staking_here);

    // ---- mining ----
    const bool mining_here = generation.mode == Mode::MINING && generation.wallet == wallet;
    const Blocker mine_blocker = miningBlocker(syncing, encrypted, generation, wallet);
    m_mine_status->setText(mining_here ? statusText(generation, true) : tr("Not mining."));
    m_mine_reason->setText(mine_blocker.reason);
    m_mine_reason->setVisible(!mining_here && !mine_blocker.reason.isEmpty());
    m_mine_go_to_wallet = mine_blocker.go_to_wallet;
    m_mine_go_to->setText(tr("Go to \"%1\"").arg(GUIUtil::walletDisplayName(mine_blocker.go_to_wallet)));
    m_mine_go_to->setVisible(!mining_here && mine_blocker.has_go_to);
    m_mine_start->setVisible(!mining_here);
    m_mine_start->setEnabled(mine_blocker.reason.isEmpty());
    m_mine_threads->setEnabled(!mining_here && mine_blocker.reason.isEmpty());
    m_mine_duration->setEnabled(!mining_here && mine_blocker.reason.isEmpty());
    m_mine_stop->setVisible(mining_here);
}

void EarnPage::onStartStaking()
{
    if (!m_model) return;
    const int64_t seconds = m_stake_duration->currentData().toLongLong();
    AskPassphraseDialog dlg(AskPassphraseDialog::UnlockStaking, this);
    dlg.setModel(m_model);
    dlg.setStakingDuration(seconds);
    dlg.exec();
    refresh();
}

void EarnPage::onStartMining()
{
    if (!m_model) return;
    AskPassphraseDialog dlg(AskPassphraseDialog::StartMining, this);
    dlg.setModel(m_model);
    dlg.setMiningParameters(m_mine_threads->value(), m_mine_duration->currentData().toLongLong());
    dlg.exec();
    refresh();
}

void EarnPage::onStop()
{
    if (!m_model) return;
    const bool mining = m_model->isMining();
    if (QMessageBox::question(this, mining ? tr("Stop mining?") : tr("Stop staking?"),
            mining ? tr("Are you sure you want to stop mining?") : tr("Are you sure you want to stop staking?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_model->stopGeneration(error)) {
        QMessageBox::warning(this, mining ? tr("Stop mining") : tr("Stop staking"), error);
    }
    refresh();
}
