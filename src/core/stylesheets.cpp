// SPDX-License-Identifier: GPL-3.0-or-later

#include "stylesheets.h"

#include <KLocalizedString>

#include <QFile>
#include <QSaveFile>

namespace
{
QString atLine(int line, const QString &problem)
{
    return i18nc("@info %1 is a line number, %2 describes a problem", "Line %1: %2", line, problem);
}

bool startsWord(const QString &sheet, qsizetype index)
{
    if (index == 0) {
        return true;
    }
    const QChar previous = sheet.at(index - 1);
    return !previous.isLetterOrNumber() && previous != QLatin1Char('-') && previous != QLatin1Char('_');
}

bool matchesAt(const QString &sheet, qsizetype index, QLatin1StringView word)
{
    return startsWord(sheet, index) && QStringView(sheet).mid(index, word.size()).compare(word, Qt::CaseInsensitive) == 0;
}

int newlines(QStringView text)
{
    return int(text.count(QLatin1Char('\n')));
}
}

QString StyleSheets::check(const QString &sheet)
{
    if (sheet.size() > MaximumSize) {
        return i18ncp("@info", "The stylesheet is longer than %1 character.", "The stylesheet is longer than %1 characters.", MaximumSize);
    }
    int line = 1;
    int depth = 0;
    int parentheses = 0;
    int rules = 0;
    int ruleLine = 0;
    const qsizetype size = sheet.size();
    for (qsizetype i = 0; i < size; ++i) {
        const QChar ch = sheet.at(i);
        if (ch == QLatin1Char('\n')) {
            ++line;
            continue;
        }
        if (ch.category() == QChar::Other_Control && ch != QLatin1Char('\t') && ch != QLatin1Char('\r') && ch != QLatin1Char('\f')) {
            return atLine(line, i18nc("@info", "Control characters are not allowed."));
        }
        if (ch == QLatin1Char('/') && i + 1 < size && sheet.at(i + 1) == QLatin1Char('*')) {
            const qsizetype end = sheet.indexOf(QLatin1String("*/"), i + 2);
            if (end < 0) {
                return atLine(line, i18nc("@info", "A comment is not closed with */."));
            }
            line += newlines(QStringView(sheet).mid(i, end - i));
            i = end + 1;
            continue;
        }
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
            qsizetype end = i + 1;
            bool closed = false;
            for (; end < size && sheet.at(end) != QLatin1Char('\n'); ++end) {
                if (sheet.at(end) == QLatin1Char('\\')) {
                    ++end;
                } else if (sheet.at(end) == ch) {
                    closed = true;
                    break;
                }
            }
            if (!closed) {
                return atLine(line, i18nc("@info", "A quoted string is not closed."));
            }
            i = end;
            continue;
        }
        if (ch == QLatin1Char('@')) {
            return atLine(line, i18nc("@info", "At-rules such as @import are not supported by Qt Style Sheets."));
        }
        if (matchesAt(sheet, i, QLatin1StringView("qproperty-"))) {
            return atLine(line, i18nc("@info", "qproperty- declarations change widget behaviour and are not allowed."));
        }
        if (matchesAt(sheet, i, QLatin1StringView("url("))) {
            const qsizetype close = sheet.indexOf(QLatin1Char(')'), i + 4);
            if (close < 0) {
                return atLine(line, i18nc("@info", "A url( is not closed."));
            }
            QString target = sheet.mid(i + 4, close - i - 4).trimmed();
            if (target.size() >= 2 && (target.front() == QLatin1Char('"') || target.front() == QLatin1Char('\'')) && target.back() == target.front()) {
                target = target.mid(1, target.size() - 2).trimmed();
            }
            if (!target.startsWith(QLatin1String(":/")) && !target.startsWith(QLatin1String("qrc:/"), Qt::CaseInsensitive)) {
                return atLine(
                    line,
                    i18nc("@info %1 is a file or address", "Loading %1 is blocked. Only images built into Plasma Opus (:/…) can be used.", target.left(80)));
            }
            line += newlines(QStringView(sheet).mid(i, close - i));
            i = close;
            continue;
        }
        switch (ch.unicode()) {
        case '{':
            if (++depth > 1) {
                return atLine(line, i18nc("@info", "Rules cannot be nested."));
            }
            ruleLine = line;
            if (++rules > MaximumRules) {
                return atLine(line, i18ncp("@info", "A stylesheet can have at most %1 rule.", "A stylesheet can have at most %1 rules.", MaximumRules));
            }
            break;
        case '}':
            if (--depth < 0) {
                return atLine(line, i18nc("@info", "A } has no matching {."));
            }
            break;
        case '(':
            ++parentheses;
            break;
        case ')':
            if (--parentheses < 0) {
                return atLine(line, i18nc("@info", "A ) has no matching (."));
            }
            break;
        default:
            break;
        }
    }
    if (depth > 0) {
        return atLine(ruleLine, i18nc("@info", "A rule is not closed with }."));
    }
    if (parentheses > 0) {
        return atLine(line, i18nc("@info", "A ( is not closed."));
    }
    return {};
}

std::optional<QString> StyleSheets::readFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = i18nc("@info", "Could not open %1: %2", path, file.errorString());
        return std::nullopt;
    }
    if (file.size() > MaximumSize) {
        *error = i18ncp("@info", "The stylesheet is larger than %1 byte.", "The stylesheet is larger than %1 bytes.", MaximumSize);
        return std::nullopt;
    }
    const QByteArray data = file.read(MaximumSize + 1);
    if (data.size() > MaximumSize) {
        *error = i18ncp("@info", "The stylesheet is larger than %1 byte.", "The stylesheet is larger than %1 bytes.", MaximumSize);
        return std::nullopt;
    }
    QString sheet = QString::fromUtf8(data);
    if (sheet.contains(QChar::ReplacementCharacter) && !data.contains("\xEF\xBF\xBD")) {
        *error = i18nc("@info", "The stylesheet is not UTF-8 text.");
        return std::nullopt;
    }
    return sheet;
}

QString StyleSheets::writeFile(const QString &path, const QString &sheet)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(sheet.toUtf8()) < 0 || !file.commit()) {
        return i18nc("@info", "Could not write %1: %2", path, file.errorString());
    }
    return {};
}
