// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QString>

/**
 * The built-in editor's Markdown and vault settings: the vault folder that
 * wiki links, backlinks and tags use, and how Markdown notes are edited,
 * previewed and navigated. A workspace can keep its own options.
 */
namespace VaultSettings
{
enum class Dialect {
    GitHub,
    CommonMark,
};

struct Options {
    /// Whether Markdown files open in visual mode rather than source mode.
    bool visual = false;
    /// Whether the Markdown preview shows when a Markdown file opens.
    bool preview = false;
    Dialect dialect = Dialect::GitHub;
    /// Whether wiki links in the preview and the Links list open the notes they name.
    bool wikiLinks = true;
    /// Whether the vault panel lists backlinks, the note's links and its tags.
    bool backlinks = true;
    bool links = true;
    bool tags = true;

    bool operator==(const Options &other) const = default;
};

/// The options from the Editor settings.
Options general();

/**
 * The vault folder from the EditorVaultFolder setting, as a clean absolute
 * path, or empty when no vault is chosen. A vault chosen before the setting
 * existed, which the state configuration kept, becomes the setting once.
 */
QString vaultFolder();
/// Stores @p root as the EditorVaultFolder setting and saves the settings; empty closes the vault.
void setVaultFolder(const QString &root);
/// Why the vault folder @p root can't be used, or an empty string when it can or when @p root is empty.
QString unavailableReason(const QString &root);

/// @p options as JSON, for a workspace's own options.
QJsonObject toJson(const Options &options);
/// Options read from @p json; missing or invalid values keep those of @p fallback.
Options fromJson(const QJsonObject &json, const Options &fallback = {});
}
