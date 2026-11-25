// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>

class KConfigGroup;
class QJsonObject;

/**
 * Non-widget helpers for Yazi mode, the optional per-pane column layout that
 * shows a parent-folder column before the pane's folder view and an optional
 * preview column after it.
 */
namespace YaziMode
{
/// Reads a pane's mode from a session group; absent means off.
bool readSession(const KConfigGroup &group);
void writeSession(KConfigGroup &group, bool enabled);

/// Writes a pane's mode into a workspace pane-session object.
void writeWorkspace(QJsonObject &object, bool enabled);
/// Reads a pane's mode from a workspace pane-session object into @p enabled.
/// An absent key, as in workspaces saved before Yazi mode existed, means off.
/// Returns false when the value is present but is not a boolean.
bool readWorkspace(const QJsonObject &object, bool &enabled);

/// The folder the parent column lists for @p current, or an empty URL when
/// @p current is invalid or has no parent, such as at a root.
QUrl parentFolder(const QUrl &current);
}
