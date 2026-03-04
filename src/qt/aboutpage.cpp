// Copyright (c) 2024 The Taler developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/aboutpage.h>
#include <qt/clientmodel.h>
#include <qt/platformstyle.h>
#include <clientversion.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QFrame>

AboutPage::AboutPage(const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    clientModel(nullptr),
    platformStyle(_platformStyle)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header: icon + name + version
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *iconLabel = new QLabel();
    QPixmap appIcon(":/icons/bitcoin");
    iconLabel->setPixmap(appIcon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    headerLayout->addWidget(iconLabel);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    QLabel *nameLabel = new QLabel(tr("Taler"));
    nameLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    titleLayout->addWidget(nameLabel);
    QLabel *versionLabel = new QLabel(QString::fromStdString(FormatFullVersion()));
    versionLabel->setStyleSheet("font-size: 13px; color: #888;");
    titleLayout->addWidget(versionLabel);
    QLabel *websiteLink = new QLabel(
        "<a href=\"https://taler.tech\" style=\"text-decoration: none; color: #1B8FBA;\">taler.tech</a>");
    websiteLink->setTextFormat(Qt::RichText);
    websiteLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    websiteLink->setOpenExternalLinks(true);
    websiteLink->setStyleSheet("font-size: 12px;");
    titleLayout->addWidget(websiteLink);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Separator
    QFrame *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep);
    mainLayout->addSpacing(8);

    // Grid: label on left, URL on right
    QGridLayout *grid = new QGridLayout();
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(6);
    int row = 0;

    auto addSectionHeader = [&](const QString &title) {
        if (row > 0) {
            grid->setRowMinimumHeight(row, 12);
            row++;
        }
        QLabel *hdr = new QLabel(title);
        hdr->setStyleSheet("font-size: 15px; font-weight: bold;");
        grid->addWidget(hdr, row, 0, 1, 2);
        row++;
    };

    auto addRow = [&](const QString &label, const QString &url, const QString &displayUrl) {
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 13px; padding-left: 12px;");
        grid->addWidget(lbl, row, 0, Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *link = new QLabel(
            QString("<a href=\"%1\" style=\"text-decoration: none;\">%2</a>")
            .arg(url, displayUrl));
        link->setTextFormat(Qt::RichText);
        link->setTextInteractionFlags(Qt::TextBrowserInteraction);
        link->setOpenExternalLinks(true);
        link->setStyleSheet("font-size: 13px;");
        grid->addWidget(link, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
        row++;
    };

    auto addPlainRow = [&](const QString &label, const QString &value) {
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 13px; padding-left: 12px;");
        grid->addWidget(lbl, row, 0, Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *val = new QLabel(value);
        val->setStyleSheet("font-size: 13px;");
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(val, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
        row++;
    };

    // Project
    addSectionHeader(tr("Project"));
    addPlainRow(tr("App Name"), "Taler");
    addPlainRow(tr("Version"), QString::fromStdString(FormatFullVersion()));
    addRow(tr("Website"), "https://taler.tech/", "taler.tech");
    addRow(tr("GitHub"), "https://github.com/abkvme/taler", "github.com/abkvme/taler");
    addRow(tr("GitHub (legacy)"), "https://github.com/cryptadev/taler", "github.com/cryptadev/taler");
    addRow(tr("License"), "https://github.com/abkvme/taler/blob/main/COPYING", "MIT License");

    // Community
    addSectionHeader(tr("Community"));
    addRow(tr("Telegram"), "https://t.me/talercommunity", "@talercommunity");

    // Network
    addSectionHeader(tr("Network"));
    addRow(tr("Seed Nodes"), "https://github.com/abkvme/taler-seeds", "github.com/abkvme/taler-seeds");
    addRow(tr("Explorer"), "https://explorer.talercoin.org/", "explorer.talercoin.org");
    addRow(tr("Explorer"), "https://explorer.talercrypto.com/", "explorer.talercrypto.com");

    // Development
    addSectionHeader(tr("Development"));
    addRow(tr("Issue Tracker"), "https://github.com/abkvme/taler/issues", "github.com/abkvme/taler/issues");
    addRow(tr("Change Log"), "https://github.com/abkvme/taler/blob/main/CHANGELOG.md", "github.com/abkvme/taler/.../CHANGELOG.md");

    mainLayout->addLayout(grid);
    mainLayout->addStretch();

    // Footer
    QLabel *footerLabel = new QLabel(tr("Maintained by abkvme, 2025-2026"));
    footerLabel->setStyleSheet("font-size: 11px; color: #888; padding: 4px 0;");
    footerLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(footerLabel);

    setLayout(mainLayout);
}

void AboutPage::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
}
