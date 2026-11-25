// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeyCombination>
#include <QList>
#include <QStringList>
#include <QUrl>

class KConfigGroup;
class QJsonObject;

/**
 * Non-widget helpers for Yazi mode, the optional per-pane column layout that
 * shows a parent-folder column before the pane's folder view and an optional
 * preview column after it.
 */
namespace YaziMode
{
/// Reads a pane's mode from a session group; absent means off.
bool readSession(const KConfigGroup &group);
void writeSession(KConfigGroup &group, bool enabled);

/**
 * Reads whether a pane shows its preview column from a session group.
 * @returns 1 when shown, 0 when hidden and -1 when nothing was saved, as in
 *          sessions saved before the preview column existed
 */
int readSessionPreview(const KConfigGroup &group);
/// Writes a pane's preview visibility; -1 removes the entry.
void writeSessionPreview(KConfigGroup &group, int preview);

/// Writes a pane's mode into a workspace pane-session object.
void writeWorkspace(QJsonObject &object, bool enabled);
/// Reads a pane's mode from a workspace pane-session object into @p enabled.
/// An absent key, as in workspaces saved before Yazi mode existed, means off.
/// Returns false when the value is present but is not a boolean.
bool readWorkspace(const QJsonObject &object, bool &enabled);

/// Writes a pane's preview visibility as a boolean; -1 writes nothing.
void writeWorkspacePreview(QJsonObject &object, int preview);
/**
 * Reads a pane's preview visibility into @p preview: 1 shown, 0 hidden, or
 * -1 when absent, as in workspaces saved before the preview column existed.
 * @returns false when the value is present but is not a boolean
 */
bool readWorkspacePreview(const QJsonObject &object, int &preview);

/// The folder the parent column lists for @p current, or an empty URL when
/// @p current is invalid or has no parent, such as at a root.
QUrl parentFolder(const QUrl &current);

/// The narrowest current column that columnWidths() allows while it shows another column.
constexpr int MinimumCurrentWidth = 220;
/// The narrowest parent column; narrower panes hide it.
constexpr int MinimumParentWidth = 100;
/// The narrowest preview column; narrower panes hide it before the parent column.
constexpr int MinimumPreviewWidth = 160;
/// Without handles, the narrowest width that shows the parent and current columns.
constexpr int TwoColumnWidth = MinimumParentWidth + MinimumCurrentWidth;
/// Without handles, the narrowest width that shows all three columns.
constexpr int ThreeColumnWidth = TwoColumnWidth + MinimumPreviewWidth;

/// The relative widths of the parent, current and preview columns. Values below 1 count as 1.
struct Ratios {
    int parent = 1;
    int current = 3;
    int preview = 2;

    bool operator==(const Ratios &other) const = default;
};

/// Column widths in pixels; 0 means the column is hidden.
struct ColumnWidths {
    int parent = 0;
    int current = 0;
    int preview = 0;

    bool operator==(const ColumnWidths &other) const = default;
};

/**
 * Shares @p available pixels between the columns in proportion to @p ratios,
 * keeping each shown column at least as wide as its minimum. As the width
 * shrinks, the preview column is hidden first, below ThreeColumnWidth, and then
 * the parent column, below TwoColumnWidth, so the current column never gets
 * narrower than MinimumCurrentWidth unless it is the only column left.
 * @param available the width to share, including the handles between columns
 * @param preview whether the preview column is wanted at all
 * @param ratios the relative widths of the columns
 * @param handleWidth the width of each handle between two shown columns
 * @returns the widths; together with the handles they add up to @p available
 */
ColumnWidths columnWidths(int available, bool preview, const Ratios &ratios = {}, int handleWidth = 0);

/**
 * What a key or key sequence does in Yazi mode. To add a command, add it
 * here, give it an identifier in commandId(), bind it in presetKeymap() and
 * run it in Pane::runYaziCommand(). File commands run the window's existing
 * actions; see commandAction().
 */
enum class Command {
    MoveUp,
    MoveDown,
    MoveFirst,
    MoveLast,
    PageUp,
    PageDown,
    HalfPageUp,
    HalfPageDown,
    /// Opens the parent folder and focuses the folder just left.
    Parent,
    /// Opens the focused item as activating it would.
    Open,
    Back,
    Forward,
    Home,
    FocusParent,
    FocusCurrent,
    FocusPreview,
    TogglePreview,
    Leave,
    /// Selects or deselects the focused item, keeping the selection while moving.
    ToggleSelection,
    /// Selects or deselects the focused item and moves down, as Space does in Yazi.
    ToggleSelectionDown,
    /// Starts or ends range selection, which selects every item between where it started and the focused item.
    VisualSelection,
    SelectAll,
    /// Copies the selected or focused items to the clipboard.
    Copy,
    /// Cuts the selected or focused items to the clipboard.
    Cut,
    /// Pastes the clipboard into the current folder.
    Paste,
    Rename,
    /// Moves the selected or focused items to the trash, never deleting them permanently.
    Trash,
    /// Offers to create a folder or file in the current folder.
    Create,
    OpenWith,
    CopyToOtherPane,
    MoveToOtherPane,
    Undo,
    ToggleHidden,
};

/// The last command, for iterating over every command.
constexpr Command LastCommand = Command::ToggleHidden;

/// A stable identifier for @p command, such as "move-down", for settings and documentation.
QString commandId(Command command);
/// Whether a count typed first, as in 5j, repeats @p command.
bool commandTakesCount(Command command);
/// Whether @p command changes the selection of the current column.
bool isSelectionCommand(Command command);
/// Whether @p command is a file action that the window runs through its own action.
bool isFileCommand(Command command);
/**
 * Whether @p command acts on the selected items. When nothing is selected,
 * the focused item is selected first, so the command acts on it.
 */
bool commandUsesSelection(Command command);
/**
 * The name of the window action that runs the file command @p command, such
 * as "move_to_trash", or empty when the command isn't a file command or is
 * run without a window action. Running the window's actions keeps their
 * confirmations, conflict dialogs, transfer queue and undo.
 */
QString commandAction(Command command);
/**
 * The names of the window actions that delete files permanently. No command
 * may run them: Yazi mode only moves files to the trash.
 */
QStringList permanentDeletionActions();

using KeySequence = QList<QKeyCombination>;

struct Binding {
    KeySequence keys;
    Command command = Command::MoveDown;

    bool operator==(const Binding &other) const = default;
};

/// The bindings of a key preset.
struct Keymap {
    QList<Binding> bindings;
    /// Whether digits typed first, as in 5j, count how often to repeat the command.
    bool counts = false;
};

/// The built-in key presets; the values match the YaziKeyPreset setting.
enum class Preset {
    /// Arrow keys, Home, End, Page Up and Page Down; Left opens the parent folder and Right or Return the focused item.
    Standard,
    /// The standard keys, plus Vim-style letters, multi-key sequences such as gg and counts such as 5j.
    /// It also binds Yazi's selection keys and file actions, but never permanent deletion.
    Vim,
};

Keymap presetKeymap(Preset preset);

/// The longest count the matcher accepts; more digits keep this value.
constexpr int MaximumCount = 999;

/**
 * Two bindings of a keymap that can't both work: one's keys are, or begin
 * with, the other's. When second is -1, the binding at first is unusable on
 * its own: it is empty, uses Escape or a modifier key alone, or starts with
 * a digit that the keymap reads as a count.
 */
struct Conflict {
    int first = -1;
    int second = -1;

    bool operator==(const Conflict &other) const = default;
};

/// The conflicts in @p keymap, in binding order; empty when every binding can be typed.
QList<Conflict> conflicts(const Keymap &keymap);

/// @p key without the keypad and group-switch modifiers, which don't change what the key means here.
QKeyCombination normalizedKey(QKeyCombination key);
/// How to show @p key to the user: "g" and "G" for letters, as typed, otherwise "Ctrl+D" or "PgDown".
QString keyText(QKeyCombination key);
/// How to show @p keys after an optional @p count: "5gg", or "2 Ctrl+D" when a key has a longer name.
QString sequenceText(const KeySequence &keys, int count = 0);

/**
 * Matches typed keys against a keymap, one key at a time. A key that
 * begins or continues a longer binding leaves the matcher pending until
 * the binding completes, Escape cancels it or a key that doesn't fit
 * arrives. The matcher has no timer: callers that want pending sequences
 * to expire call reset().
 *
 * When a pending sequence is broken by a key that doesn't fit, the typed
 * keys are dropped and that key is matched on its own, so g followed by j
 * runs j's command. Modifier keys pressed alone are ignored.
 */
class KeyMatcher
{
public:
    enum class Result {
        /// The key doesn't belong to any binding; the caller should let it through.
        NoMatch,
        /// The key began or continued a binding; more keys are needed.
        Pending,
        /// The key completed a binding; see command() and count().
        Matched,
        /// Escape cancelled a pending sequence or count.
        Cancelled,
    };

    KeyMatcher() = default;
    explicit KeyMatcher(const Keymap &keymap);

    /// Replaces the keymap and cancels a pending sequence.
    void setKeymap(const Keymap &keymap);
    const Keymap &keymap() const;

    Result feed(QKeyCombination key);
    /// What feed() would return for @p key, without changing the state.
    Result test(QKeyCombination key) const;
    /// The command of the binding the last Matched result completed.
    Command command() const;
    /// The count typed before that binding, or 1.
    int count() const;

    bool isPending() const;
    KeySequence pendingKeys() const;
    /// The count typed so far, or 0.
    int pendingCount() const;
    /// The pending count and keys as sequenceText() shows them, or empty.
    QString pendingText() const;
    void reset();

private:
    Result advance(QKeyCombination key);

    Keymap m_keymap;
    KeySequence m_pending;
    int m_pendingCount = 0;
    Command m_command = Command::MoveDown;
    int m_count = 1;
};
}
