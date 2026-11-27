// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>

#include <functional>

/**
 * Reports visited folders and opened files to Plasma Activities, which
 * links them to the current Activity for Plasma's recent files and
 * locations. Other applications, such as Plasma Impresario, see them only
 * through Plasma Activities; Plasma Opus never reads or changes their data.
 * What Plasma remembers is chosen in System Settings › Recent Files.
 */
namespace ActivityReporting
{
using Reporter = std::function<void(const QUrl &url)>;

/// Reports @p url as used in the current Activity, when the
/// ReportActivityUsage setting is on and the location may be reported.
void reportUsed(const QUrl &url);

/**
 * Replaces how locations are reported; pass an empty reporter to restore
 * Plasma Activities. Without a replacement, nothing is reported while
 * QStandardPaths test mode is on, so tests never add to the real history.
 */
void setReporter(Reporter reporter);
}
