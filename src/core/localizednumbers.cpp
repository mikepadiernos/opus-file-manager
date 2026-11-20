// SPDX-License-Identifier: GPL-3.0-or-later

#include "localizednumbers.h"

#include "settings.h"

#include <QSet>

bool LocalizedNumbers::validCustomNumerals(const QString &digits)
{
    const auto characters = digits.toUcs4();
    if (characters.size() != 10) {
        return false;
    }
    QSet<char32_t> unique;
    for (char32_t character : characters) {
        if (QChar::isSpace(character) || !QChar::isPrint(character)) {
            return false;
        }
        unique.insert(character);
    }
    return unique.size() == 10;
}

QString LocalizedNumbers::shape(const QString &number)
{
    const int style = Settings::numeralStyle();
    if (style == Settings::EnumNumeralStyle::LocaleDigits) {
        return number;
    }
    QString digits;
    if (style == Settings::EnumNumeralStyle::WesternDigits) {
        digits = QStringLiteral("0123456789");
    } else if (style == Settings::EnumNumeralStyle::EasternArabicIndicDigits) {
        digits = QString::fromUtf8("۰۱۲۳۴۵۶۷۸۹");
    } else {
        digits = Settings::customNumerals();
    }
    if (!validCustomNumerals(digits)) {
        qWarning("Invalid custom numeral symbols");
        return number;
    }
    const auto symbols = digits.toUcs4();
    QString result;
    for (char32_t character : number.toUcs4()) {
        const int digit = QChar::digitValue(character);
        if (digit >= 0 && digit <= 9) {
            character = symbols.at(digit);
        }
        result.append(QString::fromUcs4(&character, 1));
    }
    return result;
}
