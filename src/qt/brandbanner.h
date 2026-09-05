// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BRANDBANNER_H
#define BITCOIN_QT_BRANDBANNER_H

#include <QColor>
#include <QPixmap>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

class QPainter;
class QPushButton;

/** The start-up artwork, and the pieces of it that more than one screen needs.
 *
 * The splash and the Info page show the same picture, so the language choice,
 * the fallback and the version placement live here rather than being written
 * twice and drifting apart.
 */
namespace brand {

//! Logical width the artwork is shown at on the splash and the shutdown window.
//! One number, so the two screens that bracket a session are the same size.
static const int ARTWORK_WIDTH = 600;
//! Height of the strip beneath it, where progress and status are written.
static const int ARTWORK_STRIP_HEIGHT = 30;

/** Artwork for the current UI language, falling back to English.
 *
 * Only some languages have their own image; the rest get the English one, which
 * beats a blank panel. Resources are aliased splash_<language>, so adding a
 * language needs a file and one line in bitcoin.qrc and no code at all.
 */
QPixmap Artwork();

/** The artwork's own bottom-left pixel.
 *
 * Used to colour anything drawn immediately below the picture, so it reads as
 * part of it and keeps doing so if the artwork is ever replaced.
 */
QColor ArtworkEdge(const QPixmap& artwork);

/** True when the artwork is dark enough to need light text on top of it. */
bool ArtworkIsDark(const QPixmap& artwork);

/** Draw the version, and any network label, into the top-right of `area`.
 *
 * Every one of the images leaves that corner clear. The network label goes
 * underneath rather than beside, so a long version string can never push it off
 * the edge.
 */
void DrawVersion(QPainter& painter, const QRect& area, int margin, bool dark_artwork,
                 const QString& network_label);

/** A link printed on the artwork, as a fraction of the image.
 *
 * Fractions rather than pixels because the picture is drawn at whatever size it
 * is given. Measured from the artwork itself; all three languages place these
 * identically, which is checked by SplashTests.
 */
struct Link
{
    QRectF fraction;
    QString url;
};
QVector<Link> Links();

} // namespace brand

/** The artwork as a widget, with its printed links made clickable.
 *
 * The URLs are part of the picture, so there is nothing for Qt to make
 * interactive on its own. This lays transparent buttons exactly over them.
 */
class BrandBanner : public QWidget
{
    Q_OBJECT

public:
    explicit BrandBanner(QWidget* parent = nullptr);

    /** Turn the printed links off.
     *
     * They are the point of the banner on the Info page and pointless on a window
     * that is about to close, where a click would open a browser behind a
     * disappearing application.
     */
    void setLinksEnabled(bool enabled);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    //! Where the artwork is actually drawn: it keeps its aspect ratio, so a
    //! wider widget leaves a margin either side rather than stretching it.
    QRect artworkRect() const;

    QPixmap m_artwork;
    QVector<QPushButton*> m_link_buttons;
};

#endif // BITCOIN_QT_BRANDBANNER_H
