// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/mnemonicdialog.h>

#include <interfaces/node.h>
#include <qt/asyncrpc.h>
#include <random.h>
#include <univalue.h>
#include <wallet/bip39.h>

#include <QApplication>
#include <QCheckBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

namespace {

enum PageIndex {
    PAGE_INTRO = 0,
    PAGE_WORDS,
    PAGE_VERIFY,
    PAGE_RESTORE,
    PAGE_PASSPHRASE,
    PAGE_SHOW,
    PAGE_RESULT,
};

const int VERIFY_WORD_COUNT = 3;
const int PHRASE_WORDS = 24;

QStringList WordlistModel()
{
    QStringList list;
    list.reserve(static_cast<int>(bip39::EnglishWordlistSize()));
    for (size_t i = 0; i < bip39::EnglishWordlistSize(); ++i) {
        list << QString::fromLatin1(bip39::EnglishWordlist()[i]);
    }
    return list;
}

//! 24 numbered cells, so the order is unambiguous when transcribing onto paper.
QWidget* BuildWordGrid(QWidget* parent, std::vector<QLabel*>& cells)
{
    QFrame* frame = new QFrame(parent);
    frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    QGridLayout* grid = new QGridLayout(frame);
    grid->setSpacing(6);

    QFont mono = frame->font();
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(mono.pointSize() + 1);

    cells.clear();
    for (int i = 0; i < PHRASE_WORDS; ++i) {
        QLabel* cell = new QLabel(frame);
        cell->setFont(mono);
        cell->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cell->setMinimumWidth(130);
        cells.push_back(cell);
        grid->addWidget(cell, i % 8, i / 8);
    }
    return frame;
}

void FillWordGrid(const std::vector<QLabel*>& cells, const QStringList& words)
{
    for (size_t i = 0; i < cells.size(); ++i) {
        const QString word = static_cast<int>(i) < words.size() ? words.at(static_cast<int>(i)) : QString();
        cells[i]->setText(QString("%1. %2").arg(i + 1, 2).arg(word));
    }
}

QLabel* Explain(const QString& text)
{
    QLabel* label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}

//! split() + drop empties, without QString::SkipEmptyParts (deprecated in Qt 5.14)
//! or Qt::SkipEmptyParts (absent before it).
QStringList SplitWords(const QString& text)
{
    QStringList out;
    for (const QString& part : text.split(QLatin1Char(' '))) {
        if (!part.isEmpty()) out << part;
    }
    return out;
}

} // namespace

MnemonicDialog::MnemonicDialog(Mode mode, interfaces::Node& node, QWidget* parent, const QString& wallet_name)
    : QDialog(parent), m_mode(mode), m_node(node), m_wallet_name(wallet_name)
{
    setWindowTitle(mode == Mode::Create ? tr("Create wallet") :
                   mode == Mode::Restore ? tr("Restore wallet") : tr("Recovery phrase"));
    setMinimumWidth(560);

    m_pages = new QStackedWidget(this);
    buildIntroPage();
    buildWordsPage();
    buildVerifyPage();
    buildRestorePage();
    buildPassphrasePage();
    buildShowPage();
    buildResultPage();

    m_busy = new QProgressBar(this);
    m_busy->setRange(0, 0);
    m_busy->setVisible(false);

    m_back = new QPushButton(tr("Back"), this);
    m_next = new QPushButton(tr("Next"), this);
    m_cancel = new QPushButton(tr("Cancel"), this);
    m_next->setDefault(true);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(m_busy, 1);
    buttons->addStretch(1);
    buttons->addWidget(m_back);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_next);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_pages, 1);
    layout->addLayout(buttons);

    connect(m_next, &QPushButton::clicked, this, &MnemonicDialog::onNext);
    connect(m_back, &QPushButton::clicked, this, &MnemonicDialog::onBack);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);

    switch (m_mode) {
    case Mode::Create:
        goToPage(PAGE_INTRO);
        break;
    case Mode::Restore:
        goToPage(PAGE_RESTORE);
        break;
    case Mode::Show:
        if (!loadWalletPhrase()) {
            // loadWalletPhrase() has already explained why.
            QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
            break;
        }
        goToPage(PAGE_SHOW);
        break;
    }
}

MnemonicDialog::~MnemonicDialog()
{
    // The phrase must not outlive the dialog.
    m_mnemonic.clear();
    m_words.clear();
}

// --- pages -----------------------------------------------------------------

void MnemonicDialog::buildIntroPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* l = new QVBoxLayout(page);
    l->addWidget(Explain(tr("<b>Your wallet is backed up by 24 words.</b>")));
    l->addWidget(Explain(tr(
        "In the next step you will see 24 words. Written down, they restore this wallet "
        "on any Taler wallet, on any computer, even if this one is lost.")));
    l->addWidget(Explain(tr(
        "Anyone who sees those words owns your coins. Write them on paper. Do not store "
        "them in a file, a screenshot or a password manager.")));
    l->addStretch(1);
    m_pages->insertWidget(PAGE_INTRO, page);
}

void MnemonicDialog::buildWordsPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* l = new QVBoxLayout(page);
    l->addWidget(Explain(tr("Write these 24 words down, in this order, and keep them safe.")));

    l->addWidget(BuildWordGrid(page, m_word_labels));

    l->addWidget(Explain(tr(
        "<i>A screenshot or a photo of this screen is not a safe backup - anything that "
        "reads your screen or your files can read the words too.</i>")));
    l->addStretch(1);
    m_pages->insertWidget(PAGE_WORDS, page);
}

void MnemonicDialog::buildVerifyPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* l = new QVBoxLayout(page);
    m_verify_prompt = Explain(tr("Confirm you wrote the words down."));
    l->addWidget(m_verify_prompt);

    QFormLayout* form = new QFormLayout();
    for (int i = 0; i < VERIFY_WORD_COUNT; ++i) {
        QLineEdit* edit = new QLineEdit(page);
        connect(edit, &QLineEdit::textChanged, this, &MnemonicDialog::onVerifyChanged);
        m_verify_inputs.push_back(edit);
        form->addRow(new QLabel(QString(), page), edit);
    }
    l->addLayout(form);
    l->addStretch(1);
    m_pages->insertWidget(PAGE_VERIFY, page);
}

void MnemonicDialog::buildRestorePage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* l = new QVBoxLayout(page);
    l->addWidget(Explain(tr("Enter the 24 words of the wallet you want to restore.")));

    QCompleter* completer = new QCompleter(WordlistModel(), page);
    completer->setCaseSensitivity(Qt::CaseInsensitive);

    QGridLayout* grid = new QGridLayout();
    for (int i = 0; i < PHRASE_WORDS; ++i) {
        QLineEdit* edit = new QLineEdit(page);
        edit->setPlaceholderText(QString::number(i + 1));
        edit->setCompleter(completer);
        connect(edit, &QLineEdit::textChanged, this, &MnemonicDialog::onRestoreWordsChanged);
        m_restore_inputs.push_back(edit);
        grid->addWidget(edit, i / 4, i % 4);
    }
    l->addLayout(grid);

    m_restore_status = new QLabel(page);
    m_restore_status->setWordWrap(true);
    l->addWidget(m_restore_status);

    QFormLayout* form = new QFormLayout();
    m_wallet_name_edit = new QLineEdit(page);
    m_wallet_name_edit->setPlaceholderText(tr("restored"));
    connect(m_wallet_name_edit, &QLineEdit::textChanged, this, &MnemonicDialog::onRestoreWordsChanged);
    form->addRow(tr("Name for the restored wallet"), m_wallet_name_edit);
    QLabel* restoreNameHint = Explain(tr("Latin letters, digits and hyphens only. No spaces, and it cannot start or end with a hyphen \u2014 the name becomes a folder on disk."));
    restoreNameHint->setStyleSheet("color: palette(mid);");
    form->addRow(QString(), restoreNameHint);

    m_use_birthday = new QCheckBox(tr("I know roughly when this wallet was created"), page);
    m_birthday = new QDateEdit(page);
    m_birthday->setCalendarPopup(true);
    m_birthday->setDate(QDate(2017, 9, 13)); // the genesis date
    m_birthday->setEnabled(false);
    connect(m_use_birthday, &QCheckBox::toggled, m_birthday, &QWidget::setEnabled);
    form->addRow(m_use_birthday, m_birthday);
    l->addLayout(form);

    l->addWidget(Explain(tr(
        "<i>A date only makes the scan faster. If you are not sure, leave it unticked and "
        "the whole chain is scanned - slower, but it cannot miss anything.</i>")));
    l->addStretch(1);
    m_pages->insertWidget(PAGE_RESTORE, page);
}

void MnemonicDialog::buildPassphrasePage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* l = new QVBoxLayout(page);
    l->addWidget(Explain(tr("<b>Set a passphrase for this wallet.</b>")));
    l->addWidget(Explain(tr(
        "The passphrase protects this wallet file on this computer. It is not the same "
        "thing as your 24 words: the words recover your coins anywhere, even if you "
        "forget the passphrase or lose this computer.")));

    QFormLayout* form = new QFormLayout();
    m_passphrase1 = new QLineEdit(page);
    m_passphrase1->setEchoMode(QLineEdit::Password);
    m_passphrase2 = new QLineEdit(page);
    m_passphrase2->setEchoMode(QLineEdit::Password);
    connect(m_passphrase1, &QLineEdit::textChanged, this, &MnemonicDialog::onPassphraseChanged);
    connect(m_passphrase2, &QLineEdit::textChanged, this, &MnemonicDialog::onPassphraseChanged);
    form->addRow(tr("Passphrase"), m_passphrase1);
    form->addRow(tr("Repeat passphrase"), m_passphrase2);
    if (m_mode == Mode::Create) {
        m_wallet_name_edit = new QLineEdit(page);
        m_wallet_name_edit->setPlaceholderText(tr("wallet"));
        connect(m_wallet_name_edit, &QLineEdit::textChanged, this, &MnemonicDialog::onPassphraseChanged);
        form->addRow(tr("Wallet name"), m_wallet_name_edit);
        QLabel* nameHint = Explain(tr("Latin letters, digits and hyphens only. No spaces, and it cannot start or end with a hyphen \u2014 the name becomes a folder on disk."));
        nameHint->setStyleSheet("color: palette(mid);");
        form->addRow(QString(), nameHint);
    }
    l->addLayout(form);

    m_passphrase_status = new QLabel(page);
    m_passphrase_status->setWordWrap(true);
    l->addWidget(m_passphrase_status);

    m_no_passphrase = new QCheckBox(tr("Continue without a passphrase"), page);
    connect(m_no_passphrase, &QCheckBox::toggled, this, [this](bool checked) {
        m_passphrase1->setEnabled(!checked);
        m_passphrase2->setEnabled(!checked);
        onPassphraseChanged();
    });
    l->addWidget(m_no_passphrase);
    l->addWidget(Explain(tr(
        "<i>Without a passphrase the wallet stakes continuously with no unlocking. With "
        "one, you must unlock the wallet for it to stake.</i>")));
    l->addStretch(1);
    m_pages->insertWidget(PAGE_PASSPHRASE, page);
}

void MnemonicDialog::buildShowPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* l = new QVBoxLayout(page);
    l->addWidget(Explain(tr("These 24 words restore this wallet. Anyone who sees them owns it.")));
    l->addWidget(BuildWordGrid(page, m_show_labels));
    l->addStretch(1);
    m_pages->insertWidget(PAGE_SHOW, page);
}

void MnemonicDialog::buildResultPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* l = new QVBoxLayout(page);
    m_result = new QLabel(page);
    m_result->setWordWrap(true);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(m_result);
    l->addStretch(1);
    m_pages->insertWidget(PAGE_RESULT, page);
}

// --- flow ------------------------------------------------------------------

void MnemonicDialog::goToPage(int index)
{
    m_pages->setCurrentIndex(index);

    if (index == PAGE_WORDS && m_mnemonic.empty() && !generatePhrase()) return;

    if (index == PAGE_VERIFY) {
        // Ask for words at random positions, so writing down "the first three" does not pass.
        m_verify_positions.clear();
        while (m_verify_positions.size() < VERIFY_WORD_COUNT) {
            const int pos = static_cast<int>(GetRand(PHRASE_WORDS));
            if (std::find(m_verify_positions.begin(), m_verify_positions.end(), pos) == m_verify_positions.end()) {
                m_verify_positions.push_back(pos);
            }
        }
        std::sort(m_verify_positions.begin(), m_verify_positions.end());
        for (size_t i = 0; i < m_verify_inputs.size(); ++i) {
            m_verify_inputs[i]->clear();
            QWidget* row_label = m_pages->widget(PAGE_VERIFY)->layout()->itemAt(1)->layout()->itemAt(static_cast<int>(i) * 2)->widget();
            if (QLabel* label = qobject_cast<QLabel*>(row_label)) {
                label->setText(tr("Word %1").arg(m_verify_positions[i] + 1));
            }
        }
        m_verify_prompt->setText(tr("Confirm you wrote the words down: type words %1, %2 and %3.")
                                     .arg(m_verify_positions[0] + 1)
                                     .arg(m_verify_positions[1] + 1)
                                     .arg(m_verify_positions[2] + 1));
    }

    updateButtons();
}

void MnemonicDialog::updateButtons()
{
    const int page = m_pages->currentIndex();
    m_back->setVisible(m_mode == Mode::Create && (page == PAGE_WORDS || page == PAGE_VERIFY || page == PAGE_PASSPHRASE));
    m_cancel->setVisible(page != PAGE_RESULT && m_mode != Mode::Show);

    switch (page) {
    case PAGE_INTRO:
        m_next->setText(tr("Show my 24 words"));
        m_next->setEnabled(true);
        break;
    case PAGE_WORDS:
        m_next->setText(tr("I have written them down"));
        m_next->setEnabled(true);
        break;
    case PAGE_VERIFY:
        m_next->setText(tr("Continue"));
        onVerifyChanged();
        break;
    case PAGE_RESTORE:
        m_next->setText(tr("Continue"));
        onRestoreWordsChanged();
        break;
    case PAGE_PASSPHRASE:
        m_next->setText(m_mode == Mode::Restore ? tr("Restore wallet") : tr("Create wallet"));
        onPassphraseChanged();
        break;
    case PAGE_SHOW:
    case PAGE_RESULT:
        m_next->setText(tr("Close"));
        m_next->setEnabled(true);
        break;
    }
}

void MnemonicDialog::onNext()
{
    switch (m_pages->currentIndex()) {
    case PAGE_INTRO:    goToPage(PAGE_WORDS); break;
    case PAGE_WORDS:    goToPage(PAGE_VERIFY); break;
    case PAGE_VERIFY:   goToPage(PAGE_PASSPHRASE); break;
    case PAGE_RESTORE:  goToPage(PAGE_PASSPHRASE); break;
    case PAGE_PASSPHRASE:
        if (m_mode == Mode::Restore) doRestore(); else doCreate();
        break;
    case PAGE_SHOW:
    case PAGE_RESULT:
        accept();
        break;
    }
}

void MnemonicDialog::onBack()
{
    switch (m_pages->currentIndex()) {
    case PAGE_WORDS:      goToPage(PAGE_INTRO); break;
    case PAGE_VERIFY:     goToPage(PAGE_WORDS); break;
    case PAGE_PASSPHRASE: goToPage(m_mode == Mode::Restore ? PAGE_RESTORE : PAGE_VERIFY); break;
    default: break;
    }
}

void MnemonicDialog::onVerifyChanged()
{
    bool ok = m_verify_positions.size() == m_verify_inputs.size();
    for (size_t i = 0; ok && i < m_verify_inputs.size(); ++i) {
        ok = m_verify_inputs[i]->text().trimmed().toLower() == m_words.value(m_verify_positions[i]);
    }
    m_next->setEnabled(ok);
}

void MnemonicDialog::onRestoreWordsChanged()
{
    QStringList words;
    for (QLineEdit* edit : m_restore_inputs) {
        const QString word = edit->text().trimmed().toLower();
        if (!word.isEmpty()) words << word;
    }

    if (words.size() < PHRASE_WORDS) {
        m_restore_status->setText(tr("%1 of %2 words entered.").arg(words.size()).arg(PHRASE_WORDS));
        m_next->setEnabled(false);
        return;
    }

    const SecureString phrase(words.join(QLatin1Char(' ')).toStdString().c_str());
    if (!bip39::MnemonicIsValidForWallet(phrase)) {
        m_restore_status->setText(tr("<b>These words are not a valid recovery phrase.</b> Check the "
                                     "spelling and the order - one wrong word is enough to fail this check."));
        m_next->setEnabled(false);
        return;
    }

    m_restore_status->setText(tr("Recovery phrase is valid."));
    m_next->setEnabled(true);
}

void MnemonicDialog::onPassphraseChanged()
{
    if (m_no_passphrase->isChecked()) {
        m_passphrase_status->setText(tr("This wallet will not be encrypted."));
        m_next->setEnabled(true);
        return;
    }
    const QString a = m_passphrase1->text();
    const QString b = m_passphrase2->text();
    if (a.isEmpty()) {
        m_passphrase_status->setText(QString());
        m_next->setEnabled(false);
    } else if (a != b) {
        m_passphrase_status->setText(tr("The two passphrases do not match."));
        m_next->setEnabled(false);
    } else {
        m_passphrase_status->setText(QString());
        m_next->setEnabled(true);
    }
}

// --- work ------------------------------------------------------------------

bool MnemonicDialog::generatePhrase()
{
    QString result, error;
    if (!callRpc(QLatin1String("getnewmnemonic"), UniValue(UniValue::VARR), result, error)) {
        fail(tr("Could not generate a recovery phrase: %1").arg(error));
        return false;
    }
    m_mnemonic = result.toStdString().c_str();
    showWords(m_mnemonic);
    return true;
}

void MnemonicDialog::showWords(const SecureString& mnemonic)
{
    m_words = SplitWords(QString::fromStdString(std::string(mnemonic.c_str())));
    FillWordGrid(m_word_labels, m_words);
}

bool MnemonicDialog::loadWalletPhrase()
{
    QString result, error;
    if (!callRpc(QLatin1String("getwalletmnemonic"), UniValue(UniValue::VARR), result, error)) {
        QMessageBox::warning(this, tr("Recovery phrase"), error);
        return false;
    }
    m_mnemonic = result.toStdString().c_str();
    const QStringList words = SplitWords(result);
    FillWordGrid(m_show_labels, words);
    return true;
}

void MnemonicDialog::doCreate()
{
    const QString name = m_wallet_name_edit->text().trimmed().isEmpty() ? QLatin1String("wallet")
                                                                       : m_wallet_name_edit->text().trimmed();
    const QString passphrase = m_no_passphrase->isChecked() ? QString() : m_passphrase1->text();

    m_busy->setVisible(true);
    m_next->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Explicit types. Note UniValue has no push_back(bool) overload, so a bare
    // `false` would bind to push_back(int) and arrive as the number 0.
    UniValue params(UniValue::VARR);
    params.push_back(name.toStdString());
    params.push_back(UniValue(false)); // disable_private_keys
    params.push_back(std::string(m_mnemonic.c_str()));
    params.push_back(passphrase.isEmpty() ? UniValue(UniValue::VNULL) : UniValue(passphrase.toStdString()));

    QString result, error;
    const bool ok = callRpc(QLatin1String("createwallet"), params, result, error);

    QApplication::restoreOverrideCursor();
    m_busy->setVisible(false);

    if (!ok) {
        fail(tr("The wallet could not be created: %1").arg(error));
        return;
    }

    m_created_wallet = name;
    m_mnemonic.clear();
    m_words.clear();
    m_result->setText(tr("<b>Wallet \"%1\" is ready.</b><br><br>Your 24 words are its backup. "
                         "You can see them again from the wallet menu, as long as the wallet is unlocked.")
                          .arg(name));
    goToPage(PAGE_RESULT);
}

void MnemonicDialog::doRestore()
{
    QStringList words;
    for (QLineEdit* edit : m_restore_inputs) words << edit->text().trimmed().toLower();

    const QString name = m_wallet_name_edit->text().trimmed().isEmpty() ? QLatin1String("restored")
                                                                       : m_wallet_name_edit->text().trimmed();
    const QString passphrase = m_no_passphrase->isChecked() ? QString() : m_passphrase1->text();
    // QDateTime(QDate) is deprecated in Qt 5.15 and QDate::startOfDay() does not exist
    // before 5.14, so build the moment explicitly - it works on both.
    const QDateTime birthday_moment(m_birthday->date(), QTime(0, 0));
    const qint64 birthday = m_use_birthday->isChecked() ? birthday_moment.toMSecsSinceEpoch() / 1000 : 0;

    m_busy->setVisible(true);
    m_next->setEnabled(false);
    m_back->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    UniValue params(UniValue::VARR);
    params.push_back(name.toStdString());
    params.push_back(words.join(QLatin1Char(' ')).toStdString());
    params.push_back(UniValue(static_cast<int64_t>(birthday)));
    params.push_back(UniValue(UniValue::VNULL)); // gap_limit: use the node default
    params.push_back(passphrase.isEmpty() ? UniValue(UniValue::VNULL) : UniValue(passphrase.toStdString()));

    QString result, error;
    const bool ok = callRpc(QLatin1String("restorewallet"), params, result, error);

    QApplication::restoreOverrideCursor();
    m_busy->setVisible(false);
    m_back->setEnabled(true);

    if (!ok) {
        fail(tr("The wallet could not be restored: %1").arg(error));
        return;
    }

    m_created_wallet = name;
    m_result->setText(tr("<b>Wallet \"%1\" was restored.</b><br><br>%2").arg(name, result));
    goToPage(PAGE_RESULT);
}

void MnemonicDialog::fail(const QString& message)
{
    QMessageBox::warning(this, windowTitle(), message);
    m_next->setEnabled(true);
    updateButtons();
}

bool MnemonicDialog::callRpc(const QString& method, const UniValue& params, QString& result, QString& error)
{
    // Off the GUI thread: creating a wallet can take a moment and restoring one
    // rescans the chain, which would otherwise freeze the window outright.
    UniValue reply;
    const QString busy = (method == QLatin1String("restorewallet"))
                             ? tr("Restoring the wallet and scanning the chain. This can take a while.")
                             : tr("Working...");
    if (!RunNodeRpc(m_node, method, params, std::string(), reply, error, this, busy)) return false;
    result = QString::fromStdString(reply.isStr() ? reply.get_str() : reply.write(2));
    return true;
}
