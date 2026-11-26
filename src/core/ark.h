// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

/// What KDE's Ark archiver can do with the archive backends installed, and
/// the command lines that hand archives to it.
namespace Ark
{
/// What the installed Ark backends support for one archive MIME type.
struct FormatSupport {
    bool read = false;
    /// Whether Ark can create the format and add, rename or remove entries.
    bool write = false;
    bool encryption = false;
    /// Whether file names can be encrypted as well as contents, as in 7z.
    bool headerEncryption = false;
    /// Whether an archive can be split into volumes.
    bool multiVolume = false;
    bool testing = false;
    /// The plugin IDs that read the format, such as kerfuffle_cli7z.
    QStringList plugins;
};

/// The formats the installed Ark backends read and write.
class Backends
{
public:
    /**
     * Reads Ark's kerfuffle plugin metadata. A plugin reads its formats when
     * every read-only executable it needs is found, and writes them when it
     * is marked read-write and every read-write executable is found, as Ark
     * itself decides.
     * @param plugins each plugin's JSON metadata
     * @param hasExecutable whether a program is installed and in PATH
     */
    static Backends fromPlugins(const QList<QJsonObject> &plugins, const std::function<bool(const QString &)> &hasExecutable);

    /// The installed Ark plugins, checked against PATH.
    static Backends installed();

    /// The support for @p mimeType, resolving MIME aliases and subclasses.
    FormatSupport support(const QString &mimeType) const;
    bool canRead(const QString &mimeType) const;
    bool canWrite(const QString &mimeType) const;

    /// MIME types Ark reads, sorted.
    QStringList readableMimeTypes() const;
    /// MIME types Ark can only extract with the installed tools, such as RAR
    /// without the rar program, sorted.
    QStringList readOnlyMimeTypes() const;
    /// MIME types Ark creates, sorted.
    QStringList writableMimeTypes() const;

    bool isEmpty() const;

private:
    QHash<QString, FormatSupport> m_formats;
};

/// Whether the Ark application is installed.
bool isInstalled();

/// The arguments that open @p archive in Ark's main window for browsing and
/// editing.
QStringList openArguments(const QString &archive);

/**
 * The arguments that extract an archive without Ark's main window.
 * @param archive a local archive path
 * @param destination a new, empty local folder to extract into, so nothing
 *        existing is overwritten
 */
QStringList extractArguments(const QString &archive, const QString &destination);

/**
 * The arguments that open Ark's Compress dialog for new archives, which
 * offers every format, password, file name encryption and volume option the
 * installed backends support. Entries are named relative to the first
 * source's folder.
 */
QStringList compressArguments(const QStringList &sources);

/**
 * The arguments that add @p sources to an existing @p archive in place.
 * Entries are named relative to the first source's folder, so an entry with
 * the same name is replaced.
 */
QStringList addToArguments(const QString &archive, const QStringList &sources);
}
