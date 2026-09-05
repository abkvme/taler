// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_REWARDSCHART_H
#define BITCOIN_QT_REWARDSCHART_H

#include <qt/rewardsummary.h>

#include <QColor>
#include <QWidget>

/** Twelve months of block rewards, one bar each, the current month at the right.
 *
 * Painted rather than charted: twelve bars and a row of labels do not justify a
 * charting library, and the traffic graph on the Nodes page already establishes
 * the pattern. Colours arrive from the stylesheet through the properties below,
 * so light and dark are decided in one place with the rest of the theme.
 */
class RewardsChart : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor barColor READ barColor WRITE setBarColor)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor)

public:
    explicit RewardsChart(QWidget* parent = nullptr);

    void setSummary(const RewardSummary& summary, int display_unit);

    QColor barColor() const { return m_bar_color; }
    void setBarColor(const QColor& color);
    QColor labelColor() const { return m_label_color; }
    void setLabelColor(const QColor& color);
    QColor gridColor() const { return m_grid_color; }
    void setGridColor(const QColor& color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRect plotArea() const;
    QRect barRect(int index, const QRect& plot, CAmount maximum) const;
    int barAt(const QPoint& position) const;

    RewardSummary m_summary;
    int m_unit;
    int m_hovered = -1;

    QColor m_bar_color;
    QColor m_label_color;
    QColor m_grid_color;
};

#endif // BITCOIN_QT_REWARDSCHART_H
