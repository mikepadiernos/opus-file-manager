// SPDX-License-Identifier: GPL-3.0-or-later

#include "interfacelanguage.h"

#include "settings.h"

#include <KLocalizedString>

#include <QLocale>

#include <clocale>

bool InterfaceLanguage::apply(const QString &language)
{
    if (language.isEmpty()) {
        std::setlocale(LC_MESSAGES, "");
        KLocalizedString::clearLanguages();
        return true;
    }
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
    KLocalizedString::setLanguages({language});
    return true;
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
