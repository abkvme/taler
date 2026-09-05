// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/brandbanner.h>

#include <qt/guiutil.h>

#include <clientversion.h>

#include <QApplication>
#include <QDesktopServices>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QUrl>

namespace brand {

QPixmap Artwork()
{
    QPixmap artwork(QString(":/images/splash_%1").arg(GUIUtil::language()));
    if (artwork.isNull()) {
        artwork.load(QString(":/images/splash_en"));
    }
    return artwork;
}

QColor ArtworkEdge(const QPixmap& artwork)
{
    if (artwork.isNull()) return QColor(10, 25, 48);
    const QImage sampled = artwork.toImage();
    if (sampled.isNull()) return QColor(10, 25, 48);
    return QColor(sampled.pixel(2, sampled.height() - 2));
}

bool ArtworkIsDark(const QPixmap& artwork)
{
    return ArtworkEdge(artwork).lightness() < 128;
}

void DrawVersion(QPainter& painter, const QRect& area, int margin, bool dark_artwork,
                 const QString& network_label)
{
    const QString font = QApplication::font().toString();

    QFont versionFont(font, 11);
    versionFont.setWeight(QFont::DemiBold);
    painter.setFont(versionFont);
    painter.setPen(dark_artwork ? QColor(232, 238, 248) : QColor(40, 40, 40));

    const QRect versionRect(area.left(), area.top() + margin,
                            area.width() - margin, QFontMetrics(versionFont).height());
    painter.drawText(versionRect, Qt::AlignRight | Qt::AlignTop,
                     QString::fromStdString(FormatFullVersion()));

    if (network_label.isEmpty()) return;

    QFont networkFont(font, 10);
    networkFont.setWeight(QFont::Bold);
    painter.setFont(networkFont);
    painter.setPen(dark_artwork ? QColor(214, 154, 46) : QColor(150, 100, 20));
    painter.drawText(versionRect.translated(0, versionRect.height() + 2),
                     Qt::AlignRight | Qt::AlignTop, network_label);
}

QVector<Link> Links()
{
    // Measured off the artwork itself: the pills sit at x 304-584 and 615-895,
    // y 700-757 of the 1200x800 image, identically in all three languages.
    // Expressed as fractions so they follow the picture to whatever size it is
    // drawn at.
    return {
        {QRectF(304.0 / 1200.0, 700.0 / 800.0, 280.0 / 1200.0, 57.0 / 800.0),
         QStringLiteral("https://taler.tech/")},
        {QRectF(615.0 / 1200.0, 700.0 / 800.0, 280.0 / 1200.0, 57.0 / 800.0),
         QStringLiteral("https://explorer.taler.tech/")},
    };
}

} // namespace brand

BrandBanner::BrandBanner(QWidget* parent) :
    QWidget(parent),
    m_artwork(brand::Artwork())
{
    // heightForWidth has to be advertised on the policy, not just implemented, or
    // a QVBoxLayout ignores it and the artwork gets a square-ish box.
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    for (const brand::Link& link : brand::Links()) {
        QPushButton* button = new QPushButton(this);
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::TabFocus);
        button->setToolTip(link.url);
        // The label is in the picture, so a screen reader would otherwise find an
        // unnamed button here.
        button->setAccessibleName(tr("Open %1").arg(link.url));
        // Transparent at rest, because the artwork already draws the button. The
        // faint wash on hover is the only hint that the printed pill is live -
        // without it nobody would think to click a picture.
        button->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 14px; }"
            "QPushButton:hover { background: rgba(255, 255, 255, 26); }"
            "QPushButton:pressed { background: rgba(255, 255, 255, 46); }"
            "QPushButton:focus { border: 1px solid rgba(255, 255, 255, 90); }");
        const QString url = link.url;
        connect(button, &QPushButton::clicked, this,
                [url] { QDesktopServices::openUrl(QUrl(url)); });
        m_link_buttons.append(button);
    }
}

void BrandBanner::setLinksEnabled(bool enabled)
{
    for (QPushButton* button : m_link_buttons) {
        button->setVisible(enabled);
        button->setEnabled(enabled);
    }
}

QSize BrandBanner::sizeHint() const
{
    if (m_artwork.isNull()) return QSize(600, 400);
    return QSize(600, heightForWidth(600));
}

QSize BrandBanner::minimumSizeHint() const
{
    if (m_artwork.isNull()) return QSize(320, 213);
    return QSize(320, heightForWidth(320));
}

int BrandBanner::heightForWidth(int width) const
{
    if (m_artwork.isNull() || m_artwork.width() <= 0) return width * 2 / 3;
    return width * m_artwork.height() / m_artwork.width();
}

QRect BrandBanner::artworkRect() const
{
    if (m_artwork.isNull() || m_artwork.width() <= 0) return rect();

    // Fit inside the widget without distorting: the artwork is the only thing on
    // this widget, so any leftover space becomes an even margin.
    QSize scaled = m_artwork.size();
    scaled.scale(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2),
                 scaled);
}

void BrandBanner::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect area = artworkRect();
    if (m_artwork.isNull()) {
        painter.fillRect(rect(), QColor(10, 25, 48));
    } else {
        painter.fillRect(rect(), brand::ArtworkEdge(m_artwork));
        painter.drawPixmap(area, m_artwork);
    }

    brand::DrawVersion(painter, area, 12, brand::ArtworkIsDark(m_artwork), QString());
}

void BrandBanner::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Hotspots follow the artwork, not the widget: when the widget is wider than
    // the picture the picture is centred, and the buttons have to move with it.
    const QRect area = artworkRect();
    const QVector<brand::Link> links = brand::Links();
    for (int i = 0; i < m_link_buttons.size() && i < links.size(); ++i) {
        const QRectF f = links[i].fraction;
        m_link_buttons[i]->setGeometry(
            QRect(area.left() + qRound(f.x() * area.width()),
                  area.top() + qRound(f.y() * area.height()),
                  qRound(f.width() * area.width()),
                  qRound(f.height() * area.height())));
    }
}
