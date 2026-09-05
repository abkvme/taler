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
#include <QResizeEvent>

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
    nameLabel->setStyleSheet("font-size: 26px; font-weight: bold;");
    titleLayout->addWidget(nameLabel);
    QLabel *versionLabel = new QLabel(QString::fromStdString(FormatFullVersion()));
    versionLabel->setStyleSheet("font-size: 14px; color: #888;");
    titleLayout->addWidget(versionLabel);
    QLabel *websiteLink = new QLabel(
        "<a href=\"https://taler.tech\" style=\"text-decoration: none; color: #1B8FBA;\">taler.tech</a>");
    websiteLink->setTextFormat(Qt::RichText);
    websiteLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    websiteLink->setOpenExternalLinks(true);
    websiteLink->setStyleSheet("font-size: 13px;");
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

    // Each section is built as its own widget so the page can put them in one
    // column or two without rebuilding anything - see arrangeSections().
    QGridLayout *grid = nullptr;
    int row = 0;

    auto beginSection = [&](const QString &title) {
        QWidget *section = new QWidget(this);
        QVBoxLayout *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(4);

        QLabel *hdr = new QLabel(title, section);
        hdr->setStyleSheet("font-size: 17px; font-weight: bold;");
        sectionLayout->addWidget(hdr);

        grid = new QGridLayout();
        grid->setColumnStretch(0, 0);
        grid->setColumnStretch(1, 1);
        grid->setHorizontalSpacing(20);
        grid->setVerticalSpacing(6);
        sectionLayout->addLayout(grid);
        row = 0;

        m_sections.append(section);
    };

    auto addRow = [&](const QString &label, const QString &url, const QString &displayUrl) {
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 15px; padding-left: 12px;");
        grid->addWidget(lbl, row, 0, Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *link = new QLabel(
            QString("<a href=\"%1\" style=\"text-decoration: none;\">%2</a>")
            .arg(url, displayUrl));
        link->setTextFormat(Qt::RichText);
        link->setTextInteractionFlags(Qt::TextBrowserInteraction);
        link->setOpenExternalLinks(true);
        link->setStyleSheet("font-size: 15px;");
        grid->addWidget(link, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
        row++;
    };

    auto addPlainRow = [&](const QString &label, const QString &value) {
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 15px; padding-left: 12px;");
        grid->addWidget(lbl, row, 0, Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *val = new QLabel(value);
        val->setStyleSheet("font-size: 15px;");
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(val, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
        row++;
    };

    // Project - much the longest section, which is why it gets a column to itself.
    beginSection(tr("Project"));
    addPlainRow(tr("App Name"), "Taler");
    addPlainRow(tr("Version"), QString::fromStdString(FormatFullVersion()));
    addRow(tr("Website"), "https://taler.tech/", "taler.tech");
    addRow(tr("GitHub"), "https://github.com/abkvme/taler", "github.com/abkvme/taler");
    addRow(tr("GitHub (legacy)"), "https://github.com/cryptadev/taler", "github.com/cryptadev/taler");
    addRow(tr("License"), "https://github.com/abkvme/taler/blob/main/COPYING", "MIT License");

    beginSection(tr("Community"));
    addRow(tr("Telegram"), "https://t.me/talercommunity", "@talercommunity");

    beginSection(tr("Network"));
    addRow(tr("Seed Nodes"), "https://github.com/abkvme/taler-seeds", "github.com/abkvme/taler-seeds");
    addRow(tr("Explorer"), "https://explorer.taler.tech/", "explorer.taler.tech");

    beginSection(tr("Development"));
    addRow(tr("Issue Tracker"), "https://github.com/abkvme/taler/issues", "github.com/abkvme/taler/issues");
    addRow(tr("Change Log"), "https://github.com/abkvme/taler/blob/main/CHANGELOG.md", "github.com/abkvme/taler/.../CHANGELOG.md");

    m_columns = new QHBoxLayout();
    m_columns->setContentsMargins(0, 0, 0, 0);
    m_columns->setSpacing(40);
    m_left_column = new QVBoxLayout();
    m_right_column = new QVBoxLayout();
    m_left_column->setSpacing(18);
    m_right_column->setSpacing(18);
    m_columns->addLayout(m_left_column, 1);
    m_columns->addLayout(m_right_column, 1);
    mainLayout->addLayout(m_columns);
    arrangeSections();
    mainLayout->addStretch();

    // Footer
    QLabel *footerLabel = new QLabel(tr("Maintained by abkvme, 2025-2026"));
    footerLabel->setStyleSheet("font-size: 12px; color: #888; padding: 4px 0;");
    footerLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(footerLabel);

    setLayout(mainLayout);
}

namespace {
//! Below this the page keeps a single column: two narrow ones read worse than
//! one wide one, and the long GitHub URLs need the room.
const int TWO_COLUMN_WIDTH = 900;
} // namespace

void AboutPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    arrangeSections();
}

void AboutPage::arrangeSections()
{
    const int wanted = width() >= TWO_COLUMN_WIDTH ? 2 : 1;
    if (wanted == m_column_count) return;
    m_column_count = wanted;

    // Empty both columns first. Taking an item out does not delete the widget it
    // holds - those stay owned by the page - so they can simply be re-added.
    for (QVBoxLayout *column : {m_left_column, m_right_column}) {
        while (QLayoutItem *item = column->takeAt(0)) delete item;
    }

    for (int i = 0; i < m_sections.size(); ++i) {
        const bool right = (wanted == 2 && i > 0);
        (right ? m_right_column : m_left_column)->addWidget(m_sections[i]);
    }
    m_left_column->addStretch();
    if (wanted == 2) m_right_column->addStretch();

    // An empty column must not reserve half the page.
    m_columns->setStretch(1, wanted == 2 ? 1 : 0);
}

void AboutPage::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
}
