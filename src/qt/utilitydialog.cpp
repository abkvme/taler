// Copyright (c) 2011-2018 The Bitcoin Core developers
// Copyright (c) 2019-2021 Uladzimir (https://t.me/vovanchik_net)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/utilitydialog.h>

#include <qt/brandbanner.h>

#include <QTabWidget>

#include <qt/forms/ui_helpmessagedialog.h>

#include <qt/bitcoingui.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/intro.h>
#include <qt/guiutil.h>

#include <clientversion.h>
#include <init.h>
#include <interfaces/node.h>
#include <util.h>

#include <stdio.h>

#include <QCloseEvent>
#include <QLabel>
#include <QRegExp>
#include <QTextTable>
#include <QTextCursor>
#include <QVBoxLayout>

/** "Help message" or "About" dialog box */
HelpMessageDialog::HelpMessageDialog(interfaces::Node& node, QWidget *parent, bool about) :
    QDialog(parent),
    ui(new Ui::HelpMessageDialog)
{
    ui->setupUi(this);

    QString version = tr(PACKAGE_NAME) + " " + tr("version") + " " + QString::fromStdString(FormatFullVersion());
    /* On x86 add a bit specifier to the version so that users can distinguish between
     * 32 and 64 bit builds. On other architectures, 32/64 bit may be more ambiguous.
     */
#if defined(__x86_64__)
    version += " " + tr("(%1-bit)").arg(64);
#elif defined(__i386__ )
    version += " " + tr("(%1-bit)").arg(32);
#endif

    if (about)
    {
        setWindowTitle(tr("About %1").arg(tr(PACKAGE_NAME)));

        // Two tabs rather than a picture with a scrolling box wedged under it.
        // They answer different questions - "what is this" and "what are the
        // terms" - and cramming both into one view served neither: the artwork
        // was squashed and the licence got a letterbox to be read through.
        ui->aboutLogo->setVisible(false);
        ui->frame->setVisible(false);

        QTabWidget *tabs = new QTabWidget(this);

        BrandBanner *banner = new BrandBanner(tabs);
        tabs->addTab(banner, tr("About"));

        // Reparents the scroll area out of the dialog and into the tab.
        tabs->addTab(ui->scrollArea, tr("License"));

        ui->verticalLayout->insertWidget(0, tabs);
        // Tall enough for the artwork at its natural size, so the first tab never
        // opens already needing a scrollbar.
        resize(brand::ARTWORK_WIDTH + 32,
               banner->heightForWidth(brand::ARTWORK_WIDTH) + 120);

        /// HTML-format the license message from the core
        QString licenseInfo = QString::fromStdString(LicenseInfo());
        QString licenseInfoHTML = licenseInfo;
        // Make URLs clickable
        QRegExp uri("<(.*)>", Qt::CaseSensitive, QRegExp::RegExp2);
        uri.setMinimal(true); // use non-greedy matching
        licenseInfoHTML.replace(uri, "<a href=\"\\1\">\\1</a>");
        // Replace newlines with HTML breaks
        licenseInfoHTML.replace("\n", "<br>");

        ui->aboutMessage->setTextFormat(Qt::RichText);
        ui->aboutMessage->setTextInteractionFlags(Qt::TextBrowserInteraction);
        ui->aboutMessage->setOpenExternalLinks(true);
        ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        // The licence tab gets its own padding; it was inheriting the dialog's.
        ui->scrollAreaWidgetContents->layout()->setContentsMargins(12, 12, 12, 12);
        text = version + "\n" + licenseInfo;
        ui->aboutMessage->setText(licenseInfoHTML);
        ui->aboutMessage->setWordWrap(true);
        ui->helpMessage->setVisible(false);
    } else {
        setWindowTitle(tr("Command-line options"));
        QString header = "Usage:  taler-qt [command-line options]                     \n";
        QTextCursor cursor(ui->helpMessage->document());
        cursor.insertText(version);
        cursor.insertBlock();
        cursor.insertText(header);
        cursor.insertBlock();

        std::string strUsage = gArgs.GetHelpMessage();
        QString coreOptions = QString::fromStdString(strUsage);
        text = version + "\n\n" + header + "\n" + coreOptions;

        QTextTableFormat tf;
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_None);
        tf.setCellPadding(2);
        QVector<QTextLength> widths;
        widths << QTextLength(QTextLength::PercentageLength, 35);
        widths << QTextLength(QTextLength::PercentageLength, 65);
        tf.setColumnWidthConstraints(widths);

        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);

        for (const QString &line : coreOptions.split("\n")) {
            if (line.startsWith("  -"))
            {
                cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::PreviousCell);
                cursor.movePosition(QTextCursor::NextRow);
                cursor.insertText(line.trimmed());
                cursor.movePosition(QTextCursor::NextCell);
            } else if (line.startsWith("   ")) {
                cursor.insertText(line.trimmed()+' ');
            } else if (line.size() > 0) {
                //Title of a group
                if (cursor.currentTable())
                    cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::Down);
                cursor.insertText(line.trimmed(), bold);
                cursor.insertTable(1, 2, tf);
            }
        }

        ui->helpMessage->moveCursor(QTextCursor::Start);
        ui->scrollArea->setVisible(false);
        ui->aboutLogo->setVisible(false);
    }
}

HelpMessageDialog::~HelpMessageDialog()
{
    delete ui;
}

void HelpMessageDialog::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    fprintf(stdout, "%s\n", qPrintable(text));
}

void HelpMessageDialog::showOrPrint()
{
#if defined(WIN32)
    // On Windows, show a message box, as there is no stderr/stdout in windowed applications
    exec();
#else
    // On other operating systems, print help text to console
    printToConsole();
#endif
}

void HelpMessageDialog::on_okButton_accepted()
{
    close();
}


/** "Shutdown" window */
ShutdownWindow::ShutdownWindow(QWidget *parent, Qt::WindowFlags f):
    QWidget(parent, f)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // The application opened on this picture and it closes on it. Links are off:
    // a click here would open a browser behind a window that is disappearing.
    BrandBanner *banner = new BrandBanner(this);
    banner->setLinksEnabled(false);
    banner->setFixedSize(brand::ARTWORK_WIDTH, banner->heightForWidth(brand::ARTWORK_WIDTH));
    layout->addWidget(banner);

    // Same strip as the splash, in the artwork's own edge colour, so shutting
    // down looks like the other end of starting up rather than a stray dialog.
    const QPixmap artwork = brand::Artwork();
    const QColor edge = brand::ArtworkEdge(artwork);
    const bool dark_artwork = brand::ArtworkIsDark(artwork);

    QLabel *message = new QLabel(
        tr("%1 is shutting down...").arg(tr(PACKAGE_NAME)) + QString("<br/>") +
        tr("Do not shut down the computer until this window disappears."), this);
    message->setAlignment(Qt::AlignCenter);
    message->setWordWrap(true);
    message->setStyleSheet(QString("background-color: %1; color: %2; padding: 10px;")
                               .arg(edge.name(),
                                    dark_artwork ? QString("#c6d2e4") : QString("#373737")));
    layout->addWidget(message);
}

QWidget *ShutdownWindow::showShutdownWindow(BitcoinGUI *window)
{
    if (!window)
        return nullptr;

    // Show a simple window indicating shutdown status
    QWidget *shutdownWindow = new ShutdownWindow();
    shutdownWindow->setWindowTitle(window->windowTitle());

    // Center shutdown window at where main window was
    const QPoint global = window->mapToGlobal(window->rect().center());
    shutdownWindow->move(global.x() - shutdownWindow->width() / 2, global.y() - shutdownWindow->height() / 2);
    shutdownWindow->show();
    return shutdownWindow;
}

void ShutdownWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
}
