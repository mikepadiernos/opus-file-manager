// SPDX-License-Identifier: GPL-3.0-or-later

#include "interfacelanguage.h"

#include "settings.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include <QLocale>

#include <clocale>

namespace
{
bool isEnglish(const QString &language)
{
    return language == QLatin1String("en") || language.startsWith(QLatin1String("en_")) || language.startsWith(QLatin1String("en@"));
}

// gettext ignores LANGUAGE while the message locale is C, so select any
// installed UTF-8 locale before using a translation.
bool ensureMessageLocale(const QString &language)
{
    const QByteArray current(std::setlocale(LC_MESSAGES, nullptr));
    if (language != QLatin1String("en_US") && (current == "C" || current == "POSIX" || current.startsWith("C."))) {
        QStringList candidates{QLocale(language).name() + QStringLiteral(".UTF-8"), QStringLiteral("en_US.UTF-8"), QStringLiteral("en_GB.UTF-8")};
        for (const QLocale &locale : QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory)) {
            if (locale.language() != QLocale::C) {
                candidates.append(locale.name() + QStringLiteral(".UTF-8"));
            }
        }
        bool available = false;
        for (const QString &candidate : candidates) {
            const char *selected = std::setlocale(LC_MESSAGES, candidate.toLocal8Bit().constData());
            const QByteArray selectedName(selected);
            if (selected && selectedName != "C" && selectedName != "POSIX" && !selectedName.startsWith("C.")) {
                available = true;
                break;
            }
        }
        if (!available) {
            std::setlocale(LC_MESSAGES, current.constData());
            return false;
        }
    }
    return true;
}
}

bool InterfaceLanguage::apply(const QString &language)
{
    if (language.isEmpty()) {
        std::setlocale(LC_MESSAGES, "");
        KLocalizedString::clearLanguages();
        const QString environment = qEnvironmentVariable("LANGUAGE");
        if (environment.isEmpty()) {
            const QStringList plasma = plasmaLanguages();
            if (!plasma.isEmpty() && ensureMessageLocale(plasma.first())) {
                KLocalizedString::setLanguages(plasma);
            }
        }
        return true;
    }
    if (!ensureMessageLocale(language)) {
        return false;
    }
    KLocalizedString::setLanguages({language});
    return true;
}

QStringList InterfaceLanguage::parseLanguageList(const QString &value)
{
    QStringList languages;
    const QStringList parts = value.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString language = part.trimmed();
        if (!language.isEmpty() && !languages.contains(language)) {
            languages.append(language);
        }
    }
    return languages;
}

QStringList InterfaceLanguage::plasmaLanguages()
{
    const KConfig config(QStringLiteral("plasma-localerc"), KConfig::NoGlobals);
    return parseLanguageList(config.group(QStringLiteral("Translations")).readEntry("LANGUAGE", QString()));
}

QStringList InterfaceLanguage::followedLanguages(const QString &environmentLanguage, const QStringList &plasma)
{
    const QStringList environment = parseLanguageList(environmentLanguage);
    return environment.isEmpty() ? plasma : environment;
}

QString InterfaceLanguage::effectiveLanguage(const QStringList &requested, const QSet<QString> &available)
{
    for (const QString &language : requested) {
        if (isEnglish(language)) {
            return language;
        }
        if (available.contains(language)) {
            return language;
        }
        QString base = language.section(QLatin1Char('@'), 0, 0);
        if (available.contains(base)) {
            return base;
        }
        base = base.section(QLatin1Char('_'), 0, 0);
        if (available.contains(base)) {
            return base;
        }
    }
    return QString();
}

Qt::LayoutDirection InterfaceLanguage::layoutDirection(const QStringList &languages)
{
    return languages.isEmpty() ? QLocale::system().textDirection() : QLocale(languages.first()).textDirection();
}

Qt::LayoutDirection InterfaceLanguage::layoutDirection(const QStringList &languages, int overrideDirection)
{
    if (overrideDirection == Settings::EnumTranslationDirection::LeftToRight) {
        return Qt::LeftToRight;
    }
    if (overrideDirection == Settings::EnumTranslationDirection::RightToLeft) {
        return Qt::RightToLeft;
    }
    return layoutDirection(languages);
}
