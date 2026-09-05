// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/theme.h>

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QSettings>
#include <QString>
#include <QStyle>
#include <QStyleFactory>

namespace theme {

namespace {

//! Every colour the stylesheet uses. Light and dark differ only in these values.
struct Colours {
    QString window;    //!< application background
    QString surface;   //!< panels, inputs, table background
    QString surface2;  //!< alternating rows, hovered surfaces
    QString border;
    QString text;
    QString muted;     //!< secondary text
    QString accent;    //!< Taler blue, from the project mark
    QString accentHover;
    QString accentText; //!< text drawn on top of the accent
    QString selection;
    QString danger;
    QString dangerHover;
    QString success;   //!< earning, ready - the green states
    QString warning;   //!< slowing down - the amber state
    QString neutral;   //!< a state that is neither good nor bad, only "not yet"
    QString overlay_scrim; //!< dims the page behind the start-up sync panel
};

const Colours DARK = {
    "#16181d", "#1e2128", "#262b34", "#2f353f",
    "#e8eaed", "#9aa2ad",
    "#0088cc", "#17a0e6", "#ffffff",
    "#0088cc", "#e5534b", "#f0655d",
    "#2ea043", "#c99a2e", "#39404b",
    "rgba(0, 0, 0, 200)",
};

const Colours LIGHT = {
    "#f5f6f8", "#ffffff", "#eef1f5", "#d9dee5",
    "#1b1f24", "#6b7280",
    "#0088cc", "#0b9ae0", "#ffffff",
    "#0088cc", "#d1342b", "#e04a41",
    "#2f9e44", "#c98a11", "#e3e8ef",
    // Lighter than the dark theme's: on a pale page a near-black scrim reads as
    // a fault rather than as depth.
    "rgba(27, 31, 36, 140)",
};

/**
 * One stylesheet, filled in from the colour set.
 *
 * Deliberately restrained: rounded corners, real spacing, a single accent, and
 * hover/focus states so controls respond. A wallet should look calm, not decorated.
 */
QString BuildStyleSheet(const Colours& c)
{
    QString qss = R"QSS(
/* ---- base ---------------------------------------------------------- */
QWidget {
    background-color: %WINDOW%;
    color: %TEXT%;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENTTEXT%;
}
QMainWindow, QDialog { background-color: %WINDOW%; }
QLabel { background: transparent; }
/* Bare containers that sit inside a card must not repaint the window colour over
   it. The base QWidget rule above is what makes them, so they have to opt out. */
QWidget#recoveryRow { background: transparent; }
QToolTip {
    background-color: %SURFACE2%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 6px 8px;
}

/* ---- balance badge ---------------------------------------------------- */
QFrame#balanceBadge {
    background-color: %ACCENT%;
    border: none;
    border-radius: %RADIUS%;
}
QLabel#balanceBadgeCaption {
    color: %ACCENTTEXT%;
    font-size: 12px;
    background: transparent;
}
QLabel#balanceBadgeValue {
    color: %ACCENTTEXT%;
    font-size: 26px;
    font-weight: 600;
    background: transparent;
}

/* ---- status badges ---------------------------------------------------
   Same shape as the balance badge above, coloured by a dynamic property. Qt does
   not re-evaluate a stylesheet when such a property changes, so OverviewPage
   repolishes the badge and its labels; the colours themselves stay here. */
QFrame[badge="true"] {
    border: none;
    border-radius: %RADIUS%;
    background-color: %NEUTRAL%;
}
QFrame[badgeState="good"] { background-color: %SUCCESS%; }
QFrame[badgeState="warn"] { background-color: %WARNING%; }
QFrame[badgeState="bad"]  { background-color: %DANGER%; }

QFrame[badge="true"] QLabel {
    background: transparent;
    color: %TEXT%;
}
QFrame[badgeState="good"] QLabel,
QFrame[badgeState="warn"] QLabel,
QFrame[badgeState="bad"] QLabel {
    color: %ACCENTTEXT%;
}
QLabel[badgeCaption="true"] {
    font-size: 11px;
}
QFrame[badgeState="good"] QLabel[badgeCaption="true"],
QFrame[badgeState="warn"] QLabel[badgeCaption="true"],
QFrame[badgeState="bad"] QLabel[badgeCaption="true"] {
    /* Slightly held back, so the headline still leads on a saturated ground. */
    color: rgba(255, 255, 255, 200);
}
QFrame[badgeState="idle"] QLabel[badgeCaption="true"] { color: %MUTED%; }
QLabel[badgeValue="true"] {
    font-size: 17px;
    font-weight: 600;
}

/* ---- rewards chart ---------------------------------------------------- */
RewardsChart {
    qproperty-barColor: %ACCENT%;
    qproperty-labelColor: %MUTED%;
    qproperty-gridColor: %BORDER%;
    background: transparent;
}
QLabel[class="cardTotal"] {
    font-size: 13px;
    font-weight: 600;
    color: %ACCENT%;
}

/* ---- typography ------------------------------------------------------
   One scale for the whole application, applied through dynamic properties so a
   page does not have to invent its own bold labels. */
QLabel[class="walletName"] {
    font-size: 15px;
    font-weight: 600;
    color: %TEXT%;
    padding: 2px 2px 8px 2px;
}
QLabel[class="cardTitle"] {
    color: %MUTED%;
    font-weight: 600;
    padding: 2px 0 6px 0;
}
/* A page's own heading: what this screen is, and one line on what it is for.
   Full-strength text and larger, unlike cardTitle, which labels a panel inside
   a page and is deliberately quiet. */
QLabel[class="pageTitle"] {
    color: %TEXT%;
    font-size: 17px;
    font-weight: bold;
}
QLabel[class="pageSubtitle"] { color: %MUTED%; }
QLabel[class="cardHint"] { color: %MUTED%; }
QLabel[class="heroCaption"] { color: %MUTED%; }
QLabel[class="heroBalance"] {
    font-size: 26px;
    font-weight: 600;
    color: %TEXT%;
    padding: 2px 0;
}
QLabel[class="secondaryCaption"] { color: %MUTED%; }
QLabel[class="secondaryValue"] { color: %TEXT%; }
QLabel[class="emptyState"] {
    color: %MUTED%;
    background: transparent;
}

/* ---- start-up sync overlay -------------------------------------------
   The "recent transactions may not be visible" panel shown while the wallet
   catches up. Its form used to hard-code a near-white card with dark grey text,
   which stayed white in dark mode and was the first thing a user saw. Colours
   belong here with everything else so the two themes cannot drift apart. */
QWidget#bgWidget {
    background: %OVERLAY_SCRIM%;
}
QWidget#contentWidget {
    background-color: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: %RADIUS%;
}
QWidget#contentWidget QLabel { color: %TEXT%; }
/* The progress figures read as secondary next to the warning itself. */
QWidget#contentWidget QLabel#labelSyncDone { color: %MUTED%; }
/* Flat, disabled, and only ever a picture: no button chrome around the glyph. */
QPushButton#warningIcon {
    background: transparent;
    border: none;
    padding: 0px;
}

/* ---- cards -----------------------------------------------------------
   One card style for every panel in the application. The forms use a mix of
   StyledPanel/Raised, StyledPanel/Sunken and NoFrame; specifying a border here
   overrides Qt's native 3D frame drawing, so the inner/outer shadow look
   disappears and every block matches.

   The leading dot restricts this to plain QFrame containers - QListView,
   QTableView, QScrollArea and WalletFrame are QFrame subclasses and keep their
   own rules below. Shapes: 1 Box, 2 Panel, 3 WinPanel, 6 StyledPanel. */
.QFrame[frameShape="1"], .QFrame[frameShape="2"],
.QFrame[frameShape="3"], .QFrame[frameShape="6"] {
    background-color: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: %RADIUS%;
    padding: 2px;
}
/* 4 HLine, 5 VLine: separators, not cards. */
.QFrame[frameShape="4"], .QFrame[frameShape="5"] {
    background-color: %BORDER%;
    border: none;
    max-height: 1px;
    max-width: 16777215px;
}
QFrame[frameShape="5"] { max-width: 1px; max-height: 16777215px; }

QGroupBox {
    background-color: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: %RADIUS%;
    margin-top: 18px;
    padding: 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 4px;
    color: %MUTED%;
}

/* ---- toolbar -------------------------------------------------------- */
QToolBar {
    background-color: %SURFACE%;
    border: none;
    border-bottom: 1px solid %BORDER%;
    padding: 6px 8px;
    spacing: 4px;
}
QToolBar QToolButton {
    background: transparent;
    border: none;
    border-radius: 8px;
    padding: 7px 12px;
    margin: 0 1px;
    color: %MUTED%;
}
QToolBar QToolButton:hover { background-color: %SURFACE2%; color: %TEXT%; }
QToolBar QToolButton:checked {
    background-color: %SURFACE2%;
    color: %ACCENT%;
    border-bottom: 2px solid %ACCENT%;
    border-radius: 8px;
}
QToolBar QToolButton:disabled { color: %BORDER%; }
/* The stretch that pushes Exit to the far right must not paint over the bar. */
QToolBar QWidget#toolbarSpacer { background: transparent; }
/* Exit is not navigation, so it does not take the accent the tabs use. It stays
   quiet until pointed at, then warms towards the danger colour - enough to say
   what it does without making the toolbar look alarming. */
QToolBar QToolButton#settingsButton, QToolBar QToolButton#exitButton { color: %MUTED%; }
QToolBar QToolButton#settingsButton:hover { background-color: %SURFACE2%; color: %TEXT%; }
QToolBar QToolButton#exitButton { color: %MUTED%; }
QToolBar QToolButton#exitButton:hover {
    background-color: %DANGER%;
    color: %ACCENTTEXT%;
}
QToolBar QToolButton#exitButton:pressed { background-color: %DANGER%; }

/* ---- icon buttons outside the toolbar -------------------------------- */
QToolButton {
    background-color: %SURFACE2%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 5px 7px;
}
QToolButton:hover { border-color: %ACCENT%; }
QToolButton:pressed { background-color: %BORDER%; }
QToolButton:disabled { color: %MUTED%; }
QToolButton::menu-indicator { image: none; }

/* ---- menus ---------------------------------------------------------- */
QMenuBar { background-color: %SURFACE%; border-bottom: 1px solid %BORDER%; }
QMenuBar::item { background: transparent; padding: 6px 10px; border-radius: 6px; }
QMenuBar::item:selected { background-color: %SURFACE2%; }
QMenu {
    background-color: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 6px;
}
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 6px; }
QMenu::item:selected { background-color: %ACCENT%; color: %ACCENTTEXT%; }
QMenu::separator { height: 1px; background: %BORDER%; margin: 6px 8px; }

/* ---- buttons -------------------------------------------------------- */
QPushButton {
    background-color: %SURFACE2%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 7px 16px;
    min-height: 18px;
}
/* Focus, without the dotted rectangle the platform would otherwise stamp inside
   the button. Same border the widget already has, in the accent colour, so
   nothing moves when focus arrives. */
QPushButton:focus { border-color: %ACCENT%; }
QToolButton:focus { border-color: %ACCENT%; }
QCheckBox:focus::indicator, QRadioButton:focus::indicator { border-color: %ACCENT%; }
QPushButton:hover { border-color: %ACCENT%; }
QPushButton:pressed { background-color: %BORDER%; }
QPushButton:disabled { color: %MUTED%; border-color: %BORDER%; }
QPushButton:default {
    background-color: %ACCENT%;
    border-color: %ACCENT%;
    color: %ACCENTTEXT%;
}
QPushButton:default:hover { background-color: %ACCENTHOVER%; border-color: %ACCENTHOVER%; }

/* ---- inputs --------------------------------------------------------- */
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QComboBox {
    background-color: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 6px 10px;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENTTEXT%;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus,
QDoubleSpinBox:focus, QDateEdit:focus, QComboBox:focus {
    border-color: %ACCENT%;
}
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { color: %MUTED%; background-color: %SURFACE2%; }
QLineEdit:read-only { background-color: %SURFACE2%; color: %MUTED%; }
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 14px; background: transparent; border: none; }
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    border: none;
    width: 22px;
}
QComboBox::down-arrow { image: url(:/icons/chevron_down); width: 10px; height: 10px; }
QComboBox:hover { border-color: %ACCENT%; }
QToolBar::separator { background: %BORDER%; width: 1px; margin: 6px 10px; }
QComboBox QAbstractItemView {
    background-color: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 4px;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENTTEXT%;
}

/* ---- checkboxes and radios ------------------------------------------ */
QCheckBox, QRadioButton { spacing: 8px; background: transparent; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }
QCheckBox::indicator {
    border: 1px solid %BORDER%;
    border-radius: 4px;
    background-color: %SURFACE%;
}
QCheckBox::indicator:checked { background-color: %ACCENT%; border-color: %ACCENT%; }
QRadioButton::indicator {
    border: 1px solid %BORDER%;
    border-radius: 8px;
    background-color: %SURFACE%;
}
QRadioButton::indicator:checked { background-color: %ACCENT%; border-color: %ACCENT%; }

/* ---- tables and lists ----------------------------------------------- */
QTableView, QTreeView, QListView, QScrollArea, QAbstractScrollArea {
    background-color: %SURFACE%;
    alternate-background-color: %SURFACE2%;
    border: 1px solid %BORDER%;
    border-radius: %RADIUS%;
    gridline-color: %BORDER%;
    outline: none;
}
QTableView::item, QTreeView::item, QListView::item { padding: 7px 8px; border: none; }
QTableView::item:selected, QTreeView::item:selected, QListView::item:selected {
    background-color: %ACCENT%;
    color: %ACCENTTEXT%;
}
/* A scroll area cannot clip its children to a rounded corner: Qt paints its
   background on the viewport, which is square, so the corners of the frame are
   covered up. Where the widget sits inside a card anyway - the tables on the
   Nodes page, the recipient list on the Send page - it is not a second card:
   dropping its frame and letting the card behind it show through is both the
   correct rendering and the flatter look. */
QGroupBox QAbstractScrollArea {
    background-color: transparent;
    border: none;
}
QGroupBox QHeaderView::section { background-color: transparent; }
QScrollArea#scrollArea, QScrollArea#scrollArea > QWidget > QWidget {
    background: transparent;
    border: none;
}
/* With the scroll area gone flat, each recipient carries the card. */
QFrame#SendCoins {
    background-color: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: %RADIUS%;
    padding: 6px;
}

QHeaderView { background-color: %SURFACE%; border: none; }
QTableCornerButton::section { background-color: %SURFACE2%; border: none; }
QHeaderView::section {
    background-color: %SURFACE2%;
    color: %MUTED%;
    border: none;
    border-bottom: 1px solid %BORDER%;
    padding: 8px 10px;
}

/* ---- tabs ----------------------------------------------------------- */
QTabWidget::pane { border: 1px solid %BORDER%; border-radius: %RADIUS%; top: -1px; }
QTabBar::tab {
    background: transparent;
    color: %MUTED%;
    border: none;
    padding: 8px 16px;
    margin-right: 2px;
    border-radius: 8px;
}
QTabBar::tab:hover { background-color: %SURFACE2%; color: %TEXT%; }
/* Keyboard focus on an unselected tab reads like hover; on the selected one the
   accent fill already says where you are, so it needs nothing further. */
QTabBar::tab:focus { background-color: %SURFACE2%; color: %TEXT%; }
/* The selected tab takes the accent outright. A tab that differs from its
   neighbours only by a slightly lighter grey makes the reader hunt for it. */
QTabBar::tab:selected {
    background-color: %ACCENT%;
    color: %ACCENTTEXT%;
    font-weight: bold;
}
QTabBar::tab:selected:hover { background-color: %ACCENTHOVER%; }

/* ---- options dialog --------------------------------------------------
   Its form is inherited and dates from a plainer era: two stacked full-width
   secondary buttons, and a note that carried the same weight as the settings
   above it. The layout is straightened out in OptionsDialog's constructor; what
   is left is telling the three kinds of button apart. */
QLabel#overriddenByCommandLineInfoLabel,
QLabel#overriddenByCommandLineLabel {
    color: %MUTED%;
}
QPushButton#openBitcoinConfButton, QPushButton#resetButton {
    background-color: transparent;
    border: 1px solid %BORDER%;
    color: %MUTED%;
}
QPushButton#openBitcoinConfButton:hover, QPushButton#resetButton:hover {
    border-color: %ACCENT%;
    color: %TEXT%;
}
/* OK is the one action that commits, so it is the only one that looks like it. */
QPushButton#okButton {
    background-color: %ACCENT%;
    border: 1px solid %ACCENT%;
    color: %ACCENTTEXT%;
    font-weight: bold;
    min-width: 84px;
}
QPushButton#okButton:hover { background-color: %ACCENTHOVER%; border-color: %ACCENTHOVER%; }
QPushButton#okButton:focus { border-color: %ACCENTTEXT%; }

/* The one button in a dialog that does something irreversible. Red, so it is
   never the one clicked by reflex - the safe answer stays the default. */
QPushButton#dangerButton {
    background-color: %DANGER%;
    border: 1px solid %DANGER%;
    color: %ACCENTTEXT%;
    font-weight: bold;
}
QPushButton#dangerButton:hover {
    background-color: %DANGERHOVER%;
    border-color: %DANGERHOVER%;
}
QPushButton#dangerButton:focus { border-color: %ACCENTTEXT%; }
QPushButton#cancelButton { min-width: 84px; }

/* ---- scrollbars ----------------------------------------------------- */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: %BORDER%;
    border-radius: 5px;
    min-height: 28px;
    min-width: 28px;
}
QScrollBar::handle:hover { background: %MUTED%; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ---- progress and status -------------------------------------------- */
QProgressBar {
    background-color: %SURFACE2%;
    border: none;
    border-radius: 6px;
    height: 8px;
    text-align: center;
    color: %TEXT%;
}
QProgressBar::chunk { background-color: %ACCENT%; border-radius: 6px; }
QStatusBar { background-color: %SURFACE%; border-top: 1px solid %BORDER%; }
QStatusBar::item { border: none; }
)QSS";

    qss.replace("%RADIUS%", "12px"); // single radius for every container
    qss.replace("%WINDOW%", c.window);
    // A scrim, not a colour: dark themes need to darken and light themes to
    // darken less, but both must keep the page behind it faintly readable.
    qss.replace("%OVERLAY_SCRIM%", c.overlay_scrim);
    qss.replace("%SURFACE2%", c.surface2); // before %SURFACE% so the longer key wins
    qss.replace("%SURFACE%", c.surface);
    qss.replace("%BORDER%", c.border);
    qss.replace("%TEXT%", c.text);
    qss.replace("%MUTED%", c.muted);
    qss.replace("%ACCENTHOVER%", c.accentHover);
    qss.replace("%ACCENTTEXT%", c.accentText);
    qss.replace("%ACCENT%", c.accent);
    qss.replace("%DANGERHOVER%", c.dangerHover); // before %DANGER% so the longer key wins
    qss.replace("%DANGER%", c.danger);
    qss.replace("%SUCCESS%", c.success);
    qss.replace("%WARNING%", c.warning);
    qss.replace("%NEUTRAL%", c.neutral);
    return qss;
}

//! Keep the palette in step with the stylesheet, so widgets that draw themselves
//! (native dialogs, some item delegates) do not stay in the old colours.
//! Colours of the theme currently applied, for the few things painted in code.
const Colours* g_current = nullptr;

//! A message-box icon in our own colours: a filled disc with a white glyph.
QIcon GlyphIcon(const QString& glyph, const QColor& colour)
{
    QIcon icon;
    for (int size : {32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colour);
        painter.drawEllipse(QRectF(1, 1, size - 2, size - 2));

        QFont font = QApplication::font();
        font.setPixelSize(int(size * 0.62));
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(QColor("#ffffff"));
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, glyph);
        painter.end();

        icon.addPixmap(pixmap);
    }
    return icon;
}

/**
 * The platform draws message-box icons in its own grey, which on the dark theme is
 * very nearly invisible against the dialog behind it. This paints the four of them
 * instead, in the colours the rest of the application already uses to mean the
 * same things.
 */
} // namespace

QIcon theme::ExitDoorIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        // Drawn on a 64x64 grid and scaled, so every size comes out consistent.
        const qreal k = size / 64.0;
        QPen pen(QColor("#ffffff"));
        pen.setWidthF(6.0 * k);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        // Three sides of the door frame, open towards the arrow.
        QPainterPath frame;
        frame.moveTo(38 * k, 11 * k);
        frame.lineTo(15 * k, 11 * k);
        frame.lineTo(15 * k, 53 * k);
        frame.lineTo(38 * k, 53 * k);
        painter.drawPath(frame);

        // And the way out through it.
        painter.drawLine(QPointF(30 * k, 32 * k), QPointF(55 * k, 32 * k));
        QPainterPath head;
        head.moveTo(45 * k, 22 * k);
        head.lineTo(55 * k, 32 * k);
        head.lineTo(45 * k, 42 * k);
        painter.drawPath(head);
        painter.end();

        icon.addPixmap(pixmap);
    }
    return icon;
}

namespace {

//! True while one of our own stylesheets is applied.
//!
//! The dotted focus rectangle is only worth suppressing when something replaces
//! it. Under the System theme we apply no stylesheet at all, so the platform's
//! focus ring is the only indication a keyboard user gets and it has to stay.
bool g_own_focus_styling = false;

class AccentIconStyle : public QProxyStyle
{
public:
    explicit AccentIconStyle(QStyle* base) : QProxyStyle(base) {}

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                       QPainter* painter, const QWidget* widget) const override
    {
        // The dotted rectangle the platform draws inside a focused button or tab
        // fights every other line in the interface, and on a filled accent button
        // it reads as damage. Our stylesheets mark focus with an accent border
        // instead, which is the same information drawn in the app's own language.
        if (element == QStyle::PE_FrameFocusRect && g_own_focus_styling) return;
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    QIcon standardIcon(StandardPixmap pixmap, const QStyleOption* option,
                       const QWidget* widget) const override
    {
        if (g_current) {
            switch (pixmap) {
            case SP_MessageBoxQuestion:
                return GlyphIcon(QString(QChar('?')), QColor(g_current->accent));
            case SP_MessageBoxInformation:
                return GlyphIcon(QString(QChar('i')), QColor(g_current->accent));
            case SP_MessageBoxWarning:
                return GlyphIcon(QString(QChar('!')), QColor(g_current->warning));
            case SP_MessageBoxCritical:
                return GlyphIcon(QString(QChar('!')), QColor(g_current->danger));
            default:
                break;
            }
        }
        return QProxyStyle::standardIcon(pixmap, option, widget);
    }
};

//! Installed once. QApplication deletes the style it replaces, so the proxy is
//! given a fresh instance of the platform style rather than the live one.
void InstallIconStyle()
{
    static bool installed = false;
    if (installed) return;
    installed = true;

    QStyle* base = QStyleFactory::create(QApplication::style()->objectName());
    qApp->setStyle(new AccentIconStyle(base));
}

void ApplyPalette(const Colours& c)
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(c.window));
    p.setColor(QPalette::WindowText, QColor(c.text));
    p.setColor(QPalette::Base, QColor(c.surface));
    p.setColor(QPalette::AlternateBase, QColor(c.surface2));
    p.setColor(QPalette::Text, QColor(c.text));
    p.setColor(QPalette::Button, QColor(c.surface2));
    p.setColor(QPalette::ButtonText, QColor(c.text));
    p.setColor(QPalette::Highlight, QColor(c.accent));
    p.setColor(QPalette::HighlightedText, QColor(c.accentText));
    p.setColor(QPalette::ToolTipBase, QColor(c.surface2));
    p.setColor(QPalette::ToolTipText, QColor(c.text));
    p.setColor(QPalette::PlaceholderText, QColor(c.muted));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(c.muted));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(c.muted));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(c.muted));
    qApp->setPalette(p);
}

} // namespace

bool SystemPrefersDark()
{
    // No portable API for this in Qt 5, so judge by the style's own window colour.
    const QColor window = qApp->style()->standardPalette().color(QPalette::Window);
    return window.lightness() < 128;
}

void Apply(Theme t)
{
    if (!qApp) return;

    // Before the palette and stylesheet: replacing the style resets the palette,
    // so it has to happen first or the theme is undone as it is applied.
    InstallIconStyle();

    if (t == Theme::System) {
        // Even here the message-box icons stay ours - the platform's grey glyph is
        // the thing being fixed, and it is no more native for being unreadable.
        g_current = &LIGHT;
        if (SystemPrefersDark()) g_current = &DARK;
        g_own_focus_styling = false;
        qApp->setStyleSheet(QString());
        qApp->setPalette(qApp->style()->standardPalette());
        return;
    }

    const Colours& c = (t == Theme::Dark) ? DARK : LIGHT;
    g_current = &c;
    g_own_focus_styling = true;
    ApplyPalette(c);
    qApp->setStyleSheet(BuildStyleSheet(c));
}

Theme FromSettings()
{
    QSettings settings;
    const int value = settings.value("theme", static_cast<int>(Theme::System)).toInt();
    switch (value) {
    case static_cast<int>(Theme::Light): return Theme::Light;
    case static_cast<int>(Theme::Dark): return Theme::Dark;
    default: return Theme::System;
    }
}

void Save(Theme t)
{
    QSettings settings;
    settings.setValue("theme", static_cast<int>(t));
    Apply(t);
}

} // namespace theme
