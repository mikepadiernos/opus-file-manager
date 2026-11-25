// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libraryscan.h"
#include "taglibrary.h"
#include "tagsearch.h"

/// Reads the Tagging settings into plain values on the GUI thread, for core functions and worker threads.
namespace TaggingSettings
{
/// Where new libraries keep their document, from Preferences › Tagging › Privacy.
TagLibrary::Storage newLibraryStorage();

/// What library refreshes index, from Preferences › Tagging › Libraries.
LibraryScan::Options scanOptions();

/// How tag: search terms select tags, from Preferences › Tagging › Tags.
TagSearch::Options tagSearchOptions();

/// The lowercase #rrggbb colour for new tags, or empty for none or an invalid setting.
QString defaultTagColor();

/**
 * Moves a tag panel placement remembered in the state configuration by
 * earlier versions into the TagPanelPlacement setting, unless the setting
 * was already changed, and removes the state entry.
 */
void migrateTagPanelPlacement();
}
