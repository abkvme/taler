// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ASKPASSPHRASEDIALOG_H
#define BITCOIN_QT_ASKPASSPHRASEDIALOG_H

#include <cstdint>

#include <QDialog>

class WalletModel;

namespace Ui {
    class AskPassphraseDialog;
}

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class AskPassphraseDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        Encrypt,        /**< Ask passphrase twice and encrypt */
        Unlock,         /**< Ask passphrase and unlock */
        ChangePass,     /**< Ask old passphrase + new passphrase twice */
        Decrypt,        /**< Ask passphrase and decrypt wallet */
        UnlockStaking,  /**< Ask passphrase, unlock for a timed staking session */
        StartMining     /**< Ask passphrase to authorise mining; the wallet stays locked */
    };

    explicit AskPassphraseDialog(Mode mode, QWidget *parent);
    ~AskPassphraseDialog();

    void accept();

    void setModel(WalletModel *model);

    /** For mode=UnlockStaking, set the staking duration in seconds. */
    void setStakingDuration(int64_t seconds) { stakingDurationSeconds = seconds; }
    /** For mode=StartMining: threads, and seconds to mine (0 = until stopped). */
    void setMiningParameters(int threads, int64_t seconds) { miningThreads = threads; miningSeconds = seconds; }

private:
    Ui::AskPassphraseDialog *ui;
    Mode mode;
    WalletModel *model;
    bool fCapsLock;
    int64_t stakingDurationSeconds = 0;
    int miningThreads = 0;
    int64_t miningSeconds = 0;

private Q_SLOTS:
    void textChanged();
    void secureClearPassFields();
    void toggleShowPassword(bool);

protected:
    bool event(QEvent *event);
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // BITCOIN_QT_ASKPASSPHRASEDIALOG_H
