// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keybindings.h"
#include "keymapfile.h"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;

/**
 * Shows what importing a keymap file would change before anything is
 * applied: the shortcuts that change, the conflicts they add or resolve, the
 * keyboard settings and what the file has that isn't used. Apply accepts the
 * dialog; the caller applies preview().
 */
class KeymapImportDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @param fileName the file's name, for the user
     * @param bindings the window's bindings, as WindowShortcuts::bindings() lists them
     * @param keymap the file's keymap
     * @param warnings what was skipped while reading the file
     * @param settingChanges the keyboard settings the file changes, each described for the user
     */
    KeymapImportDialog(const QString &fileName,
                       const QList<KeyBindings::Binding> &bindings,
                       const KeymapFile::Keymap &keymap,
                       const QStringList &warnings,
                       const QStringList &settingChanges,
                       QWidget *parent = nullptr);

    /// What importing does with the current choice of resetting the actions the file doesn't list.
    KeymapFile::Preview preview() const;
    /// Whether the actions the file doesn't list get their defaults.
    bool resetsUnlisted() const;

private:
    void refresh();

    QList<KeyBindings::Binding> m_bindings;
    KeymapFile::Keymap m_keymap;
    QStringList m_warnings;
    QStringList m_settingChanges;
    KeymapFile::Preview m_preview;
    QLabel *m_summary;
    QTreeWidget *m_changes;
    QLabel *m_conflictSummary;
    QListWidget *m_conflicts;
    QListWidget *m_skipped;
    QCheckBox *m_resetUnlisted;
    QPushButton *m_apply;
};
