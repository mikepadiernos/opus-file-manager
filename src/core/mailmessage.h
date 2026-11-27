// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

/**
 * Reads local mail files (`message/rfc822`, `.eml`) for the preview panel.
 * Messages are untrusted: the parser decodes headers, a plain-text body and
 * the attachment list only. It never loads remote content, runs scripts or
 * decrypts anything; HTML-only bodies are reduced to plain text.
 */
namespace MailMessage
{
/// Files larger than this are not previewed.
constexpr qint64 FileLimit = 64 * 1024 * 1024;

struct Header {
    /// The localizable header name, such as "From", in English.
    QString name;
    QString value;
};

struct Attachment {
    /// A name that is safe to use as a file name in one folder.
    QString fileName;
    QString mimeType;
    /// The decoded size in bytes.
    qint64 size = 0;
};

struct Message {
    /// From, To, Cc, Date and Subject, in that order, when present.
    QList<Header> headers;
    QString subject;
    QString body;
    /// The body was converted from HTML because there was no plain-text part.
    bool bodyFromHtml = false;
    /// The body was longer than the requested limit and was cut.
    bool bodyTruncated = false;
    /// The message, or part of it, is encrypted and was not decrypted.
    bool encrypted = false;
    /// The message is signed; the signature is not verified.
    bool signedMessage = false;
    QList<Attachment> attachments;
};

/**
 * Parses @p data as a mail message. The body is cut to @p bodyLimit
 * characters.
 */
Message parse(const QByteArray &data, qsizetype bodyLimit = 1024 * 1024);

/**
 * Reads and parses the local file @p path.
 * @param error set to why the file could not be read
 */
std::optional<Message> load(const QString &path, qsizetype bodyLimit, QString *error);

/**
 * The decoded content of attachment @p index in @p data, or nothing if the
 * message has no such attachment or its name is not @p expectedName, for
 * example because the file changed after it was previewed.
 */
std::optional<QByteArray> attachmentData(const QByteArray &data, int index, const QString &expectedName);

/// Decodes RFC 2047 encoded words in a header value.
QString decodeHeader(const QByteArray &value);

/// Reduces untrusted HTML to plain text, dropping scripts, styles and markup.
QString htmlToText(const QString &html);

/// Makes @p name safe to create in a folder, or returns @p fallback.
QString safeFileName(const QString &name, const QString &fallback);
}
