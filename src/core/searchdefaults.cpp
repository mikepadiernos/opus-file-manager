// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchdefaults.h"

#include "savedpathfilters.h"

#include <KLocalizedString>

namespace
{
const char ScopeKey[] = "DefaultSearchScope";
const char FieldKey[] = "DefaultSearchField";
const char LegacyIndexedKey[] = "SearchIndexedByDefault";
const char LegacyContentsKey[] = "SearchContentsByDefault";

/// Matches KConfigSkeleton's enum storage: a choice name, or its index.
int choice(const KConfigGroup &group, const char *key, const QStringList &names, int fallback)
{
    const QString value = group.readEntry(key, QString()).trimmed();
    for (int i = 0; i < names.size(); ++i) {
        if (value.compare(names.at(i), Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    bool ok = false;
    const int index = value.toInt(&ok);
    return ok && index >= 0 && index < names.size() ? index : fallback;
}

const QStringList &scopeNames()
{
    static const QStringList names{QStringLiteral("CurrentFolder"), QStringLiteral("Indexed"), QStringLiteral("SearchFolders")};
    return names;
}

const QStringList &fieldNames()
{
    static const QStringList names{QStringLiteral("FileName"), QStringLiteral("Contents"), QStringLiteral("Query")};
    return names;
}

const QStringList &presentationNames()
{
    static const QStringList names{QStringLiteral("FullPaths"), QStringLiteral("NamesAndFolders")};
    return names;
}
}

namespace SearchDefaults
{
Defaults read(const KConfigGroup &search)
{
    Defaults defaults;
    const int legacyScope = search.readEntry(LegacyIndexedKey, false) ? int(Scope::Indexed) : int(Scope::CurrentFolder);
    defaults.scope = Scope(choice(search, ScopeKey, scopeNames(), legacyScope));
    const int legacyField = search.readEntry(LegacyContentsKey, false) ? int(LiveSearch::Field::Contents) : int(LiveSearch::Field::FileName);
    defaults.field = LiveSearch::Field(choice(search, FieldKey, fieldNames(), legacyField));
    defaults.workspaceOnly = search.readEntry("WorkspaceSearchByDefault", true);
    defaults.pathFilter = search.readEntry("DefaultSearchPathFilter", QString());
    defaults.presentation = Presentation(choice(search, "DefaultSearchResultPresentation", presentationNames(), int(Presentation::FullPaths)));
    return defaults;
}

bool migrate(KConfigGroup &search)
{
    if (!search.hasKey(LegacyIndexedKey) && !search.hasKey(LegacyContentsKey)) {
        return false;
    }
    const Defaults defaults = read(search);
    if (!search.hasKey(ScopeKey) && defaults.scope != Scope::CurrentFolder) {
        search.writeEntry(ScopeKey, scopeNames().at(int(defaults.scope)));
    }
    if (!search.hasKey(FieldKey) && defaults.field != LiveSearch::Field::FileName) {
        search.writeEntry(FieldKey, fieldNames().at(int(defaults.field)));
    }
    search.deleteEntry(LegacyIndexedKey);
    search.deleteEntry(LegacyContentsKey);
    return true;
}

PathFilter pathFilter(const QString &name, const KConfigGroup &savedFilters, QString *notice)
{
    if (name.isEmpty()) {
        return {};
    }
    QString error;
    const auto presets = SavedPathFilters::load(savedFilters, &error);
    if (!error.isEmpty()) {
        if (notice) {
            *notice = error;
        }
        return {};
    }
    for (const auto &preset : presets) {
        if (preset.name == name) {
            return preset.filter;
        }
    }
    if (notice) {
        *notice = i18nc("@info", "The default path filter “%1” no longer exists. Choose another on the Search page of Preferences.", name);
    }
    return {};
}
}
