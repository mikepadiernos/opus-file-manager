// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QSet>
#include <QStringList>
#include <Qt>

namespace InterfaceLanguage
{
// Applies an interface language override. An empty language follows Plasma:
// the session's LANGUAGE variable, or Plasma's saved language list when the
// process was started without it.
bool apply(const QString &language);
Qt::LayoutDirection layoutDirection(const QStringList &languages);
Qt::LayoutDirection layoutDirection(const QStringList &languages, int overrideDirection);

// Splits a colon-separated LANGUAGE value, dropping empty and repeated entries.
QStringList parseLanguageList(const QString &value);
// The language list saved by Plasma's Region & Language settings.
QStringList plasmaLanguages();
// The languages to follow without an override: the session's LANGUAGE
// variable when set, otherwise Plasma's saved list.
QStringList followedLanguages(const QString &environmentLanguage, const QStringList &plasma);
// The first requested language that Plasma Opus can show: one with an
// installed catalogue, matched exactly or by its base language, or English,
// which is the source text. Returns an empty string when none of the
// requested languages can be shown and the source text is used as a fallback.
QString effectiveLanguage(const QStringList &requested, const QSet<QString> &available);
}
