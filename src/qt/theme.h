// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_THEME_H
#define BITCOIN_QT_THEME_H

#include <QIcon>
#include <QString>

/**
 * Application appearance.
 *
 * Light and Dark apply a stylesheet built from one template and two colour sets, so
 * the two themes cannot drift apart. System applies nothing and leaves the platform
 * style alone, which is what a user who has tuned their desktop expects.
 */
namespace theme {

enum class Theme {
    System = 0, //!< follow the desktop; no stylesheet of our own
    Light = 1,
    Dark = 2,
};

//! Apply a theme to the running application, immediately and without a restart.
void Apply(Theme theme);

//! Theme currently stored in the settings (System when unset or unrecognised).
Theme FromSettings();

//! Store and apply in one step.
void Save(Theme theme);

//! Whether the desktop itself is currently dark. Used only to pick sensible colours
//! for widgets we draw ourselves while running in System mode.
bool SystemPrefersDark();

/** A door with an arrow leaving through it: the toolbar's Exit button.
 *
 * Drawn rather than shipped as a pixmap so it stays sharp at any size and can be
 * recoloured with the rest of the toolbar. White on transparent, because callers
 * pass it through PlatformStyle::TextColorIcon() to take the theme's text colour.
 */
QIcon ExitDoorIcon();

} // namespace theme

#endif // BITCOIN_QT_THEME_H
