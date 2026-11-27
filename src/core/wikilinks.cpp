// SPDX-License-Identifier: GPL-3.0-or-later

#include "wikilinks.h"

#include <QSet>

#include <algorithm>

namespace WikiLinks
{
namespace
{
constexpr qsizetype ContextLimit = 160;

int indentationOf(QStringView line)
{
    int width = 0;
    for (const QChar c : line) {
        if (c == QLatin1Char(' ')) {
            ++width;
        } else if (c == QLatin1Char('\t')) {
            width += 4 - width % 4;
        } else {
            break;
        }
    }
    return width;
}

bool startsListItem(QStringView trimmed)
{
    if (trimmed.size() >= 2 && QStringView(u"-*+").contains(trimmed.at(0)) && trimmed.at(1).isSpace()) {
        return true;
    }
    qsizetype digits = 0;
    while (digits < trimmed.size() && digits < 9 && trimmed.at(digits).isDigit()) {
        ++digits;
    }
    return digits > 0 && digits + 1 < trimmed.size() && (trimmed.at(digits) == QLatin1Char('.') || trimmed.at(digits) == QLatin1Char(')'))
        && trimmed.at(digits + 1).isSpace();
}

/// A code fence opener or closer: three or more backticks or tildes after at most three spaces.
bool fenceOf(QStringView line, QChar *marker, qsizetype *count)
{
    if (indentationOf(line) > 3) {
        return false;
    }
    const QStringView trimmed = line.trimmed();
    if (trimmed.isEmpty() || (trimmed.at(0) != QLatin1Char('`') && trimmed.at(0) != QLatin1Char('~'))) {
        return false;
    }
    qsizetype run = 0;
    while (run < trimmed.size() && trimmed.at(run) == trimmed.at(0)) {
        ++run;
    }
    if (run < 3 || (trimmed.at(0) == QLatin1Char('`') && trimmed.mid(run).contains(QLatin1Char('`')))) {
        return false;
    }
    *marker = trimmed.at(0);
    *count = run;
    return true;
}

Link linkFrom(QStringView inner, bool embed)
{
    Link link;
    link.embed = embed;
    QStringView target = inner;
    const qsizetype bar = inner.indexOf(QLatin1Char('|'));
    if (bar >= 0) {
        link.alias = inner.mid(bar + 1).trimmed().toString();
        // Inside a table, the pipe is escaped as `\|`.
        target = inner.first(bar > 0 && inner.at(bar - 1) == QLatin1Char('\\') ? bar - 1 : bar);
    }
    const qsizetype hash = target.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        link.heading = target.mid(hash + 1).trimmed().toString();
        target = target.first(hash);
    }
    link.target = target.trimmed().toString();
    return link;
}

bool isTagCharacter(QChar c)
{
    return c.isLetterOrNumber() || c.isMark() || c == QLatin1Char('_') || c == QLatin1Char('-') || c == QLatin1Char('/');
}

/// @p text without a leading `#` and trailing `/` when it's a valid tag, or empty.
QString tagFrom(QStringView text)
{
    if (text.startsWith(QLatin1Char('#'))) {
        text = text.mid(1);
    }
    while (text.endsWith(QLatin1Char('/'))) {
        text.chop(1);
    }
    bool hasNonDigit = false;
    for (const QChar c : text) {
        if (!isTagCharacter(c)) {
            return {};
        }
        hasNonDigit = hasNonDigit || !c.isDigit();
    }
    return hasNonDigit ? text.toString() : QString();
}

/// Adds the tags listed by a front matter value such as `[a, "b"]` or `a, b`.
void addFrontMatterTags(QStringView value, QStringList *tags)
{
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('[')) && value.endsWith(QLatin1Char(']'))) {
        value = value.mid(1, value.size() - 2);
    }
    for (QStringView part : value.split(QLatin1Char(','))) {
        for (QStringView word : part.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            word = word.trimmed();
            if (word.size() >= 2 && (word.front() == QLatin1Char('"') || word.front() == QLatin1Char('\'')) && word.back() == word.front()) {
                word = word.mid(1, word.size() - 2);
            }
            const QString tag = tagFrom(word);
            if (!tag.isEmpty()) {
                tags->append(tag);
            }
        }
    }
}

void scanLine(const QString &text, qsizetype start, qsizetype end, int lineNumber, bool *inComment, QList<Link> *links, QStringList *tags)
{
    qsizetype i = start;
    while (i < end) {
        if (*inComment) {
            const qsizetype close = text.indexOf(QLatin1String("-->"), i);
            if (close < 0 || close >= end) {
                return;
            }
            *inComment = false;
            i = close + 3;
            continue;
        }
        const QChar c = text.at(i);
        if (c == QLatin1Char('\\')) {
            i += 2;
            continue;
        }
        if (c == QLatin1Char('<') && QStringView(text).mid(i, 4) == QLatin1String("<!--")) {
            *inComment = true;
            i += 4;
            continue;
        }
        if (c == QLatin1Char('`')) {
            qsizetype run = 0;
            while (i + run < end && text.at(i + run) == QLatin1Char('`')) {
                ++run;
            }
            // A code span ends at the next run of exactly the same length.
            qsizetype j = i + run;
            qsizetype closing = -1;
            while (j < end) {
                if (text.at(j) != QLatin1Char('`')) {
                    ++j;
                    continue;
                }
                qsizetype other = 0;
                while (j + other < end && text.at(j + other) == QLatin1Char('`')) {
                    ++other;
                }
                if (other == run) {
                    closing = j + other;
                    break;
                }
                j += other;
            }
            i = closing >= 0 ? closing : i + run;
            continue;
        }
        if (tags && c == QLatin1Char('#') && (i == start || text.at(i - 1).isSpace())) {
            qsizetype j = i + 1;
            while (j < end && isTagCharacter(text.at(j))) {
                ++j;
            }
            const QString tag = tagFrom(QStringView(text).mid(i + 1, j - i - 1));
            if (!tag.isEmpty()) {
                tags->append(tag);
            }
            i = j;
            continue;
        }
        const bool embed = c == QLatin1Char('!') && QStringView(text).mid(i + 1, 2) == QLatin1String("[[");
        if (embed || (c == QLatin1Char('[') && i + 1 < end && text.at(i + 1) == QLatin1Char('['))) {
            const qsizetype open = embed ? i + 3 : i + 2;
            const qsizetype close = text.indexOf(QLatin1String("]]"), open);
            const QStringView inner = close >= 0 && close < end ? QStringView(text).mid(open, close - open) : QStringView();
            if (!inner.trimmed().isEmpty() && !inner.contains(QLatin1String("[["))) {
                Link link = linkFrom(inner, embed);
                if (!link.target.isEmpty() || !link.heading.isEmpty()) {
                    link.offset = i;
                    link.length = close + 2 - i;
                    link.line = lineNumber;
                    link.context = QStringView(text).mid(start, end - start).trimmed().left(ContextLimit).toString();
                    if (links) {
                        links->append(link);
                    }
                    i = close + 2;
                    continue;
                }
            }
            i += embed ? 3 : 2;
            continue;
        }
        ++i;
    }
}

QString escaped(const QString &text)
{
    QString out;
    out.reserve(text.size() + 8);
    for (const QChar c : text) {
        if (c.unicode() < 128 && c.isPunct()) {
            out += QLatin1Char('\\');
        } else if (c.unicode() < 128 && c.isSymbol()) {
            out += QLatin1Char('\\');
        }
        out += c;
    }
    return out;
}
}

QString Link::displayText() const
{
    if (!alias.isEmpty()) {
        return alias;
    }
    if (heading.isEmpty()) {
        return target;
    }
    return target.isEmpty() ? heading : target + QStringLiteral(" › ") + heading;
}

namespace
{
void walk(const QString &markdown, QList<Link> *links, QStringList *tags)
{
    bool inFrontMatter = false;
    bool inFrontMatterTags = false;
    bool inComment = false;
    bool inFence = false;
    QChar fenceMarker;
    qsizetype fenceCount = 0;
    bool previousBlank = true;
    bool inIndentedCode = false;
    int lineNumber = 0;
    qsizetype start = 0;
    while (start <= markdown.size()) {
        qsizetype end = markdown.indexOf(QLatin1Char('\n'), start);
        if (end < 0) {
            end = markdown.size();
        }
        qsizetype contentEnd = end;
        if (contentEnd > start && markdown.at(contentEnd - 1) == QLatin1Char('\r')) {
            --contentEnd;
        }
        const QStringView line = QStringView(markdown).mid(start, contentEnd - start);
        const bool blank = line.trimmed().isEmpty();
        QChar marker;
        qsizetype count = 0;
        if (lineNumber == 0 && line == QLatin1String("---")) {
            inFrontMatter = true;
        } else if (inFrontMatter) {
            // Links in front matter values still count; inline tags don't.
            if (line == QLatin1String("---") || line == QLatin1String("...")) {
                inFrontMatter = false;
            } else if (inFrontMatterTags && (line.startsWith(QLatin1Char(' ')) || line.startsWith(QLatin1Char('-')))) {
                const QStringView item = line.trimmed();
                if (tags && item.startsWith(QLatin1Char('-'))) {
                    addFrontMatterTags(item.mid(1), tags);
                }
            } else {
                const qsizetype colon = line.indexOf(QLatin1Char(':'));
                const QStringView key = colon > 0 ? line.first(colon).trimmed() : QStringView();
                inFrontMatterTags = key.compare(QLatin1String("tags"), Qt::CaseInsensitive) == 0 || key.compare(QLatin1String("tag"), Qt::CaseInsensitive) == 0;
                if (inFrontMatterTags && tags) {
                    addFrontMatterTags(line.mid(colon + 1), tags);
                }
            }
            if (inFrontMatter) {
                scanLine(markdown, start, contentEnd, lineNumber, &inComment, links, nullptr);
            }
        } else if (inFence) {
            if (fenceOf(line, &marker, &count) && marker == fenceMarker && count >= fenceCount && line.trimmed().size() == count) {
                inFence = false;
            }
        } else if (!inComment && fenceOf(line, &marker, &count)) {
            inFence = true;
            fenceMarker = marker;
            fenceCount = count;
            inIndentedCode = false;
        } else if (!inComment && !blank && indentationOf(line) >= 4 && (previousBlank || inIndentedCode) && !startsListItem(line.trimmed())) {
            inIndentedCode = true;
        } else {
            if (!blank) {
                inIndentedCode = false;
            }
            scanLine(markdown, start, contentEnd, lineNumber, &inComment, links, tags);
        }
        previousBlank = blank;
        ++lineNumber;
        start = end + 1;
    }
}
}

QList<Link> parse(const QString &markdown)
{
    QList<Link> links;
    walk(markdown, &links, nullptr);
    return links;
}

QStringList tags(const QString &markdown)
{
    QStringList found;
    walk(markdown, nullptr, &found);
    QStringList unique;
    QSet<QString> seen;
    for (const QString &tag : std::as_const(found)) {
        if (!seen.contains(tag.toCaseFolded())) {
            seen.insert(tag.toCaseFolded());
            unique.append(tag);
        }
    }
    return unique;
}

QString toMarkdownLinks(const QString &markdown, const std::function<QUrl(const Link &)> &urlFor)
{
    QList<Link> links = parse(markdown);
    QString out = markdown;
    std::reverse(links.begin(), links.end());
    for (const Link &link : std::as_const(links)) {
        const QUrl url = urlFor(link);
        const QString text = escaped(link.displayText());
        const QString replacement = url.isEmpty() ? text : QStringLiteral("[%1](<%2>)").arg(text, QString::fromLatin1(url.toEncoded(QUrl::FullyEncoded)));
        out.replace(link.offset, link.length, replacement);
    }
    return out;
}
}
