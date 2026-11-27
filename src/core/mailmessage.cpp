// SPDX-License-Identifier: GPL-3.0-or-later

#include "mailmessage.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>

namespace MailMessage
{
namespace
{
constexpr int DepthLimit = 16;
constexpr int PartLimit = 512;
constexpr qsizetype FileNameLimit = 200;

struct Part {
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray type = "text/plain";
    QMap<QByteArray, QString> typeParams;
    QByteArray disposition;
    QMap<QByteArray, QString> dispositionParams;
    QByteArray encoding;
    QByteArray body;
    QList<Part> children;

    QByteArray header(const QByteArray &name) const
    {
        for (const auto &header : headers) {
            if (header.first == name) {
                return header.second;
            }
        }
        return {};
    }
};

QString decodeText(const QByteArray &bytes, QByteArray charset)
{
    charset = charset.trimmed().toLower();
    if (charset.isEmpty() || charset == "us-ascii" || charset == "ascii" || charset == "utf8") {
        charset = "utf-8";
    }
    // Stateless, so a truncated sequence at the end is an error rather than pending input.
    QStringDecoder decoder(charset.constData(), QStringConverter::Flag::Stateless);
    if (!decoder.isValid()) {
        decoder = QStringDecoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    }
    QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        // Undeclared 8-bit text is most often Windows-1252 or Latin-1.
        QStringDecoder fallback("windows-1252");
        text = fallback.isValid() ? fallback.decode(bytes) : QString::fromLatin1(bytes);
    }
    return text;
}

int hexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

QByteArray decodeQuotedPrintable(const QByteArray &data, bool header)
{
    QByteArray out;
    out.reserve(data.size());
    for (qsizetype i = 0; i < data.size(); ++i) {
        const char c = data.at(i);
        if (header && c == '_') {
            out.append(' ');
        } else if (c == '=') {
            if (i + 1 < data.size() && data.at(i + 1) == '\n') {
                ++i;
            } else if (i + 2 < data.size() && data.at(i + 1) == '\r' && data.at(i + 2) == '\n') {
                i += 2;
            } else if (i + 2 < data.size() && hexValue(data.at(i + 1)) >= 0 && hexValue(data.at(i + 2)) >= 0) {
                out.append(char(hexValue(data.at(i + 1)) * 16 + hexValue(data.at(i + 2))));
                i += 2;
            } else {
                out.append(c);
            }
        } else {
            out.append(c);
        }
    }
    return out;
}

QByteArray decodeTransfer(const QByteArray &body, const QByteArray &encoding)
{
    if (encoding == "base64") {
        return QByteArray::fromBase64(body);
    }
    if (encoding == "quoted-printable") {
        return decodeQuotedPrintable(body, false);
    }
    return body;
}

QByteArray percentDecode(const QByteArray &value)
{
    return QByteArray::fromPercentEncoding(value);
}

/// Removes the quotes and backslash escapes of a quoted string.
QByteArray unquote(QByteArray value)
{
    value = value.trimmed();
    if (value.size() < 2 || !value.startsWith('"') || !value.endsWith('"')) {
        return value;
    }
    QByteArray out;
    for (qsizetype i = 1; i + 1 < value.size(); ++i) {
        if (value.at(i) == '\\' && i + 2 < value.size()) {
            ++i;
        }
        out.append(value.at(i));
    }
    return out;
}

/// Splits `type; a=b; c="d;e"` at semicolons outside quotes.
QList<QByteArray> splitParameters(const QByteArray &value)
{
    QList<QByteArray> parts;
    QByteArray current;
    bool quoted = false;
    for (qsizetype i = 0; i < value.size(); ++i) {
        const char c = value.at(i);
        if (c == '"' && (i == 0 || value.at(i - 1) != '\\')) {
            quoted = !quoted;
        }
        if (c == ';' && !quoted) {
            parts.append(current);
            current.clear();
        } else {
            current.append(c);
        }
    }
    parts.append(current);
    return parts;
}

/// Parses a structured header into its lowercase value and parameters,
/// joining RFC 2231 continuations and decoding their charsets.
QByteArray parseStructured(const QByteArray &value, QMap<QByteArray, QString> *params)
{
    const QList<QByteArray> parts = splitParameters(value);
    struct Section {
        QByteArray value;
        bool extended = false;
    };
    QMap<QByteArray, QMap<int, Section>> sections;
    for (qsizetype i = 1; i < parts.size(); ++i) {
        const qsizetype equals = parts.at(i).indexOf('=');
        if (equals < 0) {
            continue;
        }
        QByteArray key = parts.at(i).left(equals).trimmed().toLower();
        const QByteArray raw = parts.at(i).mid(equals + 1).trimmed();
        Section section;
        section.extended = key.endsWith('*');
        if (section.extended) {
            key.chop(1);
        }
        int number = -1;
        const qsizetype star = key.indexOf('*');
        if (star >= 0) {
            bool ok = false;
            number = key.mid(star + 1).toInt(&ok);
            if (!ok) {
                continue;
            }
            key = key.left(star);
        }
        section.value = raw;
        sections[key].insert(number, section);
    }
    for (auto it = sections.cbegin(); it != sections.cend(); ++it) {
        QByteArray charset;
        QByteArray bytes;
        bool anyExtended = false;
        bool first = true;
        for (const Section &section : it.value()) {
            if (section.extended) {
                anyExtended = true;
                QByteArray raw = section.value;
                if (first) {
                    // charset'language'value
                    const qsizetype quote = raw.indexOf('\'');
                    const qsizetype second = quote >= 0 ? raw.indexOf('\'', quote + 1) : -1;
                    if (second >= 0) {
                        charset = raw.left(quote);
                        raw = raw.mid(second + 1);
                    }
                }
                bytes += percentDecode(raw);
            } else {
                bytes += unquote(section.value);
            }
            first = false;
        }
        // Some clients put RFC 2047 encoded words in quoted file names.
        params->insert(it.key(), anyExtended ? decodeText(bytes, charset) : decodeHeader(bytes));
    }
    return parts.value(0).trimmed().toLower();
}

void splitHeaders(const QByteArray &raw, Part *part)
{
    qsizetype pos = 0;
    qsizetype bodyStart = raw.size();
    QList<QPair<QByteArray, QByteArray>> &headers = part->headers;
    bool firstLine = true;
    while (pos < raw.size()) {
        qsizetype end = raw.indexOf('\n', pos);
        const qsizetype next = end < 0 ? raw.size() : end + 1;
        if (end < 0) {
            end = raw.size();
        }
        QByteArray line = raw.mid(pos, end - pos);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        pos = next;
        if (line.isEmpty()) {
            bodyStart = next;
            break;
        }
        if ((line.startsWith(' ') || line.startsWith('\t')) && !headers.isEmpty()) {
            headers.last().second += line;
            continue;
        }
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0) {
            // An mbox "From " separator line, or not a header at all.
            if (firstLine && line.startsWith("From ")) {
                firstLine = false;
                continue;
            }
            if (headers.isEmpty()) {
                bodyStart = 0;
                break;
            }
            continue;
        }
        firstLine = false;
        headers.append({line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed()});
    }
    part->body = bodyStart < raw.size() ? raw.mid(bodyStart) : QByteArray();
}

void parsePart(const QByteArray &raw, Part *part, int depth, int *count)
{
    splitHeaders(raw, part);
    const QByteArray type = part->header("content-type");
    if (!type.isEmpty()) {
        const QByteArray parsed = parseStructured(type, &part->typeParams);
        if (parsed.contains('/')) {
            part->type = parsed;
        }
    }
    const QByteArray disposition = part->header("content-disposition");
    if (!disposition.isEmpty()) {
        part->disposition = parseStructured(disposition, &part->dispositionParams);
    }
    part->encoding = part->header("content-transfer-encoding").trimmed().toLower();

    const QString boundary = part->typeParams.value("boundary");
    if (!part->type.startsWith("multipart/") || boundary.isEmpty() || depth >= DepthLimit) {
        return;
    }
    const QByteArray delimiter = "--" + boundary.toUtf8();
    const QByteArray &body = part->body;
    qsizetype pos = 0;
    qsizetype partStart = -1;
    while (pos < body.size() && *count < PartLimit) {
        qsizetype end = body.indexOf('\n', pos);
        const qsizetype next = end < 0 ? body.size() : end + 1;
        if (end < 0) {
            end = body.size();
        }
        if (body.at(pos) == '-' && QByteArrayView(body).mid(pos, end - pos).startsWith(delimiter)) {
            const QByteArray rest = body.mid(pos + delimiter.size(), end - pos - delimiter.size()).trimmed();
            const bool closing = rest.startsWith("--");
            if (rest.isEmpty() || closing) {
                if (partStart >= 0) {
                    // The line break before a delimiter belongs to the delimiter.
                    qsizetype partEnd = pos;
                    if (partEnd > partStart && body.at(partEnd - 1) == '\n') {
                        --partEnd;
                        if (partEnd > partStart && body.at(partEnd - 1) == '\r') {
                            --partEnd;
                        }
                    }
                    Part child;
                    ++*count;
                    parsePart(body.mid(partStart, partEnd - partStart), &child, depth + 1, count);
                    part->children.append(std::move(child));
                }
                if (closing) {
                    partStart = -1;
                    break;
                }
                partStart = next;
            }
        }
        pos = next;
    }
    if (partStart >= 0 && partStart < body.size() && *count < PartLimit) {
        // A truncated message keeps its last, unterminated part.
        Part child;
        ++*count;
        parsePart(body.mid(partStart), &child, depth + 1, count);
        part->children.append(std::move(child));
    }
    part->body.clear();
}

struct Walker {
    Message message;
    bool haveBody = false;
    bool keepData = false;
    QList<QByteArray> data;
    QSet<QString> names;

    void addBody(const Part &part, bool html)
    {
        QString text = decodeText(decodeTransfer(part.body, part.encoding), part.typeParams.value("charset").toLatin1());
        if (html) {
            text = htmlToText(text);
        }
        if (haveBody) {
            message.body += QStringLiteral("\n\n") + text;
        } else {
            message.body = text;
            message.bodyFromHtml = html;
            haveBody = true;
        }
    }

    void addAttachment(const Part &part, const QString &suggested)
    {
        QString name = part.dispositionParams.value("filename");
        if (name.isEmpty()) {
            name = part.typeParams.value("name");
        }
        if (name.isEmpty()) {
            name = suggested;
        }
        QString fallback = QStringLiteral("attachment-%1").arg(message.attachments.size() + 1);
        const QString suffix = QMimeDatabase().mimeTypeForName(QString::fromLatin1(part.type)).preferredSuffix();
        if (!suffix.isEmpty()) {
            fallback += QLatin1Char('.') + suffix;
        }
        name = safeFileName(name, fallback);
        if (names.contains(name.toLower())) {
            const QFileInfo info(name);
            const QString base = info.completeBaseName().isEmpty() ? name : info.completeBaseName();
            const QString extension = info.completeBaseName().isEmpty() || info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
            for (int n = 2;; ++n) {
                const QString candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(n).arg(extension);
                if (!names.contains(candidate.toLower())) {
                    name = candidate;
                    break;
                }
            }
        }
        names.insert(name.toLower());
        const QByteArray decoded = decodeTransfer(part.body, part.encoding);
        message.attachments.append({name, QString::fromLatin1(part.type), decoded.size()});
        if (keepData) {
            data.append(decoded);
        }
    }

    static bool isSignature(const QByteArray &type)
    {
        return type == "application/pgp-signature" || type == "application/pkcs7-signature" || type == "application/x-pkcs7-signature";
    }

    void walk(const Part &part)
    {
        const QByteArray &type = part.type;
        if (type == "multipart/encrypted") {
            message.encrypted = true;
            return;
        }
        if (type == "application/pkcs7-mime" || type == "application/x-pkcs7-mime") {
            const QString smime = part.typeParams.value("smime-type").toLower();
            if (smime == QLatin1String("signed-data")) {
                message.signedMessage = true;
            } else {
                message.encrypted = true;
                return;
            }
        }
        if (type == "multipart/signed") {
            message.signedMessage = true;
            for (const Part &child : part.children) {
                if (!isSignature(child.type)) {
                    walk(child);
                }
            }
            return;
        }
        if (type == "multipart/alternative" && !part.children.isEmpty()) {
            for (const Part &child : part.children) {
                if (child.type == "text/plain" && child.disposition != "attachment") {
                    walk(child);
                    return;
                }
            }
            // The last alternative is the richest, often HTML with images.
            walk(part.children.last());
            return;
        }
        if (type.startsWith("multipart/")) {
            for (const Part &child : part.children) {
                walk(child);
            }
            return;
        }
        if (type == "message/rfc822" || type == "message/global") {
            Part inner;
            int count = 0;
            parsePart(decodeTransfer(part.body, part.encoding), &inner, DepthLimit, &count);
            const QString subject = decodeHeader(inner.header("subject")).simplified();
            addAttachment(part, (subject.isEmpty() ? QStringLiteral("message") : subject) + QStringLiteral(".eml"));
            return;
        }
        const bool named = part.dispositionParams.contains("filename") || part.typeParams.contains("name");
        const bool attachment = part.disposition == "attachment" || (named && part.disposition != "inline");
        if (!attachment && (type == "text/plain" || (type == "text/html" && !haveBody))) {
            addBody(part, type == "text/html");
            return;
        }
        addAttachment(part, QString());
    }
};

Walker walk(const QByteArray &data, qsizetype bodyLimit, bool keepData)
{
    Part root;
    int count = 0;
    parsePart(data, &root, 0, &count);
    Walker walker;
    walker.keepData = keepData;
    const struct {
        const char *key;
        const char *name;
    } shown[] = {{"from", "From"}, {"to", "To"}, {"cc", "Cc"}, {"date", "Date"}, {"subject", "Subject"}};
    for (const auto &header : shown) {
        const QByteArray value = root.header(header.key);
        if (!value.isEmpty()) {
            walker.message.headers.append({QString::fromLatin1(header.name), decodeHeader(value).simplified()});
        }
    }
    walker.message.subject = decodeHeader(root.header("subject")).simplified();
    walker.walk(root);
    Message &message = walker.message;
    if (message.body.contains(QLatin1String("-----BEGIN PGP MESSAGE-----"))) {
        message.encrypted = true;
    }
    if (message.body.contains(QLatin1String("-----BEGIN PGP SIGNED MESSAGE-----"))) {
        message.signedMessage = true;
    }
    if (message.body.size() > bodyLimit) {
        message.body.truncate(bodyLimit);
        message.bodyTruncated = true;
    }
    return walker;
}
}

Message parse(const QByteArray &data, qsizetype bodyLimit)
{
    return walk(data, bodyLimit, false).message;
}

std::optional<Message> load(const QString &path, qsizetype bodyLimit, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return std::nullopt;
    }
    if (file.size() > FileLimit) {
        *error = QStringLiteral("too large");
        return std::nullopt;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        *error = file.errorString();
        return std::nullopt;
    }
    return parse(data, bodyLimit);
}

std::optional<QByteArray> attachmentData(const QByteArray &data, int index, const QString &expectedName)
{
    Walker walker = walk(data, 0, true);
    if (index < 0 || index >= walker.data.size() || walker.message.attachments.at(index).fileName != expectedName) {
        return std::nullopt;
    }
    return walker.data.at(index);
}

QString decodeHeader(const QByteArray &value)
{
    const QString text = decodeText(value, {});
    static const QRegularExpression word(QStringLiteral(R"(=\?([^?\s]+)\?([bBqQ])\?([^?\s]*)\?=)"));
    QString out;
    qsizetype last = 0;
    bool previousWasWord = false;
    auto it = word.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString gap = text.mid(last, match.capturedStart() - last);
        // Whitespace between adjacent encoded words is not part of the text.
        if (!(previousWasWord && gap.trimmed().isEmpty())) {
            out += gap;
        }
        QByteArray charset = match.captured(1).toLatin1();
        const qsizetype language = charset.indexOf('*');
        if (language >= 0) {
            charset.truncate(language);
        }
        const QByteArray encoded = match.captured(3).toLatin1();
        const bool base64 = match.captured(2).compare(QLatin1String("b"), Qt::CaseInsensitive) == 0;
        out += decodeText(base64 ? QByteArray::fromBase64(encoded) : decodeQuotedPrintable(encoded, true), charset);
        last = match.capturedEnd();
        previousWasWord = true;
    }
    out += text.mid(last);
    return out;
}

QString htmlToText(const QString &html)
{
    static const QRegularExpression hidden(QStringLiteral(R"(<(script|style|head|title|template)\b.*?</\1\s*>)"),
                                           QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression comment(QStringLiteral("<!--.*?-->"), QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression space(QStringLiteral(R"(\s+)"));
    static const QRegularExpression lineBreak(QStringLiteral(R"(<br\s*/?>)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression blockEnd(QStringLiteral(R"(</(p|div|tr|h[1-6]|blockquote|table|ul|ol|pre)\s*>)"),
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression blockStart(QStringLiteral(R"(<(p|div|tr|h[1-6]|blockquote|table|ul|ol|pre)\b[^>]*>)"),
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression item(QStringLiteral(R"(<li\b[^>]*>)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cell(QStringLiteral(R"(</t[dh]\s*>)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    static const QRegularExpression entity(QStringLiteral(R"(&(#[0-9]{1,7}|#[xX][0-9a-fA-F]{1,6}|[a-zA-Z]{2,8});)"));
    static const QRegularExpression blankLines(QStringLiteral(R"(\n{3,})"));

    QString text = html;
    text.remove(hidden);
    text.remove(comment);
    text.replace(space, QStringLiteral(" "));
    text.replace(lineBreak, QStringLiteral("\n"));
    text.replace(blockEnd, QStringLiteral("\n"));
    text.replace(blockStart, QStringLiteral("\n"));
    text.replace(item, QStringLiteral("\n• "));
    text.replace(cell, QStringLiteral("\t"));
    text.remove(tag);

    static const QMap<QString, QString> named{
        {QStringLiteral("amp"), QStringLiteral("&")},
        {QStringLiteral("lt"), QStringLiteral("<")},
        {QStringLiteral("gt"), QStringLiteral(">")},
        {QStringLiteral("quot"), QStringLiteral("\"")},
        {QStringLiteral("apos"), QStringLiteral("'")},
        {QStringLiteral("nbsp"), QStringLiteral(" ")},
        {QStringLiteral("copy"), QStringLiteral("©")},
        {QStringLiteral("reg"), QStringLiteral("®")},
        {QStringLiteral("hellip"), QStringLiteral("…")},
        {QStringLiteral("mdash"), QStringLiteral("—")},
        {QStringLiteral("ndash"), QStringLiteral("–")},
    };
    QString decoded;
    qsizetype last = 0;
    auto it = entity.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        decoded += text.mid(last, match.capturedStart() - last);
        const QString name = match.captured(1);
        QString replacement = match.captured(0);
        if (name.startsWith(QLatin1Char('#'))) {
            bool ok = false;
            const uint code = name.startsWith(QLatin1String("#x"), Qt::CaseInsensitive) ? name.mid(2).toUInt(&ok, 16) : name.mid(1).toUInt(&ok, 10);
            if (ok && code > 0 && code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF)) {
                const char32_t character = code;
                replacement = QString::fromUcs4(&character, 1);
            }
        } else if (named.contains(name.toLower())) {
            replacement = named.value(name.toLower());
        }
        decoded += replacement;
        last = match.capturedEnd();
    }
    decoded += text.mid(last);

    QStringList lines = decoded.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        line = line.trimmed();
    }
    return lines.join(QLatin1Char('\n')).replace(blankLines, QStringLiteral("\n\n")).trimmed();
}

QString safeFileName(const QString &name, const QString &fallback)
{
    QString safe;
    safe.reserve(name.size());
    for (const QChar c : name) {
        if (c == QLatin1Char('/') || c == QLatin1Char('\\') || c.category() == QChar::Other_Control || c.category() == QChar::Other_Format) {
            safe += QLatin1Char('_');
        } else {
            safe += c;
        }
    }
    safe = safe.simplified();
    while (safe.startsWith(QLatin1Char('.'))) {
        safe.remove(0, 1);
    }
    if (safe.size() > FileNameLimit) {
        const QString suffix = QFileInfo(safe).suffix();
        safe = suffix.isEmpty() || suffix.size() > 16 ? safe.left(FileNameLimit) : safe.left(FileNameLimit - suffix.size() - 1) + QLatin1Char('.') + suffix;
    }
    return safe.isEmpty() ? fallback : safe;
}
}
