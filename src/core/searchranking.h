// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "livesearch.h"

#include <QString>
#include <QStringList>

/**
 * Orders search results by how closely their names resemble the search
 * terms. Ranking only reorders the results a search found; it never adds or
 * removes matches.
 */
namespace SearchRanking
{
/**
 * Scores how closely @p name, a file name, resembles @p term, ignoring case:
 * an exact match of the name or its base name scores 1000, a prefix 800, the
 * start of a word 600, elsewhere in the name 400, a near spelling of the base
 * name or one of its words 300, and the term's letters in order from 100.
 * Unrelated names score 0.
 */
int score(const QString &name, const QString &term);
/**
 * The terms used to rank the results of a search for @p text: the required
 * name, content and metadata values of a query, or the trimmed text itself.
 */
QStringList terms(const QString &text, LiveSearch::Field field);
/**
 * Sorts @p paths by the highest score() of their file names over @p terms,
 * best first; ties and unrelated names keep their order.
 */
QStringList rank(const QStringList &paths, const QStringList &terms);
}
