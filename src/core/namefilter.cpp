// SPDX-License-Identifier: GPL-3.0-or-later

#include "namefilter.h"

#include <algorithm>

NameFilter::NameFilter(const QString &text, Scope scope)
    : m_text(text)
    , m_scope(scope)
{
    const QString trimmed = text.trimmed();
    const bool isGlob = std::any_of(trimmed.cbegin(), trimmed.cend(), [](QChar c) {
        return c == u'*' || c == u'?' || c == u'[';
    });
    if (!isGlob) {
        return;
    }
    static const QRegularExpression separators(QStringLiteral("[\\s,;]+"));
    const QStringList globs = trimmed.split(separators, Qt::SkipEmptyParts);
    for (const QString &glob : globs) {
        m_patterns.append(QRegularExpression(QRegularExpression::wildcardToRegularExpression(glob), QRegularExpression::CaseInsensitiveOption));
    }
}

QString NameFilter::text() const
{
    return m_text;
}

NameFilter::Scope NameFilter::scope() const
{
    return m_scope;
}

bool NameFilter::isEmpty() const
{
    return m_text.trimmed().isEmpty();
}

bool NameFilter::appliesTo(bool isFolder) const
{
    return !isEmpty() && !(m_scope == Scope::Files && isFolder) && !(m_scope == Scope::Folders && !isFolder);
}

bool NameFilter::matches(const QString &name, bool isFolder) const
{
    return matches(name, isFolder, {}, 0);
}

bool NameFilter::matches(const QString &name, bool isFolder, const QString &label, int rating) const
{
    if (!appliesTo(isFolder)) {
        return true;
    }
    const QString trimmed = m_text.trimmed();
    if (!isFolder && trimmed.startsWith(u'#')) {
        return !label.isEmpty() && label.contains(trimmed.mid(1), Qt::CaseInsensitive);
    }
    if (!isFolder && trimmed.size() > 1 && trimmed.at(0) == u'*') {
        bool ok = false;
        const int minimumRating = trimmed.mid(1).toInt(&ok);
        if (ok && minimumRating >= 1 && minimumRating <= 5) {
            return rating >= minimumRating;
        }
    }
    if (m_patterns.isEmpty()) {
        return name.contains(trimmed, Qt::CaseInsensitive);
    }
    return std::any_of(m_patterns.cbegin(), m_patterns.cend(), [&name](const QRegularExpression &pattern) {
        return pattern.match(name).hasMatch();
    });
}

bool NameFilter::operator==(const NameFilter &other) const
{
    return m_text == other.m_text && m_scope == other.m_scope;
}
