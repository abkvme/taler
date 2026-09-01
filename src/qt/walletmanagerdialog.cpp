// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/walletmanagerdialog.h>

#include <interfaces/node.h>
#include <qt/asyncrpc.h>
#include <qt/guiutil.h>
#include <qt/mnemonicdialog.h>

#include <QApplication>
#include <QDateTime>
#include <QFont>
#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QUrl>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

enum Column { COL_NAME = 0, COL_DEFAULT, COL_TYPE, COL_BALANCE, COL_PATH, COLUMN_COUNT };

//! A wallet name becomes a file name, so keep it to something that cannot escape the
//! wallet directory or collide with Berkeley DB's own files.
bool ValidWalletName(const QString& name, QString& why)
{
    if (name.isEmpty()) {
        why = QObject::tr("The name cannot be empty.");
        return false;
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) ||
        name == QLatin1String(".") || name == QLatin1String("..")) {
        why = QObject::tr("The name cannot contain a path.");
        return false;
    }
    if (name == QLatin1String("removed") || name == QLatin1String("backups") ||
        name == QLatin1String("database") || name.startsWith(QLatin1String("__db.")) ||
        name.startsWith(QLatin1String("log."))) {
        why = QObject::tr("That name is reserved by the wallet directory.");
        return false;
    }
    return true;
}

//! Update the GUI's own memory of which wallets exist, so a renamed or removed wallet
//! is not reloaded on the next start.
void ForgetWallet(const QString& name)
{
    QSettings settings;
    QStringList remembered = settings.value("RememberedWallets").toStringList();
    remembered.removeAll(name);
    settings.setValue("RememberedWallets", remembered);
    // A removed or renamed wallet must not stay the start-up default, or the app
    // would open on a wallet that no longer exists.
    if (GUIUtil::hasDefaultWallet() && GUIUtil::defaultWallet() == name) {
        GUIUtil::clearDefaultWallet();
    }
}

void RememberWallet(const QString& name)
{
    QSettings settings;
    QStringList remembered = settings.value("RememberedWallets").toStringList();
    if (!remembered.contains(name)) {
        remembered << name;
        settings.setValue("RememberedWallets", remembered);
    }
}

} // namespace

WalletManagerDialog::WalletManagerDialog(interfaces::Node& node, QWidget* parent, const QString& current_wallet)
    : QDialog(parent), m_node(node), m_current_wallet(current_wallet)
{
    setWindowTitle(tr("Manage wallets"));
    resize(820, 380);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(COLUMN_COUNT);
    m_table->setHorizontalHeaderLabels({tr("Wallet"), tr("Default"), tr("Type"), tr("Balance"), tr("File on disk")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->horizontalHeader()->setHighlightSections(false);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &WalletManagerDialog::onSelectionChanged);

    m_create = new QPushButton(tr("Create wallet..."), this);
    m_restore = new QPushButton(tr("Restore wallet..."), this);
    m_rename = new QPushButton(tr("Rename..."), this);
    m_make_default = new QPushButton(tr("Open this one at start"), this);
    m_remove = new QPushButton(tr("Remove..."), this);
    QPushButton* close = new QPushButton(tr("Close"), this);
    close->setDefault(true);

    connect(m_create, &QPushButton::clicked, this, &WalletManagerDialog::onCreate);
    connect(m_restore, &QPushButton::clicked, this, &WalletManagerDialog::onRestore);
    connect(m_rename, &QPushButton::clicked, this, &WalletManagerDialog::onRename);
    connect(m_make_default, &QPushButton::clicked, this, &WalletManagerDialog::onMakeDefault);
    connect(m_remove, &QPushButton::clicked, this, &WalletManagerDialog::onRemove);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(m_create);
    buttons->addWidget(m_restore);
    buttons->addSpacing(16);
    buttons->addWidget(m_make_default);
    buttons->addWidget(m_rename);
    buttons->addWidget(m_remove);
    buttons->addStretch(1);
    buttons->addWidget(close);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_table, 1);
    QLabel* note = new QLabel(tr("Removing a wallet moves its file to the \"removed\" folder inside "
                                 "the wallet directory. Nothing is deleted, so a mistake can be undone "
                                 "by moving the folder back."), this);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addLayout(buttons);

    refresh();
}

// --- data ------------------------------------------------------------------

bool WalletManagerDialog::rpc(const QString& method, const UniValue& params, UniValue& result, QString& error)
{
    return RunNodeRpc(m_node, method, params, std::string(), result, error, this, tr("Working..."));
}

bool WalletManagerDialog::rpcForWallet(const QString& wallet, const QString& method, const UniValue& params,
                                       UniValue& result, QString& error)
{
    const QByteArray encoded = QUrl::toPercentEncoding(wallet);
    const std::string uri = "/wallet/" + std::string(encoded.constData(), encoded.length());
    return RunNodeRpc(m_node, method, params, uri, result, error, this, tr("Working..."));
}

std::vector<WalletManagerDialog::WalletRow> WalletManagerDialog::collect()
{
    std::vector<WalletRow> rows;
    UniValue dir;
    QString error;
    if (!rpc(QLatin1String("listwalletdir"), UniValue(UniValue::VARR), dir, error)) {
        QMessageBox::warning(this, windowTitle(), tr("Could not list the wallet directory: %1").arg(error));
        return rows;
    }
    m_wallet_dir = QString::fromStdString(find_value(dir, "walletdir").get_str());

    const UniValue& wallets = find_value(dir, "wallets");
    for (size_t i = 0; i < wallets.size(); ++i) {
        const UniValue& w = wallets[i];
        WalletRow row;
        row.name = QString::fromStdString(find_value(w, "name").get_str());
        const UniValue& filename = find_value(w, "filename");
        row.filename = filename.isStr() ? QString::fromStdString(filename.get_str()) : row.name;
        row.path = QString::fromStdString(find_value(w, "path").get_str());
        const UniValue& file = find_value(w, "file");
        row.file = file.isStr() ? QString::fromStdString(file.get_str()) : row.path;
        row.loaded = find_value(w, "loaded").get_bool();
        row.is_default = GUIUtil::hasDefaultWallet() && GUIUtil::defaultWallet() == row.name;

        if (row.loaded) {
            UniValue info;
            QString err;
            if (rpcForWallet(row.name, QLatin1String("getwalletinfo"), UniValue(UniValue::VARR), info, err)) {
                const UniValue& scheme = find_value(info, "hdscheme");
                row.scheme = scheme.isStr() ? QString::fromStdString(scheme.get_str()) : QString();
                const UniValue& balance = find_value(info, "balance");
                if (balance.isNum()) row.balance = QString::number(balance.get_real(), 'f', 8);
            }
        }
        rows.push_back(row);
    }
    return rows;
}

void WalletManagerDialog::refresh()
{
    m_rows = collect();
    m_table->setRowCount(static_cast<int>(m_rows.size()));

    QFont bold = m_table->font();
    bold.setBold(true);
    QFont mono = m_table->font();
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(std::max(8, mono.pointSize() - 1));

    for (size_t i = 0; i < m_rows.size(); ++i) {
        const WalletRow& row = m_rows[i];
        const int r = static_cast<int>(i);
        m_table->setRowHeight(r, 30);

        // Name: the wallet as the user knows it, with the active one called out.
        const QString display = GUIUtil::walletDisplayName(row.name);
        QTableWidgetItem* name = new QTableWidgetItem(display);
        name->setFont(bold);
        if (row.loaded && row.name == m_current_wallet) {
            name->setText(tr("%1  •  in use").arg(display));
        }
        name->setToolTip(tr("File name: %1").arg(row.filename));
        m_table->setItem(r, COL_NAME, name);

        // Default: a green dot on the wallet the app opens with.
        QTableWidgetItem* def = new QTableWidgetItem(row.is_default ? QString(QChar(0x25CF)) : QString());
        def->setTextAlignment(Qt::AlignCenter);
        if (row.is_default) {
            def->setForeground(QBrush(QColor(0x3f, 0xb9, 0x50)));
            def->setToolTip(tr("This wallet opens when the application starts."));
        }
        m_table->setItem(r, COL_DEFAULT, def);

        // Type: what kind of wallet it is, which is not the same as whether it is open.
        QString type;
        if (row.scheme == QLatin1String("bip44")) {
            type = tr("Recovery phrase");
        } else if (row.scheme == QLatin1String("legacy")) {
            type = tr("Standard (file backup)");
        } else {
            type = tr("Not open");
        }
        QTableWidgetItem* type_item = new QTableWidgetItem(type);
        type_item->setToolTip(row.loaded
            ? tr("A recovery-phrase wallet can be restored from its 24 words. A standard "
                 "wallet can only be restored from a copy of its file.")
            : tr("This wallet is not open, so its type and balance are unknown. Open it "
                 "from the wallet selector to see them."));
        m_table->setItem(r, COL_TYPE, type_item);

        QTableWidgetItem* balance = new QTableWidgetItem(row.loaded ? row.balance + QLatin1String(" TLR")
                                                                   : QString(QChar(0x2014)));  // em dash; a UTF-8 literal in QLatin1String renders as mojibake
        balance->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, COL_BALANCE, balance);

        QTableWidgetItem* path = new QTableWidgetItem(row.file);
        path->setFont(mono);
        path->setToolTip(row.file);
        m_table->setItem(r, COL_PATH, path);
    }

    m_table->resizeColumnsToContents();
    onSelectionChanged();
}

const WalletManagerDialog::WalletRow* WalletManagerDialog::selectedRow() const
{
    const int r = m_table->currentRow();
    if (r < 0 || static_cast<size_t>(r) >= m_rows.size()) return nullptr;
    return &m_rows[static_cast<size_t>(r)];
}

void WalletManagerDialog::onSelectionChanged()
{
    const WalletRow* row = selectedRow();
    // The default (unnamed) wallet is what -wallet resolves to, so renaming it would
    // leave talerd creating a fresh empty wallet on its next start.
    const bool renameable = row && GUIUtil::isRenamableWallet(row->name);
    m_rename->setEnabled(renameable);
    m_rename->setToolTip(renameable ? QString()
                                    : tr("The legacy wallet cannot be renamed: its name is what the node "
                                         "resolves by default, and changing it would leave talerd creating "
                                         "an empty wallet in its place."));
    m_remove->setEnabled(row != nullptr);
    m_make_default->setEnabled(row != nullptr && !row->is_default);
}

// --- actions ---------------------------------------------------------------

void WalletManagerDialog::onCreate()
{
    MnemonicDialog dlg(MnemonicDialog::Mode::Create, m_node, this);
    if (dlg.exec() == QDialog::Accepted && !dlg.createdWallet().isEmpty()) {
        m_activate = dlg.createdWallet();
        RememberWallet(m_activate);
        refresh();
    }
}

void WalletManagerDialog::onRestore()
{
    MnemonicDialog dlg(MnemonicDialog::Mode::Restore, m_node, this);
    if (dlg.exec() == QDialog::Accepted && !dlg.createdWallet().isEmpty()) {
        m_activate = dlg.createdWallet();
        RememberWallet(m_activate);
        refresh();
    }
}

bool WalletManagerDialog::unloadIfLoaded(const WalletRow& row, QString& error)
{
    if (!row.loaded) return true;
    UniValue params(UniValue::VARR);
    params.push_back(row.name.toStdString());
    UniValue result;
    // Must not run on the GUI thread: the node waits here for the GUI to release its
    // own reference to the wallet, which only happens as events are processed.
    return RunNodeRpc(m_node, QLatin1String("unloadwallet"), params, std::string(), result, error,
                      this, tr("Closing the wallet..."));
}

bool WalletManagerDialog::copyWallet(const QString& from, const QString& to, QString& error)
{
    QFileInfo info(from);
    if (info.isDir()) {
        QDir().mkpath(to);
        for (const QFileInfo& entry : QDir(from).entryInfoList(QDir::Files)) {
            if (!QFile::copy(entry.absoluteFilePath(), QDir(to).filePath(entry.fileName()))) {
                error = QObject::tr("Could not copy %1").arg(entry.fileName());
                return false;
            }
        }
        return true;
    }
    if (!QFile::copy(from, to)) {
        error = QObject::tr("Could not copy the wallet file to %1").arg(to);
        return false;
    }
    return true;
}

bool WalletManagerDialog::retire(const QString& path, const QString& name, QString& error)
{
    const QString stamp = QDateTime::currentDateTime().toString(QLatin1String("yyyyMMdd-HHmmss"));
    const QString removed_dir = QDir(m_wallet_dir).filePath(QLatin1String("removed"));
    if (!QDir().mkpath(removed_dir)) {
        error = tr("Could not create %1").arg(removed_dir);
        return false;
    }
    const QString target = QDir(removed_dir).filePath(QString("%1-%2").arg(name.isEmpty() ? QLatin1String("wallet") : name, stamp));
    if (!QDir().rename(path, target)) {
        error = tr("Could not move the wallet to %1").arg(target);
        return false;
    }
    return true;
}

void WalletManagerDialog::onRename()
{
    const WalletRow* selected = selectedRow();
    if (!selected) return;
    const WalletRow row = *selected; // refresh() invalidates the pointer

    bool ok = false;
    const QString new_name = QInputDialog::getText(this, tr("Rename wallet"),
                                                   tr("New name for \"%1\":").arg(row.name),
                                                   QLineEdit::Normal, row.name, &ok).trimmed();
    if (!ok || new_name == row.name) return;

    QString why;
    if (!ValidWalletName(new_name, why)) {
        QMessageBox::warning(this, tr("Rename wallet"), why);
        return;
    }
    const QString new_path = QDir(m_wallet_dir).filePath(new_name);
    if (QFileInfo::exists(new_path)) {
        QMessageBox::warning(this, tr("Rename wallet"), tr("A wallet named \"%1\" already exists.").arg(new_name));
        return;
    }

    QString error;
    const bool was_loaded = row.loaded;

    // Unload, copy to the new name, load the copy, and only then retire the original.
    bool success = unloadIfLoaded(row, error);
    if (success) success = copyWallet(row.path, new_path, error);
    if (success) {
        UniValue params(UniValue::VARR);
        params.push_back(new_name.toStdString());
        UniValue result;
        success = rpc(QLatin1String("loadwallet"), params, result, error);
        if (!success) {
            // The copy did not open: leave the original untouched and clean up.
            QFileInfo info(new_path);
            if (info.isDir()) QDir(new_path).removeRecursively(); else QFile::remove(new_path);
            if (was_loaded) {
                UniValue reload(UniValue::VARR);
                reload.push_back(row.name.toStdString());
                UniValue ignored;
                QString ignored_error;
                rpc(QLatin1String("loadwallet"), reload, ignored, ignored_error);
            }
        }
    }
    if (success) success = retire(row.path, row.name, error);


    if (!success) {
        QMessageBox::warning(this, tr("Rename wallet"),
                             tr("The wallet was not renamed: %1\n\nThe original wallet is untouched.").arg(error));
        refresh();
        return;
    }

    ForgetWallet(row.name);
    RememberWallet(new_name);
    m_activate = new_name;
    QMessageBox::information(this, tr("Rename wallet"),
                             tr("Renamed to \"%1\".\n\nThe previous file was moved to the \"removed\" folder "
                                "rather than deleted.").arg(new_name));
    refresh();
}

void WalletManagerDialog::onMakeDefault()
{
    const WalletRow* selected = selectedRow();
    if (!selected) return;
    GUIUtil::setDefaultWallet(selected->name);
    refresh();
}

void WalletManagerDialog::onRemove()
{
    const WalletRow* selected = selectedRow();
    if (!selected) return;
    const WalletRow row = *selected;
    const QString display = GUIUtil::walletDisplayName(row.name);

    // What the user is about to lose access to, stated before anything happens.
    QString detail = tr("<b>Remove wallet \"%1\"?</b><br><br>File: %2<br><br>").arg(display, row.file);
    if (row.loaded) {
        UniValue info;
        QString err;
        if (rpcForWallet(row.name, QLatin1String("getwalletinfo"), UniValue(UniValue::VARR), info, err)) {
            const UniValue& scheme = find_value(info, "hdscheme");
            if (scheme.isStr() && scheme.get_str() == "legacy") {
                detail += tr("This is a legacy wallet: it has <b>no recovery phrase</b>, so this file is the "
                             "only copy of its keys.<br><br>");
            }
            const UniValue& balance = find_value(info, "balance");
            if (balance.isNum() && balance.get_real() > 0) {
                detail += tr("<b>This wallet still holds %1 TLR.</b><br><br>").arg(balance.get_real(), 0, 'f', 8);
            }
        }
    }
    detail += tr("The wallet will be moved to the \"removed\" folder in the wallet directory. "
                 "Nothing is deleted, and you can bring it back by moving it out again.");

    QMessageBox confirm(this);
    confirm.setWindowTitle(tr("Remove wallet"));
    confirm.setTextFormat(Qt::RichText);
    confirm.setText(detail);
    confirm.setIcon(QMessageBox::Warning);
    confirm.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    confirm.setDefaultButton(QMessageBox::Cancel);
    if (confirm.exec() != QMessageBox::Ok) return;

    bool ok = false;
    const QString typed = QInputDialog::getText(this, tr("Remove wallet"),
                                                tr("Type the wallet name to confirm: %1").arg(display),
                                                QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok) return;
    if (typed != GUIUtil::walletDisplayName(row.name)) {
        QMessageBox::warning(this, tr("Remove wallet"), tr("The name did not match. Nothing was removed."));
        return;
    }

    QString error;
    bool success = unloadIfLoaded(row, error);
    if (success) success = retire(row.path, row.name, error);

    if (!success) {
        QMessageBox::warning(this, tr("Remove wallet"), tr("The wallet was not removed: %1").arg(error));
        refresh();
        return;
    }

    ForgetWallet(row.name);
    QMessageBox::information(this, tr("Remove wallet"),
                             tr("\"%1\" was moved to the \"removed\" folder in the wallet directory.").arg(display));
    refresh();
}
