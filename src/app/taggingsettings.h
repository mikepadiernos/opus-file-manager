// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libraryscan.h"
#include "taglibrary.h"

/// Reads the Tagging settings into plain values on the GUI thread, for core functions and worker threads.
namespace TaggingSettings
{
/// Where new libraries keep their document, from Preferences › Tagging › Privacy.
TagLibrary::Storage newLibraryStorage();

/// What library refreshes index, from Preferences › Tagging › Libraries.
LibraryScan::Options scanOptions();
}
