// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QUrl>

/// Browsing archive files as folders through KIO's archive workers.
namespace Archives
{
enum class TestResult {
    Passed,
    PasswordRequired,
    Failed,
};

/**
 * The URL that lists an archive's contents, such as zip:/home/me/a.zip/.
 * @param file the archive file
 * @param mimeType the file's MIME type
 * @returns an empty URL when no KIO worker handles the MIME type or the
 *          file is not local
 */
QUrl browseUrl(const QUrl &file, const QString &mimeType);

/**
 * The archive's file name without its archive extension, used to name the
 * folder it is extracted to.
 * @param fileName such as photos.tar.gz
 * @returns such as photos; @p fileName when it has no known extension
 */
QString baseName(const QString &fileName);

/**
 * Reads every file in a local archive to check that the archive can be
 * decompressed. K7Zip archives may prompt for a password and are retried
 * when @p password is supplied.
 */
TestResult testIntegrity(const QString &fileName, const QString &password, QString *error = nullptr);
}
