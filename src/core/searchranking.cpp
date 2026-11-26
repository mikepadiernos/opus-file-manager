// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchranking.h"

#include "searchquery.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <utility>

namespace
{
// The optimal string alignment distance: edits, counting an adjacent transposition as one.
int distance(const QString &a, const QString &b)
{
    QList<int> beforePrevious(b.size() + 1);
    QList<int> previous(b.size() + 1);
    QList<int> current(b.size() + 1);
    for (qsizetype j = 0; j <= b.size(); ++j) {
        previous[j] = int(j);
    }
    for (qsizetype i = 1; i <= a.size(); ++i) {
        current[0] = int(i);
        for (qsizetype j = 1; j <= b.size(); ++j) {
            const int substitution = previous[j - 1] + (a.at(i - 1) == b.at(j - 1) ? 0 : 1);
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1, substitution});
            if (i > 1 && j > 1 && a.at(i - 1) == b.at(j - 2) && a.at(i - 2) == b.at(j - 1)) {
                current[j] = std::min(current[j], beforePrevious[j - 2] + 1);
            }
        }
        std::swap(beforePrevious, previous);
        std::swap(previous, current);
    }
    return previous[b.size()];
}

// Within a tier, terms covering more of the name rank higher.
int coverage(const QString &name, const QString &term)
{
    return int(99 * term.size() / std::max<qsizetype>(name.size(), 1));
}

bool isWordStart(const QString &original, qsizetype index)
{
    if (index == 0) {
        return true;
    }
    const QChar before = original.at(index - 1);
    const QChar at = original.at(index);
    return !before.isLetterOrNumber() || (before.isLower() && at.isUpper()) || (before.isLetter() != at.isLetter());
}
}

int SearchRanking::score(const QString &name, const QString &term)
{
    const QString needle = term.trimmed().toCaseFolded();
    if (needle.isEmpty() || name.isEmpty()) {
        return 0;
    }
    const QString folded = name.toCaseFolded();
    const QFileInfo info(name);
    const QString base = info.completeBaseName().toCaseFolded();
    if (folded == needle || base == needle || info.baseName().toCaseFolded() == needle) {
        return 1000;
    }
    if (folded.startsWith(needle)) {
        return 800 + coverage(folded, needle);
    }
    int best = 0;
    for (qsizetype index = folded.indexOf(needle); index >= 0; index = folded.indexOf(needle, index + 1)) {
        best = std::max(best, (isWordStart(name, index) ? 600 : 400) + coverage(folded, needle));
    }
    if (best > 0) {
        return best;
    }
    if (needle.size() >= 3) {
        const int allowed = std::max<int>(1, int(needle.size() / 4));
        QStringList words = base.split(QRegularExpression(QStringLiteral("[^\\w]+|_")), Qt::SkipEmptyParts);
        words.prepend(base);
        int closest = allowed + 1;
        for (const QString &word : std::as_const(words)) {
            if (std::abs(word.size() - needle.size()) <= allowed) {
                closest = std::min(closest, distance(word, needle));
            }
        }
        if (closest <= allowed) {
            return 300 - 10 * (closest - 1);
        }
    }
    qsizetype position = 0;
    for (const QChar character : needle) {
        position = folded.indexOf(character, position);
        if (position < 0) {
            return 0;
        }
        ++position;
    }
    return 100 + coverage(folded, needle);
}

QStringList SearchRanking::terms(const QString &text, LiveSearch::Field field)
{
    if (field != LiveSearch::Field::Query) {
        const QString trimmed = text.trimmed();
        return trimmed.isEmpty() ? QStringList() : QStringList{trimmed};
    }
    const SearchQuery::Query query = SearchQuery::Query::parse(text);
    QStringList result;
    if (!query.isValid()) {
        return result;
    }
    const QList<SearchQuery::Term> required = query.requiredTerms();
    for (const SearchQuery::Term &term : required) {
        if (!term.text.isEmpty() && !result.contains(term.text, Qt::CaseInsensitive)) {
            result.append(term.text);
        }
    }
    return result;
}

QStringList SearchRanking::rank(const QStringList &paths, const QStringList &terms)
{
    if (terms.isEmpty()) {
        return paths;
    }
    QList<std::pair<int, QString>> scored;
    scored.reserve(paths.size());
    for (const QString &path : paths) {
        const QString name = QFileInfo(QDir::cleanPath(path)).fileName();
        int best = 0;
        for (const QString &term : terms) {
            best = std::max(best, score(name, term));
        }
        scored.append({best, path});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto &left, const auto &right) {
        return left.first > right.first;
    });
    QStringList result;
    result.reserve(scored.size());
    for (const auto &[value, path] : std::as_const(scored)) {
        Q_UNUSED(value)
        result.append(path);
    }
    return result;
}
