// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigDialog>

/**
 * The Preferences dialog: one page per feature. Widgets named
 * "kcfg_<Entry>" are bound to the Settings singleton by KConfigDialog.
 */
class SettingsDialog : public KConfigDialog
{
    Q_OBJECT

public:
    /// Name under which KConfigDialog::exists() finds the open dialog.
    static constexpr auto Name = "settings";

    explicit SettingsDialog(QWidget *parent);

private:
    QWidget *createStartupPage();
    QWidget *createPanesPage();
    QWidget *createViewsPage();
    QWidget *createPreviewsPage();
};
