// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardhints.h"

#include <algorithm>
#include <numeric>

namespace KeyboardHints
{

namespace
{

QString normalized(const QString &alphabet)
{
    QString letters;
    for (const QChar letter : alphabet) {
        const QChar lower = letter.toLower();
        if (!lower.isSpace() && lower.isPrint() && !letters.contains(lower)) {
            letters.append(lower);
        }
    }
    return letters;
}

// The sequence at @p index among all sequences of @p length, in alphabet order.
QString sequence(qint64 index, int length, const QString &letters)
{
    const qint64 base = letters.size();
    QString result(length, QChar());
    for (int position = length - 1; position >= 0; --position) {
        result[position] = letters.at(index % base);
        index /= base;
    }
    return result;
}

} // namespace

QString alphabet(Layout layout)
{
    switch (layout) {
    case Layout::Azerty:
        return QStringLiteral("fjdkslqmgh");
    case Layout::Dvorak:
        return QStringLiteral("uhetonasid");
    case Layout::Colemak:
        return QStringLiteral("tnseriaodh");
    case Layout::Qwerty:
        break;
    }
    return QStringLiteral("fjdkslagh");
}

QStringList labels(int count, const QString &alphabet)
{
    const QString letters = normalized(alphabet);
    const qint64 base = letters.size();
    QStringList result;
    if (count <= 0) {
        return result;
    }
    if (count <= base) {
        for (int i = 0; i < count; ++i) {
            result.append(letters.at(i));
        }
        return result;
    }
    if (base < 2) {
        return result;
    }

    // The fewest keys come from a complete tree: every label is `length` or
    // `length - 1` keys long. Expanding a shorter sequence into `base` longer
    // ones gains `base - 1` labels, so expand just enough of the last ones.
    int length = 1;
    qint64 shorter = 1;
    while (shorter * base < count) {
        shorter *= base;
        ++length;
    }
    const qint64 expanded = (count - shorter + base - 2) / (base - 1);
    const qint64 kept = shorter - expanded;
    result.reserve(count);
    for (qint64 i = 0; i < kept; ++i) {
        result.append(sequence(i, length - 1, letters));
    }
    for (qint64 i = kept; i < shorter && result.size() < count; ++i) {
        const QString prefix = sequence(i, length - 1, letters);
        for (qint64 letter = 0; letter < base && result.size() < count; ++letter) {
            result.append(prefix + letters.at(letter));
        }
    }
    return result;
}

QStringList assign(const QList<Kind> &kinds, const QString &alphabet)
{
    QList<int> order(kinds.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&kinds](int left, int right) {
        return kinds.at(left) < kinds.at(right);
    });
    const QStringList generated = labels(kinds.size(), alphabet);
    QStringList result(kinds.size());
    for (int i = 0; i < generated.size(); ++i) {
        result[order.at(i)] = generated.at(i);
    }
    return result;
}

Matcher::Matcher(const QStringList &labels)
    : m_labels(labels)
{
}

Matcher::Result Matcher::feed(QChar key)
{
    const QString candidate = m_typed + key.toLower();
    bool pending = false;
    for (int i = 0; i < m_labels.size(); ++i) {
        const QString &label = m_labels.at(i);
        if (label.isEmpty() || !label.startsWith(candidate)) {
            continue;
        }
        if (label.size() == candidate.size()) {
            m_typed = candidate;
            m_matched = i;
            return Result::Matched;
        }
        pending = true;
    }
    if (!pending) {
        return Result::NoMatch;
    }
    m_typed = candidate;
    m_matched = -1;
    return Result::Pending;
}

bool Matcher::backspace()
{
    if (m_typed.isEmpty()) {
        return false;
    }
    m_typed.chop(1);
    m_matched = -1;
    return true;
}

void Matcher::reset()
{
    m_typed.clear();
    m_matched = -1;
}

QString Matcher::typed() const
{
    return m_typed;
}

int Matcher::matched() const
{
    return m_matched;
}

bool Matcher::isCandidate(int index) const
{
    if (index < 0 || index >= m_labels.size()) {
        return false;
    }
    const QString &label = m_labels.at(index);
    return !label.isEmpty() && label.startsWith(m_typed);
}

int Matcher::candidateCount() const
{
    int count = 0;
    for (int i = 0; i < m_labels.size(); ++i) {
        count += isCandidate(i) ? 1 : 0;
    }
    return count;
}

} // namespace KeyboardHints
