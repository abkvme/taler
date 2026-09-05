// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/rewardschart.h>

#include <qt/bitcoinunits.h>

#include <QFontMetrics>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace {
//! Room under the bars for the month letters, and above them for breathing space.
const int LABEL_HEIGHT = 18;
const int TOP_PADDING = 8;
const int BAR_SPACING = 6;
const int BAR_RADIUS = 3;
//! A month with a real but tiny amount still has to be visible.
const int MINIMUM_BAR_HEIGHT = 2;
} // namespace

RewardsChart::RewardsChart(QWidget* parent) :
    QWidget(parent),
    m_unit(BitcoinUnits::BTC)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Sensible without a stylesheet, so the System theme - which applies none -
    // still gets a readable chart.
    m_bar_color = QColor(0x00, 0x88, 0xcc);
    m_label_color = palette().color(QPalette::WindowText);
    m_label_color.setAlpha(140);
    m_grid_color = palette().color(QPalette::Mid);
}

void RewardsChart::setSummary(const RewardSummary& summary, int display_unit)
{
    m_summary = summary;
    m_unit = display_unit;
    update();
}

void RewardsChart::setBarColor(const QColor& color) { m_bar_color = color; update(); }
void RewardsChart::setLabelColor(const QColor& color) { m_label_color = color; update(); }
void RewardsChart::setGridColor(const QColor& color) { m_grid_color = color; update(); }

QSize RewardsChart::sizeHint() const { return QSize(320, 132); }
QSize RewardsChart::minimumSizeHint() const { return QSize(180, 96); }

QRect RewardsChart::plotArea() const
{
    return QRect(0, TOP_PADDING, width(), height() - TOP_PADDING - LABEL_HEIGHT);
}

QRect RewardsChart::barRect(int index, const QRect& plot, CAmount maximum) const
{
    const int count = m_summary.monthly.size();
    if (count <= 0) return QRect();

    const double slot = double(plot.width()) / count;
    const int bar_width = qMax(3, int(slot) - BAR_SPACING);
    const int left = plot.left() + int(slot * index) + (int(slot) - bar_width) / 2;

    if (maximum <= 0) return QRect(left, plot.bottom(), bar_width, 0);

    const double fraction = double(m_summary.monthly[index]) / double(maximum);
    int bar_height = int(fraction * plot.height());
    if (m_summary.monthly[index] > 0 && bar_height < MINIMUM_BAR_HEIGHT) bar_height = MINIMUM_BAR_HEIGHT;

    return QRect(left, plot.bottom() - bar_height, bar_width, bar_height);
}

int RewardsChart::barAt(const QPoint& position) const
{
    const int count = m_summary.monthly.size();
    if (count <= 0) return -1;
    const QRect plot = plotArea();
    const double slot = double(plot.width()) / count;
    const int index = int((position.x() - plot.left()) / slot);
    if (index < 0 || index >= count) return -1;
    return index;
}

void RewardsChart::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect plot = plotArea();

    if (m_summary.monthly.isEmpty() || m_summary.maximum() <= 0) {
        // Nothing earned yet. An axis with twelve empty slots would look like a
        // failure to load rather than an honest zero.
        painter.setPen(m_label_color);
        painter.drawText(rect(), Qt::AlignCenter, tr("No rewards yet in this wallet"));
        return;
    }

    // Baseline, so the bars stand on something.
    painter.setPen(m_grid_color);
    painter.drawLine(plot.left(), plot.bottom(), plot.right(), plot.bottom());

    const CAmount maximum = m_summary.maximum();
    QFont label_font = font();
    label_font.setPointSizeF(qMax(7.0, font().pointSizeF() - 2.0));
    const QFontMetrics metrics(label_font);

    for (int i = 0; i < m_summary.monthly.size(); ++i) {
        const QRect bar = barRect(i, plot, maximum);
        if (bar.height() > 0) {
            QColor colour = m_bar_color;
            // The hovered bar lifts slightly, so it is clear which month the
            // tooltip is talking about.
            if (i == m_hovered) colour = colour.lighter(115);

            QPainterPath path;
            path.addRoundedRect(bar, BAR_RADIUS, BAR_RADIUS);
            painter.fillPath(path, colour);
        }

        // One letter per month, and the year under January so a twelve-month span
        // that crosses new year still reads unambiguously.
        const QDate month = m_summary.month_starts[i];
        const QString letter = QLocale().monthName(month.month(), QLocale::ShortFormat).left(1);
        const QString caption = (month.month() == 1 || i == 0)
                                    ? letter + QString("·") + QString::number(month.year() % 100)
                                    : letter;

        painter.setFont(label_font);
        painter.setPen(m_label_color);
        const QRect label(bar.left() - BAR_SPACING, plot.bottom() + 2,
                          bar.width() + 2 * BAR_SPACING, LABEL_HEIGHT);
        painter.drawText(label, Qt::AlignHCenter | Qt::AlignTop,
                         metrics.elidedText(caption, Qt::ElideNone, label.width()));
    }
}

void RewardsChart::mouseMoveEvent(QMouseEvent* event)
{
    const int index = barAt(event->pos());
    if (index != m_hovered) {
        m_hovered = index;
        update();
    }
    if (index >= 0 && index < m_summary.monthly.size()) {
        const QDate month = m_summary.month_starts[index];
        const QString text = QString("%1 %2 - %3")
                                 .arg(QLocale().monthName(month.month(), QLocale::LongFormat))
                                 .arg(month.year())
                                 .arg(BitcoinUnits::formatWithUnit(m_unit, m_summary.monthly[index],
                                                                   false, BitcoinUnits::separatorAlways));
        setToolTip(text);
    } else {
        setToolTip(QString());
    }
    QWidget::mouseMoveEvent(event);
}

void RewardsChart::leaveEvent(QEvent* event)
{
    if (m_hovered != -1) {
        m_hovered = -1;
        update();
    }
    QWidget::leaveEvent(event);
}
