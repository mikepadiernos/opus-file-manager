// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class KConfigGroup;

/**
 * The command palette: KDE's command bar listing the window's menu, toolbar
 * and saved-command actions by name, with their shortcuts and the recently
 * used ones first. The window collects the candidates; this decides which
 * are listed, how they are grouped and where KDE's command bar remembers the
 * recently used ones. Nothing here runs an action.
 */
namespace CommandPalette
{
/// The name of the action that opens the palette, which KDE's main window creates.
inline constexpr char ActionName[] = "open_kcommand_bar";

/// Where KDE's command bar stores the recently used actions in the state configuration.
inline constexpr char HistoryGroup[] = "General";
inline constexpr char HistoryKey[] = "CommandBarLastUsedActions";
/// How many recently used actions KDE's command bar remembers.
inline constexpr int HistorySize = 6;

/// Whether the action named @p name runs a saved command.
bool isSavedCommand(const QString &name);

/// An action the palette could list.
struct Candidate {
    /// The action's name, such as "copy", or empty for menu actions without one.
    QString name;
    /// The action's text, with any accelerator markers.
    QString text;
    /// The titles of the menus that hold the action, outermost first, or empty when it isn't in a menu.
    QStringList menuPath;
    /// The title of the toolbar that holds the action, when it isn't in a menu.
    QString toolBar;
    bool enabled = true;
    bool visible = true;

    bool operator==(const Candidate &other) const = default;
};

struct Options {
    /// Whether saved commands are listed; they open their preview rather than run.
    bool savedCommands = true;
    /// The name of the action that opens the palette, which isn't listed.
    QString paletteAction = QString::fromLatin1(ActionName);
};

/**
 * Whether @p candidate is listed: it is enabled, visible and has a text, it
 * isn't the action that opens the palette, and it isn't a saved command
 * while @p options leave them out.
 */
bool isListed(const Candidate &candidate, const Options &options = {});

/**
 * The group the palette shows before @p candidate's text: its menu path
 * without accelerator markers, joined by " › ", or else its toolbar's title,
 * or empty for saved commands, whose text already says what they are, and
 * for actions found in neither.
 */
QString groupName(const Candidate &candidate);

/// The indexes of the listed candidates of one group.
struct Group {
    QString name;
    QList<int> indexes;

    bool operator==(const Group &other) const = default;
};

/// The listed candidates of @p candidates by group, in order of first appearance.
QList<Group> groups(const QList<Candidate> &candidates, const Options &options = {});

/// The texts of the recently used actions in @p state, most recent first, as KDE's command bar stores them.
QStringList history(const KConfigGroup &state);

/// @p history without accelerator markers or duplicates, for the user.
QStringList historyText(const QStringList &history);

/// Forgets the recently used actions in @p state.
void clearHistory(KConfigGroup &state);
}
