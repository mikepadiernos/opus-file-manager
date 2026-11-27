// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudassets.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>

namespace NextcloudAccounts
{
struct Account;
}

/**
 * Favourites, collaborative (system) tags and server-side search of a
 * Nextcloud server. These are the server's own records: they are never
 * stored with, or mixed into, Plasma Opus's local tags.
 */
namespace Nextcloud
{
/// A collaborative tag from remote.php/dav/systemtags.
struct SystemTag {
    QString id;
    QString name;
    bool userVisible = true;
    bool userAssignable = true;
    /// nc:can-assign, which the server reports for the signed-in user since Nextcloud 11.
    std::optional<bool> canAssign;

    bool operator==(const SystemTag &) const = default;
};

/// Whether the signed-in user may assign @p tag to files and remove it from them.
bool canAssign(const SystemTag &tag);
/// The tag's name, labelled when the user can't see or assign it.
QString describeTag(const SystemTag &tag);

/// The PROPFIND body that asks for the properties of a SystemTag.
QString systemTagsRequest();
/**
 * Parses a PROPFIND response of remote.php/dav/systemtags or of a file's
 * systemtags-relations folder into its tags, sorted by name.
 * @return nullopt, setting @p error, when @p xml isn't a multistatus response
 */
std::optional<QList<SystemTag>> parseSystemTags(const QByteArray &xml, QString *error = nullptr);
/// The JSON body that creates a tag named @p name that users can see and assign.
QByteArray createTagRequest(const QString &name);
/// Why @p name can't be a tag's name, or an empty string.
QString tagNameError(const QString &name);
/// The tag ID at the end of a created tag's Content-Location header among @p headers, one per line.
QString createdTagId(const QString &headers);

/// The PROPPATCH body that marks an item as a favourite or no longer one.
QString favoriteRequest(bool favorite);
/**
 * Whether a PROPPATCH multistatus response confirms the change.
 * @param error receives why the server refused it
 */
bool parsePropPatch(const QByteArray &xml, QString *error = nullptr);

/// The kinds of file a search can be limited to.
enum class AssetType {
    Any,
    Images,
    Videos,
    Audio,
    Documents,
    Folders,
};

/// A server-side search of the user's files.
struct SearchQuery {
    /// Text the item's name must contain; empty matches any name.
    QString text;
    AssetType type = AssetType::Any;
    bool favoritesOnly = false;
    /// The most results to show; the server is asked for one more to detect truncation.
    int limit = 200;
};

/// What a listing of favourites, tagged files or search results returned.
struct AssetResults {
    QList<FileMetadata> items;
    /// Whether the server had more results than the limit.
    bool truncated = false;
    /// Results that were left out or are incomplete because the server withheld their properties or they are outside the user's files.
    int incomplete = 0;
    /// Why the listing failed, or an empty string.
    QString error;
    /// Whether the server doesn't offer the listing at all, as opposed to failing it.
    bool unavailable = false;
};

/// The REPORT body that lists the user's favourites, or the files with the tag @p tagId when it isn't empty.
QString filterFilesRequest(const QString &tagId = QString());
/// The SEARCH body for @p query within the files of @p account.
QString searchRequest(const NextcloudAccounts::Account &account, const SearchQuery &query);
/**
 * Turns a REPORT or SEARCH multistatus response into results for
 * @p account, keeping at most @p limit items in the server's order.
 */
AssetResults parseAssetResults(const QByteArray &xml, const NextcloudAccounts::Account &account, int limit);
/// The URL of the item at the decoded server @p path, or an empty URL when it isn't in the files of @p account.
QUrl itemUrl(const NextcloudAccounts::Account &account, const QString &path);
/// Why a listing failed with @p kioError and @p httpStatus, marking @p unavailable when the server doesn't offer it.
QString listingError(int kioError, int httpStatus, const QString &errorText, bool *unavailable);
}
