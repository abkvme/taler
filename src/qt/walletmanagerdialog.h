// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLETMANAGERDIALOG_H
#define BITCOIN_QT_WALLETMANAGERDIALOG_H

#include <univalue.h>

#include <QDialog>
#include <QString>

#include <vector>

class QPushButton;
class QTableWidget;

namespace interfaces {
class Node;
}

/**
 * Manage wallets: see every wallet in the wallet directory with its type, balance and
 * absolute path on disk, and create, restore, rename or remove one.
 *
 * Two safety properties are deliberate:
 *
 *  - Nothing here is ever deleted. Removing a wallet moves it to a "removed" folder
 *    inside the wallet directory, so a mistake is recoverable by moving it back. That
 *    matters most for a legacy wallet, whose file is the only copy of its keys.
 *  - Renaming copies to the new name, loads and verifies it, and only then retires the
 *    old file the same way. Berkeley DB keeps shared transaction logs in the wallet
 *    directory, so copy-then-retire is safer than renaming a file in place.
 *
 * All file handling happens here rather than behind an RPC, so the node exposes no
 * destructive wallet operation to remote callers.
 */
class WalletManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WalletManagerDialog(interfaces::Node& node, QWidget* parent = nullptr,
                                 const QString& current_wallet = QString());

    //! Wallet the caller should switch to, if the dialog created or restored one.
    QString walletToActivate() const { return m_activate; }

private Q_SLOTS:
    void refresh();
    void onCreate();
    void onRestore();
    void onRename();
    void onRemove();
    void onMakeDefault();
    void onSelectionChanged();

private:
    struct WalletRow {
        QString name;      //!< name the node loads it under ("" for the default wallet)
        QString filename;  //!< file name in the wallet directory
        QString path;      //!< wallet location: the directory for a phrase wallet
        QString file;      //!< the wallet.dat itself, which is what we show the user
        bool loaded = false;
        bool is_default = false;  //!< opens at start-up
        QString scheme;     //!< "bip44", "legacy", or empty when not loaded
        QString balance;    //!< formatted, empty when not loaded
    };

    std::vector<WalletRow> collect();
    const WalletRow* selectedRow() const;

    bool rpc(const QString& method, const UniValue& params, UniValue& result, QString& error);
    //! Same, but addressed at one wallet via the /wallet/<name> endpoint. Without this
    //! a per-wallet call such as getwalletinfo answers for the default wallet instead.
    bool rpcForWallet(const QString& wallet, const QString& method, const UniValue& params,
                      UniValue& result, QString& error);
    bool unloadIfLoaded(const WalletRow& row, QString& error);
    //! Move a wallet file or directory into <walletdir>/removed/<name>-<timestamp>/
    bool retire(const QString& path, const QString& name, QString& error);
    static bool copyWallet(const QString& from, const QString& to, QString& error);

    interfaces::Node& m_node;
    QString m_current_wallet;
    QString m_activate;
    QString m_wallet_dir;
    std::vector<WalletRow> m_rows;

    QTableWidget* m_table = nullptr;
    QPushButton* m_create = nullptr;
    QPushButton* m_restore = nullptr;
    QPushButton* m_rename = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_make_default = nullptr;
};

#endif // BITCOIN_QT_WALLETMANAGERDIALOG_H
