// SPDX-License-Identifier: GPL-3.0-or-later

#include "commandpalette.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <algorithm>

namespace CommandPalette
{
bool isSavedCommand(const QString &name)
{
    return name.startsWith(QLatin1String("custom_command_preset_"));
}

bool isListed(const Candidate &candidate, const Options &options)
{
    if (!candidate.enabled || !candidate.visible || KLocalizedString::removeAcceleratorMarker(candidate.text).trimmed().isEmpty()) {
        return false;
    }
    if (!candidate.name.isEmpty() && candidate.name == options.paletteAction) {
        return false;
    }
    return options.savedCommands || !isSavedCommand(candidate.name);
}

QString groupName(const Candidate &candidate)
{
    if (isSavedCommand(candidate.name)) {
        return {};
    }
    QStringList titles;
    for (const QString &title : candidate.menuPath) {
        const QString text = KLocalizedString::removeAcceleratorMarker(title).trimmed();
        if (!text.isEmpty()) {
            titles.append(text);
        }
    }
    if (!titles.isEmpty()) {
        return titles.join(QStringLiteral(" › "));
    }
    return KLocalizedString::removeAcceleratorMarker(candidate.toolBar).trimmed();
}

QList<Group> groups(const QList<Candidate> &candidates, const Options &options)
{
    QList<Group> result;
    for (int i = 0; i < candidates.size(); ++i) {
        if (!isListed(candidates.at(i), options)) {
            continue;
        }
        const QString name = groupName(candidates.at(i));
        auto group = std::find_if(result.begin(), result.end(), [&name](const Group &group) {
            return group.name == name;
        });
        if (group == result.end()) {
            result.append({name, {i}});
        } else {
            group->indexes.append(i);
        }
    }
    return result;
}

QStringList history(const KConfigGroup &state)
{
    return state.readEntry(HistoryKey, QStringList());
}

QStringList historyText(const QStringList &history)
{
    QStringList result;
    for (const QString &entry : history) {
        const QString text = KLocalizedString::removeAcceleratorMarker(entry).trimmed();
        if (!text.isEmpty() && !result.contains(text)) {
            result.append(text);
        }
    }
    return result;
}

void clearHistory(KConfigGroup &state)
{
    state.deleteEntry(HistoryKey);
    state.sync();
}
}
