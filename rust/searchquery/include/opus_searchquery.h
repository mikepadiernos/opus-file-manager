// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Status codes returned by opus_query_check(). */
#define OPUS_QUERY_VALID 0 /* The output holds the normalized query. */
#define OPUS_QUERY_INVALID 1 /* The output holds an English error message. */
#define OPUS_QUERY_BAD_ENCODING 2 /* The input isn't UTF-8; no output. */
#define OPUS_QUERY_BAD_ARGUMENT 3 /* A required pointer is null; no output. */
#define OPUS_QUERY_INTERNAL_ERROR 4 /* The parser panicked; no output. */

/*
 * Parses len bytes of UTF-8 at text. On OPUS_QUERY_VALID and
 * OPUS_QUERY_INVALID, *out and *out_len receive a UTF-8 buffer that isn't
 * NUL-terminated; release it with opus_query_free(). Otherwise they receive
 * NULL and 0. text may be NULL only when len is 0. Thread-safe.
 */
int32_t opus_query_check(const uint8_t *text, size_t len, uint8_t **out, size_t *out_len);

/* Releases a buffer returned by opus_query_check(). NULL is ignored. */
void opus_query_free(uint8_t *ptr, size_t len);

#ifdef __cplusplus
}

#include <QString>

#include <memory>

namespace OpusSearchQuery
{
struct Result {
    int32_t status = OPUS_QUERY_INTERNAL_ERROR;
    /// The normalized query or the error message.
    QString text;
};

/// Checks UTF-8 bytes, copying the Rust-owned output into a QString.
inline Result check(const QByteArray &utf8)
{
    uint8_t *out = nullptr;
    size_t outLength = 0;
    const int32_t status = opus_query_check(reinterpret_cast<const uint8_t *>(utf8.constData()), size_t(utf8.size()), &out, &outLength);
    const auto release = [outLength](uint8_t *buffer) {
        opus_query_free(buffer, outLength);
    };
    const std::unique_ptr<uint8_t, decltype(release)> owned(out, release);
    return {status, QString::fromUtf8(reinterpret_cast<const char *>(owned.get()), qsizetype(outLength))};
}

/// Encodes @p text as UTF-8, replacing unpaired surrogates with U+FFFD; QString::toUtf8() drops them, which can
/// change a query's meaning. Like an unpaired surrogate, U+FFFD is neither a letter nor a space.
inline QByteArray toUtf8(QString text)
{
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i).isHighSurrogate() && i + 1 < text.size() && text.at(i + 1).isLowSurrogate()) {
            ++i;
        } else if (text.at(i).isSurrogate()) {
            text[i] = QChar::ReplacementCharacter;
        }
    }
    return text.toUtf8();
}

/// Checks a query; unpaired UTF-16 surrogates reach Rust as U+FFFD.
inline Result check(const QString &text)
{
    return check(toUtf8(text));
}
}
#endif
