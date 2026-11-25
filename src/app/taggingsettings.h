// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libraryscan.h"
#include "metadataimport.h"
#include "taglibrary.h"
#include "tagsearch.h"

/// Reads the Tagging settings into plain values on the GUI thread, for core functions and worker threads.
namespace TaggingSettings
{
/// Where new libraries keep their document, from Preferences › Tagging › Privacy.
TagLibrary::Storage newLibraryStorage();

/// The fields new library documents start with, from Preferences › Tagging › Fields.
QList<TagLibrary::Field> newLibraryFields();

/// What library refreshes index, from Preferences › Tagging › Libraries.
LibraryScan::Options scanOptions();

/// How tag search terms select tags and items, from Preferences › Tagging › Tags and Search.
TagSearch::Options tagSearchOptions();

/// The lowercase #rrggbb colour for new tags, or empty for none or an invalid setting.
QString defaultTagColor();

/// The embedded metadata properties that are read, from Preferences › Tagging › Embedded Metadata.
QStringList embeddedMetadataKeys();

/// The saved embedded metadata mappings, including those of properties that aren't read.
MetadataImport::NamedMappings embeddedMetadataMappings();

/// Saves @p mappings as the embedded metadata mappings, without writing the configuration file.
void setEmbeddedMetadataMappings(const MetadataImport::NamedMappings &mappings);

/// Whether imported embedded metadata keeps or replaces different field values.
MetadataImport::Policy embeddedMetadataPolicy();

/// Whether library refreshes import embedded metadata: the setting is on and a read property is mapped.
bool importsEmbeddedMetadataOnRefresh();

/**
 * Moves a tag panel placement remembered in the state configuration by
 * earlier versions into the TagPanelPlacement setting, unless the setting
 * was already changed, and removes the state entry.
 */
void migrateTagPanelPlacement();
}
