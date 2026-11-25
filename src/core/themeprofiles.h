// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

class KCoreConfigSkeleton;
class KConfigSkeletonItem;

/**
 * Named, versioned sets of appearance options from the Themes settings
 * group. Profiles are plain data: importing one only validates and stores
 * setting values, it never runs code or reads other files.
 */
namespace ThemeProfiles
{
/// The JSON "format" value of an exported theme profile.
inline constexpr auto Format = "plasma-opus-theme";
/// The newest profile format version that can be read and the one written.
inline constexpr int Version = 1;
/// Larger files are rejected before they are parsed.
inline constexpr qint64 MaximumFileSize = 512 * 1024;
inline constexpr int MaximumProfiles = 100;
inline constexpr int MaximumNameLength = 100;
/// The settings group that stores saved profiles.
inline constexpr auto Group = "ThemeProfiles";

struct Profile {
    QString id;
    QString name;
    /// Setting values by entry name, in the types the settings use.
    QVariantMap values;
};

/**
 * The Themes entries a profile holds: every appearance option except
 * folder colour rules, which are user-made lists, and the high contrast
 * safeguard, which stays a personal accessibility choice.
 */
QList<KConfigSkeletonItem *> items(const KCoreConfigSkeleton *settings);
QStringList entries(const KCoreConfigSkeleton *settings);

/// The current values of entries() in @p settings.
QVariantMap capture(const KCoreConfigSkeleton *settings);

/// The default values of entries(), which follow Plasma.
QVariantMap defaults(const KCoreConfigSkeleton *settings);

/**
 * Checks a profile name: not empty after trimming, at most
 * MaximumNameLength characters and without control characters.
 * @returns an error message, or an empty string when the name is valid
 */
QString nameError(const QString &name);

/// Writes @p profile as an exportable JSON document.
QByteArray toJson(const KCoreConfigSkeleton *settings, const Profile &profile);

/**
 * Reads an exported profile. Entries missing from the file use their
 * defaults; unknown entries are skipped with a warning.
 * @param error set to the reason when the file is rejected
 * @param warnings receives messages about skipped entries
 * @returns the profile without an id, or std::nullopt when it is rejected
 */
std::optional<Profile> fromJson(const KCoreConfigSkeleton *settings, const QByteArray &json, QString *error, QStringList *warnings = nullptr);

/// Reads an exported profile file, refusing files over MaximumFileSize.
std::optional<Profile> importFile(const KCoreConfigSkeleton *settings, const QString &path, QString *error, QStringList *warnings = nullptr);

/// Writes @p profile to @p path. @returns an error message, or an empty string
QString exportFile(const KCoreConfigSkeleton *settings, const Profile &profile, const QString &path);

/**
 * Lists the colour schemes and font families that @p values refer to but
 * that are not installed. The values are kept, so they apply again once the
 * scheme or font is installed; until then Plasma's own is used.
 */
QStringList unavailable(const QVariantMap &values);

/**
 * Reads saved profiles. Invalid profiles are skipped and reported in
 * @p error.
 */
QList<Profile> load(const KCoreConfigSkeleton *settings, const KConfigGroup &group, QString *error = nullptr);

/**
 * Saves @p profiles, assigning ids to profiles without one.
 * @returns false with @p error set when a name is invalid or repeated, or
 *          there are more than MaximumProfiles
 */
bool save(const KCoreConfigSkeleton *settings, KConfigGroup group, QList<Profile> &profiles, QString *error = nullptr);

/// The index of the profile named @p name, ignoring case, or -1.
qsizetype indexOfName(const QList<Profile> &profiles, const QString &name);
}
