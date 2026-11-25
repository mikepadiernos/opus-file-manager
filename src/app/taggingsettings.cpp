// SPDX-License-Identifier: GPL-3.0-or-later

#include "taggingsettings.h"

#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>

TagLibrary::Storage TaggingSettings::newLibraryStorage()
{
    return Settings::newLibraryStorage() == Settings::EnumNewLibraryStorage::PrivateFolder ? TagLibrary::Storage::PrivateFolder
                                                                                           : TagLibrary::Storage::LibraryFolder;
}

LibraryScan::Options TaggingSettings::scanOptions()
{
    LibraryScan::Options options;
    options.exclusions = Settings::libraryExclusions();
    options.hidden = Settings::indexHiddenItems();
    options.limit = Settings::libraryScanLimit();
    options.depth = Settings::libraryScanDepth();
    return options;
}

TagSearch::Options TaggingSettings::tagSearchOptions()
{
    TagSearch::Options options;
    options.childTags = Settings::tagMatchesChildTags();
    options.aliases = Settings::tagMatchAliases();
    options.untaggedOutsideLibraries = Settings::untaggedIncludesOutsideLibraries();
    options.scope = Settings::tagSearchScope() == Settings::EnumTagSearchScope::LibraryRoot ? TagSearch::Scope::LibraryRoot : TagSearch::Scope::CurrentFolder;
    return options;
}

QString TaggingSettings::defaultTagColor()
{
    const QString color = Settings::defaultTagColor().trimmed();
    return TagLibrary::isValidColor(color) ? color.toLower() : QString();
}

void TaggingSettings::migrateTagPanelPlacement()
{
    KConfigGroup state = KSharedConfig::openStateConfig()->group(QStringLiteral("TagPanel"));
    if (!state.hasKey("Placement")) {
        return;
    }
    if (state.readEntry("Placement") == QLatin1String("below") && Settings::tagPanelPlacement() == Settings::EnumTagPanelPlacement::Right) {
        Settings::setTagPanelPlacement(Settings::EnumTagPanelPlacement::Below);
        Settings::self()->save();
    }
    state.deleteEntry("Placement");
    state.sync();
}
