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
    if (!appliesTo(isFolder)) {
        return true;
    }
    if (m_patterns.isEmpty()) {
        return name.contains(m_text.trimmed(), Qt::CaseInsensitive);
    }
    return std::any_of(m_patterns.cbegin(), m_patterns.cend(), [&name](const QRegularExpression &pattern) {
        return pattern.match(name).hasMatch();
    });
}

bool NameFilter::operator==(const NameFilter &other) const
{
    return m_text == other.m_text && m_scope == other.m_scope;
}
