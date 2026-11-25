// SPDX-License-Identifier: GPL-3.0-or-later

#include "taggingsettings.h"

#include "settings.h"

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
