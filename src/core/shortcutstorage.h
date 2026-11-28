// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>

#include <QKeySequence>
#include <QList>
#include <QString>

#include <optional>
#include <utility>

/**
 * Where Plasma Opus keeps the window's shortcuts: the Shortcuts group of its
 * configuration file, opus-file-managerrc, and nowhere else. Each action
 * whose shortcuts differ from its defaults has an entry named after the
 * action, in the format of KActionCollection::writeSettings: the shortcuts
 * in Qt's portable text, primary first, separated by "; ", or "none" for no
 * shortcut. Actions without an entry have their defaults. A Version entry
 * marks the format.
 *
 * KDE's Configure Keyboard Shortcuts dialog saves into the window's ui.rc
 * file instead, so Plasma Opus moves the shortcuts it finds there into the
 * group and removes them from the file.
 */
namespace ShortcutStorage
{
/// The configuration group of the shortcuts.
inline constexpr const char Group[] = "Shortcuts";
/// The entry of the group with its format version, which isn't an action.
inline constexpr const char VersionKey[] = "Version";
/// The current format version.
inline constexpr int Version = 1;

/// The shortcuts of an entry's text, or nothing when the text is empty or not a list of valid key sequences.
std::optional<QList<QKeySequence>> fromText(const QString &text);

/// The text of an entry for @p shortcuts; empty sequences are left out.
QString toText(const QList<QKeySequence> &shortcuts);

/// The shortcuts stored for action @p name, or nothing when it has its defaults or its entry isn't valid.
std::optional<QList<QKeySequence>> read(const KConfigGroup &group, const QString &name);

/// Stores @p shortcuts for action @p name, or removes its entry when they are its @p defaults. Empty sequences are ignored.
void write(KConfigGroup &group, const QString &name, const QList<QKeySequence> &shortcuts, const QList<QKeySequence> &defaults);

/// The names of the actions with an entry in @p group.
QStringList actionNames(const KConfigGroup &group);

/// Whether @p group has the current format version.
bool isCurrent(const KConfigGroup &group);

/// Gives @p group the current format version.
void markCurrent(KConfigGroup &group);

/// The shortcuts found in a ui.rc file.
struct XmlGuiShortcuts {
    /// Each action's shortcuts by name, in the order of the file; an empty list means no shortcut.
    QList<std::pair<QString, QList<QKeySequence>>> shortcuts;
    /// The file without its shortcuts, or empty when it has none or can't be read.
    QString stripped;
};

/**
 * The shortcut attributes of the Action elements in the ActionProperties of
 * @p uiRc for the shortcut @p scheme, as KDE's shortcut editor saves them,
 * and the file without them. Action elements left with only a name are
 * removed too. ActionProperties of other schemes are left as they are.
 */
XmlGuiShortcuts takeXmlGuiShortcuts(const QString &uiRc, const QString &scheme = QStringLiteral("Default"));

/**
 * Moves @p found into @p group, replacing the entries of the same actions,
 * and marks the group current. The ui.rc shortcuts are the newest, because
 * Plasma Opus removes them once they are stored.
 */
void migrate(KConfigGroup &group, const XmlGuiShortcuts &found);
}
