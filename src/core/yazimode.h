// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeyCombination>
#include <QKeySequence>
#include <QList>
#include <QStringList>
#include <QUrl>

#include <optional>

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

/// How a pane chooses its mode when a window starts. The values match the
/// YaziLeftPaneDefault and YaziLeftPaneRestore settings and their second-pane
/// counterparts.
struct Activation {
    /// The mode the pane starts with when it has no previous choice to restore.
    bool defaultMode = false;
    /// Whether the pane restores the mode it had when its session was saved.
    bool restore = true;
    bool operator==(const Activation &other) const = default;
};

/**
 * The mode a pane starts with when a window opens. Explicitly switching to or
 * restoring a workspace layout doesn't use this; the layout's behaviors decide.
 * @param activation the pane's default mode and whether it restores its
 *        previous choice
 * @param saved the mode saved in the pane's restored session, or nullopt
 *        when no session is restored for the pane, such as on the first start
 *        or when the window opens the home folder or fixed folders
 * @returns @p saved when restoring and there is one, otherwise the default
 */
bool startupMode(const Activation &activation, std::optional<bool> saved);

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

/// What the preview column shows for the focused item. The values match the YaziPreviewContent setting.
enum class PreviewContent {
    /// The item's preview, or a folder's contents.
    Preview = 0,
    /// The item's file information, such as its size, type, dates and permissions.
    Information = 1,
};

/**
 * Reads what a pane's preview column shows from a session group.
 * @returns a PreviewContent value, or -1 when nothing or an unknown value was
 *          saved, as in sessions saved before the choice existed
 */
int readSessionPreviewContent(const KConfigGroup &group);
/// Writes what a pane's preview column shows; -1 removes the entry.
void writeSessionPreviewContent(KConfigGroup &group, int content);
/// Writes what a pane's preview column shows as "preview" or "information"; -1 writes nothing.
void writeWorkspacePreviewContent(QJsonObject &object, int content);
/**
 * Reads what a pane's preview column shows into @p content: a PreviewContent
 * value, or -1 when absent, as in workspaces saved before the choice existed.
 * @returns false when the value is present but is not "preview" or "information"
 */
bool readWorkspacePreviewContent(const QJsonObject &object, int &content);

/**
 * Reads whether a pane shows its parent column from a session group.
 * @returns 1 when shown, 0 when hidden and -1 when nothing was saved, as in
 *          sessions saved before the choice existed
 */
int readSessionParentColumn(const KConfigGroup &group);
/// Writes whether a pane shows its parent column; -1 removes the entry.
void writeSessionParentColumn(KConfigGroup &group, int parent);
/// Writes whether a pane shows its parent column as a boolean; -1 writes nothing.
void writeWorkspaceParentColumn(QJsonObject &object, int parent);
/**
 * Reads whether a pane shows its parent column into @p parent: 1 shown, 0
 * hidden, or -1 when absent, as in workspaces saved before the choice existed.
 * @returns false when the value is present but is not a boolean
 */
bool readWorkspaceParentColumn(const QJsonObject &object, int &parent);

/**
 * The folder the parent column lists for @p current, or an empty URL when
 * @p current is invalid or has no parent, such as at a root. At the root of
 * an archive, such as zip:/home/me/a.zip/, it is the local folder that
 * holds the archive.
 */
QUrl parentFolder(const QUrl &current);
/**
 * The URL that stands for @p current in the listing of parentFolder(): the
 * archive file at the root of an archive, otherwise @p current itself.
 */
QUrl parentItem(const QUrl &current);

/// How well Yazi mode's columns work in a location.
enum class SupportLevel {
    /// Local folders: every column works.
    Full,
    /// Folders that list, but whose parent column or preview is limited, such as remote folders, archives and the trash.
    Limited,
    /// Locations without a meaningful parent folder, such as search results or recent files; the pane browses them as usual.
    Unsupported,
};

struct Support {
    SupportLevel level = SupportLevel::Full;
    /// Why the location is limited or unsupported, as a sentence for the user; empty for Full.
    QString reason;

    bool operator==(const Support &other) const = default;
};

/**
 * How well Yazi mode works in @p url. Local folders are Full. Folders that
 * KIO lists, such as sftp:, smb:, trash:, desktop: and archives, are
 * Limited. Locations whose items come from many folders or aren't files,
 * such as search results, recent files, tags and remote:, are Unsupported,
 * as is an empty or invalid URL.
 */
Support support(const QUrl &url);

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

/// The largest ratio that clampedRatios() keeps. Dragging stores the columns' widths in pixels, which stay well below it.
constexpr int MaximumRatio = 100000;

/// @p ratios with every value at least 1 and at most MaximumRatio; larger values scale all three down, keeping their proportions.
Ratios clampedRatios(const Ratios &ratios);
/// @p ratios as the parent, current and preview values, as the YaziColumnRatios setting stores them.
QList<int> ratiosToList(const Ratios &ratios);
/**
 * The ratios that @p list holds, clamped by clampedRatios().
 * @returns nothing unless @p list has exactly three positive values
 */
std::optional<Ratios> ratiosFromList(const QList<int> &list);

/**
 * Reads a pane's column ratios from a session group.
 * @returns three positive values, or an empty list when nothing or something
 *          invalid was saved; the pane then uses the YaziColumnRatios default
 */
QList<int> readSessionRatios(const KConfigGroup &group);
/// Writes a pane's column ratios; an empty list removes the entry.
void writeSessionRatios(KConfigGroup &group, const QList<int> &ratios);
/// Writes a pane's column ratios as an array of three numbers; an empty list writes nothing.
void writeWorkspaceRatios(QJsonObject &object, const QList<int> &ratios);
/**
 * Reads a pane's column ratios into @p ratios, or an empty list when absent,
 * as in workspaces saved before the ratios were stored.
 * @returns false when the value is present but is not an array of three
 *          positive integers
 */
bool readWorkspaceRatios(const QJsonObject &object, QList<int> &ratios);

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
 * @param parent whether the parent column is wanted at all; without it, the
 *        preview column needs only room for itself and the current column
 * @returns the widths; together with the handles they add up to @p available
 */
ColumnWidths columnWidths(int available, bool preview, const Ratios &ratios = {}, int handleWidth = 0, bool parent = true);

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
    /// Switches the preview column between the preview and the file information.
    TogglePreviewContent,
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
    /// Opens the pane's filter bar and focuses it.
    Filter,
    /// Clears the pane's filter and closes the filter bar.
    ClearFilter,
    /// Opens the search panel for a file name search in the current folder.
    SearchNames,
    /// Opens the search panel for a file contents search in the current folder.
    SearchContents,
    SortByName,
    SortBySize,
    SortByModified,
    SortByType,
    /// Reverses the sort order, keeping the sort column.
    ReverseSort,
    /// Opens a tab at the focused folder, or at the current folder when the focused item isn't a folder.
    NewTab,
    PreviousTab,
    NextTab,
    /// Closes the current tab unless it is the only, a pinned or a locked tab.
    CloseTab,
};

/// The last command, for iterating over every command.
constexpr Command LastCommand = Command::CloseTab;

/// A stable identifier for @p command, such as "move-down", for settings and documentation.
QString commandId(Command command);
/// A short description of @p command for the user, such as "Move down".
QString commandText(Command command);
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
    /// It also binds Yazi's selection keys, file actions, filter, search, sorting and tab keys, but never permanent deletion.
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

/// A window action and its shortcuts, for shadowedShortcuts().
struct ActionShortcuts {
    /// The action's name, such as "open_gallery".
    QString name;
    /// The action's text, without accelerator markers.
    QString text;
    QList<QKeySequence> shortcuts;
};

/// A window shortcut that a keymap takes over while a Yazi mode column has the keyboard focus.
struct ShadowedShortcut {
    QString name;
    QString text;
    QKeySequence shortcut;
    /// The bindings whose first key is the shortcut's first key, in binding order; empty when that key starts a count.
    QList<int> bindings;

    bool operator==(const ShadowedShortcut &other) const = default;
};

/**
 * The shortcuts of @p actions that @p keymap shadows, in action and shortcut
 * order. While no sequence is pending, Yazi mode accepts the shortcut
 * override of every key that begins a binding or a count, as
 * KeyMatcher::test() tells, so a shortcut whose first key is such a key
 * doesn't fire while a Yazi mode column has the keyboard focus. Keys that
 * only continue a sequence, and Escape, which cancels one, are taken only
 * while a sequence is pending, after a key that is listed here.
 */
QList<ShadowedShortcut> shadowedShortcuts(const Keymap &keymap, const QList<ActionShortcuts> &actions);

/**
 * @p key without the modifiers that don't change what the key means here:
 * keypad and group switch, and Shift for punctuation such as / or [, which
 * some keyboard layouts can only type with Shift.
 */
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
