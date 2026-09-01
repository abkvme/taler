// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/theme.h>

#include <QApplication>
#include <QPalette>
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
};

const Colours DARK = {
    "#16181d", "#1e2128", "#262b34", "#2f353f",
    "#e8eaed", "#9aa2ad",
    "#0088cc", "#17a0e6", "#ffffff",
    "#0088cc", "#e5534b",
};

const Colours LIGHT = {
    "#f5f6f8", "#ffffff", "#eef1f5", "#d9dee5",
    "#1b1f24", "#6b7280",
    "#0088cc", "#0b9ae0", "#ffffff",
    "#0088cc", "#d1342b",
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
QTabBar::tab:selected { background-color: %SURFACE2%; color: %TEXT%; }

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
    qss.replace("%SURFACE2%", c.surface2); // before %SURFACE% so the longer key wins
    qss.replace("%SURFACE%", c.surface);
    qss.replace("%BORDER%", c.border);
    qss.replace("%TEXT%", c.text);
    qss.replace("%MUTED%", c.muted);
    qss.replace("%ACCENTHOVER%", c.accentHover);
    qss.replace("%ACCENTTEXT%", c.accentText);
    qss.replace("%ACCENT%", c.accent);
    qss.replace("%DANGER%", c.danger);
    return qss;
}

//! Keep the palette in step with the stylesheet, so widgets that draw themselves
//! (native dialogs, some item delegates) do not stay in the old colours.
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

    if (t == Theme::System) {
        qApp->setStyleSheet(QString());
        qApp->setPalette(qApp->style()->standardPalette());
        return;
    }

    const Colours& c = (t == Theme::Dark) ? DARK : LIGHT;
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
