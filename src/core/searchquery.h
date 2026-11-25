// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QFileInfo>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

/**
 * Advanced search queries: name, path, type, size, modified, content, tag
 * and tagged criteria combined with AND, OR, NOT and parentheses.
 *
 * Words without a field match file names. Juxtaposed terms are combined with
 * AND, which binds more tightly than OR. Operators are recognised only in
 * upper case; quote a value to search for them literally.
 */
namespace SearchQuery
{
enum class Field {
    Name,
    Path,
    Type,
    Size,
    Modified,
    Content,
    /// A library tag by name, alias or shorthand; tag: includes child tags, tag= doesn't.
    Tag,
    /// Whether a library item has tags.
    Tagged,
};

enum class Comparison {
    Contains,
    Equal,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
};

enum class TypeMatch {
    File,
    Folder,
    MimeType,
    MimePrefix,
    Extension,
};

struct Term {
    Field field = Field::Name;
    Comparison comparison = Comparison::Contains;
    /// The value as written, without quotes.
    QString text;
    /// Set for unquoted name and path values containing * or ?.
    QRegularExpression pattern;
    TypeMatch type = TypeMatch::File;
    qint64 size = 0;
    /// Start (inclusive) and end (exclusive) of a modified-date period, in local time.
    QDateTime from;
    QDateTime to;
    /// For tagged criteria, whether matching items have tags.
    bool tagged = true;
};

struct Node {
    enum class Kind {
        Term,
        And,
        Or,
        Not,
    };
    Kind kind = Kind::Term;
    Term term;
    std::vector<Node> children;
};

/// Reports whether the entry's text contains @p needle; files that can't be searched as text don't.
using ContentMatcher = std::function<bool(const QString &needle)>;
/// Reports whether the entry satisfies a tag or tagged @p term; entries outside a tag library satisfy none.
using TagMatcher = std::function<bool(const Term &term)>;

class Query
{
public:
    /// Parses @p text; check isValid() before use. @p now resolves today and yesterday.
    static Query parse(const QString &text, const QDateTime &now = QDateTime::currentDateTime());

    bool isValid() const;
    /// Explains why the query is invalid, or is empty.
    QString error() const;
    bool usesContent() const;
    bool usesTags() const;
    /// The distinct values of content terms, in query order.
    QStringList contentTerms() const;
    /// Positive literal name and content terms that every match must satisfy.
    QList<Term> requiredTerms() const;
    /**
     * Evaluates the query; @p content and @p tags are called only for the
     * terms that affect the result. Without @p tags, no tag term matches.
     */
    bool matches(const QFileInfo &entry, const ContentMatcher &content = {}, const TagMatcher &tags = {}) const;
    /// Normalized form with explicit operators and grouping.
    QString toString() const;

private:
    Node m_root;
    QString m_error;
};
}
