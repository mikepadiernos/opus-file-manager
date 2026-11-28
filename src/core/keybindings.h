// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeyCombination>
#include <QKeySequence>
#include <QList>
#include <QString>

/**
 * The window's keyboard shortcuts as Preferences › Keyboard summarizes them:
 * which actions have shortcuts other than their defaults, and which
 * shortcuts conflict. KDE's Configure Keyboard Shortcuts dialog edits them.
 */
namespace KeyBindings
{
/// Where a binding applies, or where the keyboard focus is.
enum class Context {
    /// Anywhere in the main window, as its menu and toolbar actions.
    Application,
    /// A pane's file view, including Yazi mode columns, and the window's other lists.
    FileView,
    /// The folder tree.
    FolderTree,
    /// The image gallery, a window of its own.
    Viewer,
    /// A text field, spin box or combo box of the main window, including inline rename editors.
    TextInput,
};

/// Whether a key pressed with the focus in @p a can reach a binding of @p b, or the other way round.
bool overlaps(Context a, Context b);

/// The name of @p context for the user.
QString contextText(Context context);

/// Whether @p key types or edits text in a text field, such as a letter, Backspace, an arrow or Ctrl+C, or moves the focus from it, as Tab does.
bool isTypingKey(QKeyCombination key);

/// Whether @p key moves through the items of a list or tree: an arrow, Home, End, Page Up or Page Down, alone or with Shift.
bool isNavigationKey(QKeyCombination key);

/// Whether @p key moves around without a mouse: Tab and Shift+Tab move the focus, Escape closes, F10 and Shift+F10 or Menu open menus.
bool isAccessibilityKey(QKeyCombination key);

/**
 * Whether widgets with the focus in @p context keep @p key for themselves,
 * so a window shortcut beginning with it doesn't run there: text fields keep
 * their typing keys and lists and the folder tree their navigation keys.
 */
bool isReserved(QKeyCombination key, Context context);

/// An action's shortcuts and the ones it has by default.
struct Binding {
    /// The action's name, such as "search_folder".
    QString name;
    /// The action's text, without accelerator markers.
    QString text;
    /// The primary shortcut first, then any alternates; empty sequences are ignored.
    QList<QKeySequence> shortcuts;
    QList<QKeySequence> defaults;
    /// Whether the shortcuts are global ones, which KDE's global shortcut service reserves in every application.
    bool global = false;
    /// Where the shortcuts apply; global shortcuts apply everywhere.
    Context context = Context::Application;

    bool operator==(const Binding &other) const = default;
};

/// The name of where @p binding applies, for the user.
QString contextText(const Binding &binding);

/// @p shortcuts without empty sequences, in order.
QList<QKeySequence> keys(const QList<QKeySequence> &shortcuts);

/// Whether @p binding's shortcuts differ from its defaults, including their order.
bool isCustomized(const Binding &binding);

/// The indexes of the customized @p bindings, in order.
QList<int> customized(const QList<Binding> &bindings);

/// Whether @p sequence begins with @p prefix and is longer than it.
bool startsWith(const QKeySequence &sequence, const QKeySequence &prefix);

enum class ConflictKind {
    /// Both bindings have the same shortcut.
    Same,
    /// The first binding's shortcut begins the second's longer key sequence, so the longer one can't be typed.
    Prefix,
    /**
     * The first binding's customized shortcut begins with a key that widgets
     * of the conflict's context keep, or with an accessibility key; there is
     * no second binding.
     */
    Reserved,
};

/// Two shortcuts that get in each other's way.
struct Conflict {
    /// Index of the binding with the shorter, or equal, shortcut.
    int first = -1;
    QKeySequence firstKeys;
    int second = -1;
    QKeySequence secondKeys;
    ConflictKind kind = ConflictKind::Same;
    /// For a reserved key, the context whose widgets keep it, or Application for an accessibility key.
    Context context = Context::Application;

    bool operator==(const Conflict &other) const = default;
};

/**
 * The conflicts between @p bindings, in binding order: two bindings with the
 * same shortcut, and a shortcut that begins another binding's, or the same
 * binding's, longer key sequence, when their contexts overlap or either is
 * global. A shortcut listed twice for one binding is not a conflict. Then,
 * for each customized shortcut of a window binding that isn't one of its
 * defaults, each context that keeps its first key and whether it is an
 * accessibility key.
 */
QList<Conflict> conflicts(const QList<Binding> &bindings);

/// Describes @p conflict between @p bindings as a sentence for the user.
QString conflictText(const Conflict &conflict, const QList<Binding> &bindings);

/// Whether global shortcuts can work in this session.
enum class GlobalAvailability {
    /// KDE's global shortcut service runs, or D-Bus can start it.
    Available,
    /// KDE's global shortcut service isn't running and can't be started.
    NoService,
    /**
     * The service is there, but this is a Wayland session of another desktop:
     * only the compositor sees keys pressed in other windows, so whether a
     * global shortcut works depends on that desktop.
     */
    OtherDesktop,
};

/**
 * Whether global shortcuts can work, given whether KDE's global shortcut
 * service is available, the session type (XDG_SESSION_TYPE) and the
 * desktops of the session (XDG_CURRENT_DESKTOP, separated by colons).
 */
GlobalAvailability globalAvailability(bool serviceAvailable, const QString &sessionType, const QString &currentDesktops);

/// Explains @p availability to the user, or nothing when global shortcuts are available.
QString globalAvailabilityText(GlobalAvailability availability);

/// The keys of @p requested that KDE's global shortcut service didn't give the action, which it does when another application has them.
QList<QKeySequence> refusedKeys(const QList<QKeySequence> &requested, const QList<QKeySequence> &accepted);

/// Tells the user KDE didn't accept the @p refused keys of a global shortcut, or nothing when there are none.
QString refusedText(const QList<QKeySequence> &refused);
}
