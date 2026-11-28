// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keyboardmap.h"
#include "yazimode.h"

#include <QDialog>
#include <QPointer>

class KKeySequenceWidget;
class KXmlGuiWindow;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

/**
 * Help › Keyboard Map…: every action of a window with its description,
 * shortcuts and where they apply, plus the image gallery's keys, the chosen
 * Yazi mode key preset, the Vim-style navigation keys and the global
 * shortcuts. It is searched by typing
 * an action's name or by pressing keys, and exported as an HTML or PDF
 * reference. It only shows shortcuts; Configure Keyboard Shortcuts changes
 * them.
 */
class KeyboardMapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyboardMapDialog(KXmlGuiWindow *window, QWidget *parent = nullptr);

    /// Every entry of @p window's map, before filtering: its actions sorted by text, pressing Shift twice to search when it is on, the gallery's keys, the
    /// Yazi mode preset's commands, the Vim-style navigation commands (named vim- and the command's identifier), then the global shortcuts.
    static QList<KeyboardMap::Entry> entries(KXmlGuiWindow *window);

    /// Shows the entries whose shortcuts are, or begin with, @p keys, as pressing them in Find by Key does; empty keys show every entry.
    void setSearchKeys(const QKeySequence &keys);
    /// The entries the list shows, in order.
    QList<KeyboardMap::Entry> shownEntries() const;
    /// Writes the shown entries to @p path as Export… does, a PDF for a .pdf file and HTML otherwise; returns an error message or nothing.
    QString exportTo(const QString &path) const;

public Q_SLOTS:
    /// Reads the shortcuts again, after they change.
    void refresh();

private:
    /// One entry per command of @p keymap, named @p namePrefix and the command's identifier, listing its keys.
    static QList<KeyboardMap::Entry> keymapEntries(const YaziMode::Keymap &keymap, const QString &namePrefix, const QString &context);
    void updateList();
    void exportReference();

    QPointer<KXmlGuiWindow> m_window;
    QLineEdit *m_search = nullptr;
    KKeySequenceWidget *m_keys = nullptr;
    QCheckBox *m_withShortcuts = nullptr;
    QTreeWidget *m_list = nullptr;
    QLabel *m_count = nullptr;
    QPushButton *m_configure = nullptr;
    QList<KeyboardMap::Entry> m_entries;
    QList<KeyboardMap::Entry> m_shown;
};
