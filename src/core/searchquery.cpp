// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchquery.h"

#include <KLocalizedString>

#include <QMimeDatabase>

#include <algorithm>
#include <cmath>
#include <optional>

using namespace SearchQuery;

namespace
{
struct Token {
    enum class Kind {
        Word,
        Open,
        Close,
        And,
        Or,
        Not,
    };
    Kind kind = Kind::Word;
    /// Field name as written, empty for a plain word.
    QString field;
    QString op;
    QString value;
    bool quoted = false;
    /// The token as written, for error messages.
    QString source;
};

bool isBreak(QChar c)
{
    return c.isSpace() || c == QLatin1Char('(') || c == QLatin1Char(')');
}

bool isOperatorChar(QChar c)
{
    return c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char('=');
}

QString tokenize(const QString &text, QList<Token> &tokens)
{
    qsizetype i = 0;
    const qsizetype length = text.size();
    while (i < length) {
        const QChar c = text.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char(')')) {
            tokens.append({c == QLatin1Char('(') ? Token::Kind::Open : Token::Kind::Close, {}, {}, {}, false, QString(c)});
            ++i;
            continue;
        }
        if (c == QLatin1Char('-') && i + 1 < length && !isBreak(text.at(i + 1))) {
            tokens.append({Token::Kind::Not, {}, {}, {}, false, QStringLiteral("-")});
            ++i;
            continue;
        }
        const qsizetype start = i;
        Token token;
        // A field is a run of letters followed by ':' or a comparison operator.
        qsizetype end = i;
        while (end < length && text.at(end).isLetter()) {
            ++end;
        }
        if (end > i && end < length && (text.at(end) == QLatin1Char(':') || isOperatorChar(text.at(end)))) {
            token.field = text.mid(i, end - i);
            i = end;
            if (text.at(i) == QLatin1Char(':')) {
                ++i;
            }
            const qsizetype opStart = i;
            while (i < length && isOperatorChar(text.at(i))) {
                ++i;
            }
            token.op = text.mid(opStart, i - opStart);
        }
        if (i < length && text.at(i) == QLatin1Char('"')) {
            const qsizetype close = text.indexOf(QLatin1Char('"'), i + 1);
            if (close < 0) {
                return i18nc("@info", "A quotation mark is not closed: %1", text.mid(start));
            }
            token.value = text.mid(i + 1, close - i - 1);
            token.quoted = true;
            i = close + 1;
            if (i < length && !isBreak(text.at(i))) {
                return i18nc("@info", "Separate a quoted value from the following text with a space: %1", text.mid(start, i - start + 1));
            }
        } else {
            const qsizetype valueStart = i;
            while (i < length && !isBreak(text.at(i))) {
                if (text.at(i) == QLatin1Char('"')) {
                    return i18nc("@info", "Quote a whole value, not part of a word: %1", text.mid(start, i - start + 1));
                }
                ++i;
            }
            token.value = text.mid(valueStart, i - valueStart);
        }
        token.source = text.mid(start, i - start);
        if (token.field.isEmpty() && !token.quoted) {
            if (token.value == QLatin1String("AND")) {
                token.kind = Token::Kind::And;
            } else if (token.value == QLatin1String("OR")) {
                token.kind = Token::Kind::Or;
            } else if (token.value == QLatin1String("NOT")) {
                token.kind = Token::Kind::Not;
            }
        }
        tokens.append(token);
    }
    return {};
}

std::optional<Field> fieldNamed(const QString &name)
{
    const QString key = name.toLower();
    if (key == QLatin1String("name") || key == QLatin1String("filename")) {
        return Field::Name;
    }
    if (key == QLatin1String("path")) {
        return Field::Path;
    }
    if (key == QLatin1String("type")) {
        return Field::Type;
    }
    if (key == QLatin1String("size")) {
        return Field::Size;
    }
    if (key == QLatin1String("modified") || key == QLatin1String("date")) {
        return Field::Modified;
    }
    if (key == QLatin1String("content") || key == QLatin1String("contents")) {
        return Field::Content;
    }
    if (key == QLatin1String("tag") || key == QLatin1String("tags")) {
        return Field::Tag;
    }
    if (key == QLatin1String("tagged")) {
        return Field::Tagged;
    }
    if (key == QLatin1String("meta") || key == QLatin1String("metadata")) {
        return Field::Metadata;
    }
    return std::nullopt;
}

std::optional<Comparison> comparisonFor(const QString &op)
{
    if (op.isEmpty()) {
        return Comparison::Contains;
    }
    if (op == QLatin1String("=")) {
        return Comparison::Equal;
    }
    if (op == QLatin1String("<")) {
        return Comparison::Less;
    }
    if (op == QLatin1String("<=")) {
        return Comparison::LessOrEqual;
    }
    if (op == QLatin1String(">")) {
        return Comparison::Greater;
    }
    if (op == QLatin1String(">=")) {
        return Comparison::GreaterOrEqual;
    }
    return std::nullopt;
}

bool ordered(Comparison comparison)
{
    return comparison == Comparison::Less || comparison == Comparison::LessOrEqual || comparison == Comparison::Greater
        || comparison == Comparison::GreaterOrEqual;
}

std::optional<qint64> parseSize(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^(\\d+(?:\\.\\d+)?)\\s*([kmgt]i?b?|b)?$"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    double value = match.captured(1).toDouble();
    const QString unit = match.captured(2).toLower();
    const QString units = QStringLiteral("kmgt");
    if (!unit.isEmpty() && unit != QLatin1String("b")) {
        value *= std::pow(1024.0, units.indexOf(unit.at(0)) + 1);
    }
    if (value > 9.0e18) {
        return std::nullopt;
    }
    return static_cast<qint64>(std::llround(value));
}

bool parsePeriod(const QString &text, const QDateTime &now, QDateTime &from, QDateTime &to)
{
    const QString key = text.toLower();
    QDate start;
    QDate end;
    if (key == QLatin1String("today") || key == QLatin1String("yesterday")) {
        start = now.date().addDays(key == QLatin1String("today") ? 0 : -1);
        end = start.addDays(1);
    } else if (static const QRegularExpression day(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$")); day.match(text).hasMatch()) {
        start = QDate::fromString(text, Qt::ISODate);
        end = start.addDays(1);
    } else if (static const QRegularExpression month(QStringLiteral("^(\\d{4})-(\\d{2})$")); month.match(text).hasMatch()) {
        start = QDate(text.left(4).toInt(), text.mid(5, 2).toInt(), 1);
        end = start.addMonths(1);
    } else if (static const QRegularExpression year(QStringLiteral("^\\d{4}$")); year.match(text).hasMatch()) {
        start = QDate(text.toInt(), 1, 1);
        end = start.addYears(1);
    }
    if (!start.isValid() || !end.isValid()) {
        return false;
    }
    from = start.startOfDay();
    to = end.startOfDay();
    return true;
}

QString buildTerm(const Token &token, const QDateTime &now, Term &term)
{
    if (!token.field.isEmpty()) {
        const std::optional<Field> field = fieldNamed(token.field);
        if (!field) {
            return i18nc("@info",
                         "Unknown field “%1”. Use name, path, type, size, modified, content, tag, tagged or meta, or quote the text to search for it "
                         "literally.",
                         token.field);
        }
        term.field = *field;
    }
    const std::optional<Comparison> comparison = comparisonFor(token.op);
    if (!comparison) {
        return i18nc("@info", "Unknown comparison “%1” in %2. Use :, =, <, <=, > or >=.", token.op, token.source);
    }
    term.comparison = *comparison;
    term.text = token.value;
    if (term.text.isEmpty()) {
        return token.field.isEmpty() ? i18nc("@info", "Enter text inside the quotation marks.") : i18nc("@info", "Enter a value after %1.", token.source);
    }
    switch (term.field) {
    case Field::Name:
    case Field::Path:
    case Field::Content:
    case Field::Tag:
    case Field::Metadata:
        if (ordered(term.comparison)) {
            if (term.field == Field::Metadata) {
                return i18nc("@info", "Metadata criteria can't use <, <=, > or >=: %1", token.source);
            }
            return term.field == Field::Tag ? i18nc("@info", "Tag criteria can't use <, <=, > or >=: %1", token.source)
                                            : i18nc("@info", "Name, path and content criteria can't use <, <=, > or >=: %1", token.source);
        }
        if (term.field == Field::Content && term.comparison == Comparison::Equal) {
            return i18nc("@info", "Content criteria match text within files; use content: instead of content=: %1", token.source);
        }
        if (term.field != Field::Content && !token.quoted && (term.text.contains(QLatin1Char('*')) || term.text.contains(QLatin1Char('?')))) {
            term.pattern = QRegularExpression(QRegularExpression::wildcardToRegularExpression(term.text, QRegularExpression::NonPathWildcardConversion),
                                              QRegularExpression::CaseInsensitiveOption);
        }
        break;
    case Field::Type: {
        if (ordered(term.comparison)) {
            return i18nc("@info", "Type criteria can't use <, <=, > or >=: %1", token.source);
        }
        const QString value = term.text.toLower();
        if (value == QLatin1String("file") || value == QLatin1String("files")) {
            term.type = TypeMatch::File;
        } else if (value == QLatin1String("folder") || value == QLatin1String("folders") || value == QLatin1String("dir")
                   || value == QLatin1String("directory")) {
            term.type = TypeMatch::Folder;
        } else if (value.endsWith(QLatin1String("/*")) && value.count(QLatin1Char('/')) == 1 && value.size() > 2) {
            term.type = TypeMatch::MimePrefix;
            term.text = value.chopped(1);
        } else if (value.contains(QLatin1Char('/'))) {
            const QMimeType mimeType = QMimeDatabase().mimeTypeForName(value);
            if (!mimeType.isValid()) {
                return i18nc("@info",
                             "Unknown file type “%1”. Use file, folder, an extension such as pdf, or a MIME type such as image/png or image/*.",
                             term.text);
            }
            term.type = TypeMatch::MimeType;
            term.text = mimeType.name();
        } else {
            QString extension = value;
            if (extension.startsWith(QLatin1String("*."))) {
                extension.remove(0, 2);
            } else if (extension.startsWith(QLatin1Char('.'))) {
                extension.remove(0, 1);
            }
            if (extension.isEmpty() || extension.contains(QLatin1Char('*')) || extension.contains(QLatin1Char('?'))) {
                return i18nc("@info",
                             "Unknown file type “%1”. Use file, folder, an extension such as pdf, or a MIME type such as image/png or image/*.",
                             term.text);
            }
            term.type = TypeMatch::Extension;
            term.text = extension;
        }
        break;
    }
    case Field::Size: {
        const std::optional<qint64> size = parseSize(term.text);
        if (!size) {
            return i18nc("@info", "“%1” is not a valid size. Use a number with an optional unit, such as 500K, 10M or 2G.", term.text);
        }
        term.size = *size;
        if (term.comparison == Comparison::Contains) {
            term.comparison = Comparison::Equal;
        }
        break;
    }
    case Field::Modified:
        if (!parsePeriod(term.text, now, term.from, term.to)) {
            return i18nc("@info", "“%1” is not a valid date. Use YYYY-MM-DD, YYYY-MM, YYYY, today or yesterday.", term.text);
        }
        if (term.comparison == Comparison::Contains) {
            term.comparison = Comparison::Equal;
        }
        break;
    case Field::Tagged: {
        if (ordered(term.comparison)) {
            return i18nc("@info", "Tag criteria can't use <, <=, > or >=: %1", token.source);
        }
        const QString value = term.text.toLower();
        if (value == QLatin1String("yes") || value == QLatin1String("true")) {
            term.tagged = true;
        } else if (value == QLatin1String("no") || value == QLatin1String("false")) {
            term.tagged = false;
        } else {
            return i18nc("@info", "“%1” is not a valid tagged value. Use tagged:yes or tagged:no.", term.text);
        }
        term.text = term.tagged ? QStringLiteral("yes") : QStringLiteral("no");
        term.comparison = Comparison::Contains;
        break;
    }
    }
    return {};
}

class Parser
{
public:
    Parser(const QList<Token> &tokens, const QDateTime &now)
        : m_tokens(tokens)
        , m_now(now)
    {
    }

    QString parse(Node &root)
    {
        if (const QString error = parseOr(root); !error.isEmpty()) {
            return error;
        }
        if (m_position < m_tokens.size()) {
            return i18nc("@info", "A closing parenthesis has no matching opening parenthesis.");
        }
        return {};
    }

private:
    const Token *peek() const
    {
        return m_position < m_tokens.size() ? &m_tokens.at(m_position) : nullptr;
    }

    static void append(Node &parent, Node &&child)
    {
        if (child.kind == parent.kind) {
            for (Node &grandchild : child.children) {
                parent.children.push_back(std::move(grandchild));
            }
        } else {
            parent.children.push_back(std::move(child));
        }
    }

    QString parseOr(Node &node)
    {
        if (const QString error = parseAnd(node); !error.isEmpty()) {
            return error;
        }
        while (peek() && peek()->kind == Token::Kind::Or) {
            ++m_position;
            Node right;
            if (const QString error = parseAnd(right, QStringLiteral("OR")); !error.isEmpty()) {
                return error;
            }
            if (node.kind != Node::Kind::Or) {
                Node left = std::move(node);
                node = Node{Node::Kind::Or, {}, {}};
                append(node, std::move(left));
            }
            append(node, std::move(right));
        }
        return {};
    }

    QString parseAnd(Node &node, const QString &after = {})
    {
        if (const QString error = parseUnary(node, after); !error.isEmpty()) {
            return error;
        }
        while (const Token *token = peek()) {
            if (token->kind == Token::Kind::Or || token->kind == Token::Kind::Close) {
                break;
            }
            QString operatorName;
            if (token->kind == Token::Kind::And) {
                ++m_position;
                operatorName = QStringLiteral("AND");
            }
            Node right;
            if (const QString error = parseUnary(right, operatorName); !error.isEmpty()) {
                return error;
            }
            if (node.kind != Node::Kind::And) {
                Node left = std::move(node);
                node = Node{Node::Kind::And, {}, {}};
                append(node, std::move(left));
            }
            append(node, std::move(right));
        }
        return {};
    }

    QString parseUnary(Node &node, const QString &after)
    {
        const Token *token = peek();
        if (token && token->kind == Token::Kind::Not) {
            ++m_position;
            Node child;
            if (const QString error = parseUnary(child, token->source); !error.isEmpty()) {
                return error;
            }
            if (child.kind == Node::Kind::Not) {
                node = std::move(child.children.front());
            } else {
                node = Node{Node::Kind::Not, {}, {}};
                node.children.push_back(std::move(child));
            }
            return {};
        }
        return parsePrimary(node, after);
    }

    QString parsePrimary(Node &node, const QString &after)
    {
        const Token *token = peek();
        if (!token) {
            return after.isEmpty() ? i18nc("@info", "Enter text to search for.") : i18nc("@info", "Add a search term after %1.", after);
        }
        switch (token->kind) {
        case Token::Kind::Open: {
            ++m_position;
            if (peek() && peek()->kind == Token::Kind::Close) {
                return i18nc("@info", "Parentheses must contain a search term.");
            }
            if (const QString error = parseOr(node); !error.isEmpty()) {
                return error;
            }
            if (!peek() || peek()->kind != Token::Kind::Close) {
                return i18nc("@info", "An opening parenthesis is not closed.");
            }
            ++m_position;
            return {};
        }
        case Token::Kind::Close:
            return after.isEmpty() ? i18nc("@info", "A closing parenthesis has no matching opening parenthesis.")
                                   : i18nc("@info", "Add a search term after %1.", after);
        case Token::Kind::And:
        case Token::Kind::Or:
            return i18nc("@info", "%1 must be placed between two search terms. Quote it to search for the word.", token->source);
        case Token::Kind::Not:
        case Token::Kind::Word:
            break;
        }
        ++m_position;
        node = Node{};
        return buildTerm(*token, m_now, node.term);
    }

    const QList<Token> &m_tokens;
    const QDateTime m_now;
    qsizetype m_position = 0;
};

bool compare(qint64 actual, Comparison comparison, qint64 expected)
{
    switch (comparison) {
    case Comparison::Less:
        return actual < expected;
    case Comparison::LessOrEqual:
        return actual <= expected;
    case Comparison::Greater:
        return actual > expected;
    case Comparison::GreaterOrEqual:
        return actual >= expected;
    case Comparison::Contains:
    case Comparison::Equal:
        break;
    }
    return actual == expected;
}

bool matchesText(const Term &term, const QString &value)
{
    if (!term.pattern.pattern().isEmpty()) {
        return term.pattern.match(value).hasMatch();
    }
    return term.comparison == Comparison::Equal ? value.compare(term.text, Qt::CaseInsensitive) == 0 : value.contains(term.text, Qt::CaseInsensitive);
}

bool matchesTerm(const Term &term, const QFileInfo &entry, const ContentMatcher &content, const TagMatcher &tags, const MetadataReader &metadata)
{
    switch (term.field) {
    case Field::Name:
        return matchesText(term, entry.fileName());
    case Field::Path:
        return matchesText(term, entry.absoluteFilePath());
    case Field::Type:
        switch (term.type) {
        case TypeMatch::File:
            return entry.isFile();
        case TypeMatch::Folder:
            return entry.isDir();
        case TypeMatch::Extension:
            return entry.isFile() && entry.fileName().endsWith(QLatin1Char('.') + term.text, Qt::CaseInsensitive);
        case TypeMatch::MimeType:
            return QMimeDatabase().mimeTypeForFile(entry).inherits(term.text);
        case TypeMatch::MimePrefix:
            return QMimeDatabase().mimeTypeForFile(entry).name().startsWith(term.text);
        }
        return false;
    case Field::Size:
        return entry.isFile() && compare(entry.size(), term.comparison, term.size);
    case Field::Modified: {
        const QDateTime modified = entry.lastModified();
        switch (term.comparison) {
        case Comparison::Less:
            return modified < term.from;
        case Comparison::LessOrEqual:
            return modified < term.to;
        case Comparison::Greater:
            return modified >= term.to;
        case Comparison::GreaterOrEqual:
            return modified >= term.from;
        case Comparison::Contains:
        case Comparison::Equal:
            break;
        }
        return modified >= term.from && modified < term.to;
    }
    case Field::Content:
        return entry.isFile() && content && content(term.text);
    case Field::Tag:
    case Field::Tagged:
        return tags && tags(term);
    case Field::Metadata: {
        if (!entry.isFile() || !metadata) {
            return false;
        }
        const QStringList texts = metadata();
        return std::any_of(texts.cbegin(), texts.cend(), [&term](const QString &text) {
            return matchesText(term, text);
        });
    }
    }
    return false;
}

bool evaluate(const Node &node, const QFileInfo &entry, const ContentMatcher &content, const TagMatcher &tags, const MetadataReader &metadata)
{
    switch (node.kind) {
    case Node::Kind::Term:
        return matchesTerm(node.term, entry, content, tags, metadata);
    case Node::Kind::Not:
        return !evaluate(node.children.front(), entry, content, tags, metadata);
    case Node::Kind::And:
    case Node::Kind::Or: {
        const bool all = node.kind == Node::Kind::And;
        // Check content and metadata last so cheap criteria can decide first; both read the file.
        for (const bool slowPass : {false, true}) {
            for (const Node &child : node.children) {
                const bool isSlow = child.kind == Node::Kind::Term && (child.term.field == Field::Content || child.term.field == Field::Metadata);
                if (isSlow != slowPass) {
                    continue;
                }
                if (evaluate(child, entry, content, tags, metadata) != all) {
                    return !all;
                }
            }
        }
        return all;
    }
    }
    return false;
}

void visitTerms(const Node &node, const std::function<void(const Term &)> &visit)
{
    if (node.kind == Node::Kind::Term) {
        visit(node.term);
    }
    for (const Node &child : node.children) {
        visitTerms(child, visit);
    }
}

QString describe(const Node &node)
{
    switch (node.kind) {
    case Node::Kind::Term: {
        static const char *const names[] = {"name", "path", "type", "size", "modified", "content", "tag", "tagged", "meta"};
        static const char *const operators[] = {":", "=", "<", "<=", ">", ">="};
        QString text = QString::fromLatin1(names[static_cast<int>(node.term.field)]) + QString::fromLatin1(operators[static_cast<int>(node.term.comparison)]);
        const bool prefix = node.term.field == Field::Type && node.term.type == TypeMatch::MimePrefix;
        return text + QLatin1Char('"') + node.term.text + (prefix ? QStringLiteral("*\"") : QStringLiteral("\""));
    }
    case Node::Kind::Not:
        return QStringLiteral("NOT ") + describe(node.children.front());
    case Node::Kind::And:
    case Node::Kind::Or: {
        QStringList parts;
        for (const Node &child : node.children) {
            parts.append(describe(child));
        }
        return QLatin1Char('(') + parts.join(node.kind == Node::Kind::And ? QStringLiteral(" AND ") : QStringLiteral(" OR ")) + QLatin1Char(')');
    }
    }
    return {};
}
}

Query Query::parse(const QString &text, const QDateTime &now)
{
    Query query;
    QList<Token> tokens;
    query.m_error = tokenize(text, tokens);
    if (query.m_error.isEmpty()) {
        query.m_error = Parser(tokens, now).parse(query.m_root);
    }
    return query;
}

bool Query::isValid() const
{
    return m_error.isEmpty();
}

QString Query::error() const
{
    return m_error;
}

bool Query::usesContent() const
{
    return !contentTerms().isEmpty();
}

bool Query::usesTags() const
{
    bool uses = false;
    if (isValid()) {
        visitTerms(m_root, [&uses](const Term &term) {
            uses = uses || term.field == Field::Tag || term.field == Field::Tagged;
        });
    }
    return uses;
}

bool Query::usesMetadata() const
{
    bool uses = false;
    if (isValid()) {
        visitTerms(m_root, [&uses](const Term &term) {
            uses = uses || term.field == Field::Metadata;
        });
    }
    return uses;
}

QStringList Query::contentTerms() const
{
    QStringList terms;
    if (!isValid()) {
        return terms;
    }
    visitTerms(m_root, [&terms](const Term &term) {
        if (term.field == Field::Content && !terms.contains(term.text, Qt::CaseInsensitive)) {
            terms.append(term.text);
        }
    });
    return terms;
}

QList<Term> Query::requiredTerms() const
{
    QList<Term> terms;
    if (!isValid()) {
        return terms;
    }
    const auto add = [&terms](const Node &node) {
        if (node.kind == Node::Kind::Term && (node.term.field == Field::Name || node.term.field == Field::Content) && node.term.pattern.pattern().isEmpty()) {
            terms.append(node.term);
        }
    };
    if (m_root.kind == Node::Kind::And) {
        for (const Node &child : m_root.children) {
            add(child);
        }
    } else {
        add(m_root);
    }
    return terms;
}

bool Query::matches(const QFileInfo &entry, const ContentMatcher &content, const TagMatcher &tags, const MetadataReader &metadata) const
{
    return isValid() && evaluate(m_root, entry, content, tags, metadata);
}

QString Query::toString() const
{
    return isValid() ? describe(m_root) : QString();
}
