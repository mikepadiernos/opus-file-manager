// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

#include <functional>

/**
 * Obsidian-style wiki links in Markdown: `[[Note]]`, `[[Note|alias]]`,
 * `[[Note#Heading]]` and embeds such as `![[Image.png]]`. Links inside code
 * blocks, code spans and HTML comments, and escaped brackets, are ignored.
 * The source text is never changed; previews rewrite a copy.
 */
namespace WikiLinks
{
struct Link {
    /// The note or file named, as written, without the heading or alias.
    QString target;
    /// The heading or block reference after `#`, if any.
    QString heading;
    /// The text after `|`, if any.
    QString alias;
    bool embed = false;
    /// Where the whole link, including `!`, starts in the text, and its length.
    qsizetype offset = 0;
    qsizetype length = 0;
    /// The zero-based line the link is on, and that line, trimmed and shortened.
    int line = 0;
    QString context;

    /// The text to show for the link: its alias, or its target and heading.
    QString displayText() const;
    bool operator==(const Link &other) const = default;
};

QList<Link> parse(const QString &markdown);

/**
 * Returns @p markdown with each wiki link replaced by a Markdown link to the
 * URL @p urlFor returns, or by its display text when that URL is empty.
 */
QString toMarkdownLinks(const QString &markdown, const std::function<QUrl(const Link &)> &urlFor);
}
