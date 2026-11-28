// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keybindings.h"

#include <QByteArray>
#include <QDateTime>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>
#include <utility>

class KCoreConfigSkeleton;

/**
 * Keymap files: the window's shortcuts, and the keyboard settings that go
 * with them, exported from Preferences › Keyboard › Shortcuts and imported
 * after a preview. A keymap file is a JSON object:
 *
 * @code
 * {
 *     "format": "plasma-opus-keymap",
 *     "version": 1,
 *     "application": "opus-file-manager",
 *     "exported": "2026-11-28T12:00:00Z",
 *     "actions": { "open_gallery": ["F9", "Ctrl+K, G"], "search_folder": [] },
 *     "settings": { "ExtraNavigationShortcuts": true, "YaziKeyPreset": "Vim" }
 * }
 * @endcode
 *
 * Each action is named as in the Shortcuts group of opus-file-managerrc (see
 * ShortcutStorage), with its shortcuts in Qt's portable text, primary first;
 * an empty list means no shortcut. Only data is read: nothing in a file is
 * run, and only actions the window has and the settings of settingNames()
 * are used.
 */
namespace KeymapFile
{
/// The value of the "format" member.
inline constexpr const char Format[] = "plasma-opus-keymap";
/// The format version this version of Plasma Opus writes and the newest it reads.
inline constexpr int Version = 1;
/// The largest file read, in bytes.
inline constexpr qint64 MaximumFileSize = 256 * 1024;
/// The most actions a file may list.
inline constexpr int MaximumActions = 4096;
/// The most shortcuts an action may have in a file.
inline constexpr int MaximumShortcuts = 16;
/// The longest action name or shortcut text read.
inline constexpr int MaximumTextLength = 256;

/// The value of the "application" member: the application whose actions the file names.
QString application();

/// The names of the keyboard settings a file can carry, as kcfg entry names.
QStringList settingNames();

/// The name of setting @p name for the user.
QString settingText(const QString &name);

/// A keymap read from a file or about to be written to one.
struct Keymap {
    /// Each action's shortcuts by name, in order; an empty list means no shortcut.
    QList<std::pair<QString, QList<QKeySequence>>> actions;
    /// Keyboard settings by kcfg entry name, as KConfigSkeleton items hold them: a bool, or an enum's index.
    QVariantMap settings;

    bool operator==(const Keymap &other) const = default;
};

/**
 * The key sequence of @p text in Qt's portable text, such as "Ctrl+K, G", or
 * nothing when it isn't one to four valid key combinations, each with a key
 * other than a modifier.
 */
std::optional<QKeySequence> parseSequence(const QString &text);

/// Whether @p binding is a window action's, whose shortcuts a keymap file can carry.
bool isExportable(const KeyBindings::Binding &binding);

/// The keymap of @p bindings: each exportable binding whose shortcuts differ from its defaults.
Keymap fromBindings(const QList<KeyBindings::Binding> &bindings);

/// The values of the settingNames() entries of @p settings, as Keymap::settings holds them.
QVariantMap settingValues(const KCoreConfigSkeleton *settings);

/// The JSON of @p keymap, with enum settings named as the choices of @p settings, and @p exported when valid.
QByteArray toJson(const Keymap &keymap, const KCoreConfigSkeleton *settings, const QDateTime &exported = {});

/**
 * Reads a keymap file. Returns nothing and sets @p error when it isn't a
 * valid keymap of this application in a version this one reads: too large,
 * not JSON, a member of the wrong type, an action name that isn't text or a
 * shortcut that isn't a valid key sequence. Unknown members and settings,
 * settings without @p settings and shortcuts listed twice for one action are
 * left out and described in @p warnings.
 */
std::optional<Keymap> fromJson(const QByteArray &json, const KCoreConfigSkeleton *settings, QString *error = nullptr, QStringList *warnings = nullptr);

/// Reads the keymap file at @p path, as fromJson() does; files larger than MaximumFileSize aren't read.
std::optional<Keymap> importFile(const QString &path, const KCoreConfigSkeleton *settings, QString *error = nullptr, QStringList *warnings = nullptr);

/// Writes @p keymap to @p path, replacing the file only once it is complete. Returns an error message, or empty on success.
QString exportFile(const Keymap &keymap, const KCoreConfigSkeleton *settings, const QString &path, const QDateTime &exported = {});

/// A binding whose shortcuts an import changes.
struct Change {
    /// The index of the binding.
    int index = -1;
    /// Its shortcuts after the import.
    QList<QKeySequence> shortcuts;

    bool operator==(const Change &other) const = default;
};

/// What importing a keymap into a window would do.
struct Preview {
    /// The bindings whose shortcuts change, in binding order.
    QList<Change> changes;
    /// The names of the actions the file lists that the window doesn't have, in file order.
    QStringList unknown;
    /// How many listed actions already have the file's shortcuts.
    int unchanged = 0;
    /// The bindings after the import, including extra navigation keys that give way to imported shortcuts.
    QList<KeyBindings::Binding> bindings;
    /// The conflicts the import adds, as KeyBindings::conflicts() finds them in bindings.
    QList<KeyBindings::Conflict> added;
    /// The conflicts that stay as they are.
    QList<KeyBindings::Conflict> kept;
    /// How many of the current conflicts the import resolves.
    int resolved = 0;

    bool operator==(const Preview &other) const = default;
};

/**
 * What importing @p keymap into a window with @p bindings would do: each
 * listed exportable binding gets the file's shortcuts and, when
 * @p resetUnlisted, every other exportable binding its defaults. An extra
 * navigation key that is one of a binding's defaults gives way when another
 * binding takes it, as the window does.
 */
Preview preview(const QList<KeyBindings::Binding> &bindings, const Keymap &keymap, bool resetUnlisted);

/// Describes action @p name, which a file lists and the window doesn't have, for the user.
QString unknownText(const QString &name);
}
