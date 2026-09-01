// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MNEMONICDIALOG_H
#define BITCOIN_QT_MNEMONICDIALOG_H

#include <support/allocators/secure.h>

#include <univalue.h>

#include <QDialog>
#include <QString>
#include <QStringList>

#include <vector>

class QCheckBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;

namespace interfaces {
class Node;
}

/**
 * Recovery-phrase dialogs: create a wallet, restore one, or show the phrase of the
 * wallet already open.
 *
 * Creation runs in a fixed order and nothing touches disk until the last step:
 *
 *     explain -> show the 24 words -> verify a few of them -> passphrase -> create
 *
 * Cancelling at any point before the final step leaves nothing behind, and the
 * words live only in memory (SecureString) until then. The wallet is created and
 * encrypted in a single RPC call, so a phrase-based wallet is never written to disk
 * unencrypted - which matters because the node auto-backs-up wallet files on every
 * startup.
 */
class MnemonicDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Create,  //!< generate a phrase and create a wallet from it
        Restore, //!< take an existing phrase and rescan for its history
        Show,    //!< reveal the phrase of the current wallet
    };

    explicit MnemonicDialog(Mode mode, interfaces::Node& node, QWidget* parent = nullptr,
                            const QString& wallet_name = QString());
    ~MnemonicDialog();

    //! Name of the wallet that was created or restored, empty if none was.
    QString createdWallet() const { return m_created_wallet; }

private Q_SLOTS:
    void onNext();
    void onBack();
    void onVerifyChanged();
    void onRestoreWordsChanged();
    void onPassphraseChanged();

private:
    void buildIntroPage();
    void buildWordsPage();
    void buildVerifyPage();
    void buildPassphrasePage();
    void buildRestorePage();
    void buildShowPage();
    void buildResultPage();

    void goToPage(int index);
    void updateButtons();
    void showWords(const SecureString& mnemonic);
    bool generatePhrase();
    bool loadWalletPhrase();
    void doCreate();
    void doRestore();
    void fail(const QString& message);

    /**
     * Run an RPC through the node interface, returning false and filling @p error on
     * failure. Callers build @p params with explicit JSON types: guessing a type from
     * the text of an argument silently sends a wallet named "2024" as a number.
     */
    bool callRpc(const QString& method, const UniValue& params, QString& result, QString& error);

    Mode m_mode;
    interfaces::Node& m_node;
    QString m_wallet_name;
    QString m_created_wallet;

    SecureString m_mnemonic;      //!< in memory only until the wallet is created
    QStringList m_words;          //!< display copy, cleared with the dialog
    std::vector<int> m_verify_positions;

    QStackedWidget* m_pages = nullptr;
    std::vector<QLabel*> m_word_labels;  //!< creation page, 24 numbered cells
    std::vector<QLabel*> m_show_labels;  //!< "show my phrase" page
    QLabel* m_verify_prompt = nullptr;
    std::vector<QLineEdit*> m_verify_inputs;
    std::vector<QLineEdit*> m_restore_inputs;
    QLabel* m_restore_status = nullptr;
    QDateEdit* m_birthday = nullptr;
    QCheckBox* m_use_birthday = nullptr;
    QLineEdit* m_wallet_name_edit = nullptr;
    QLineEdit* m_passphrase1 = nullptr;
    QLineEdit* m_passphrase2 = nullptr;
    QLabel* m_passphrase_status = nullptr;
    QCheckBox* m_no_passphrase = nullptr;
    QLabel* m_result = nullptr;
    QProgressBar* m_busy = nullptr;

    QPushButton* m_back = nullptr;
    QPushButton* m_next = nullptr;
    QPushButton* m_cancel = nullptr;
};

#endif // BITCOIN_QT_MNEMONICDIALOG_H
