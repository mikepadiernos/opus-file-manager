// SPDX-License-Identifier: GPL-3.0-or-later

#include "batchrename.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
namespace Property = KFileMetaData::Property;

QString metadataValue(const KFileMetaData::PropertyMultiMap &metadata, const QString &token)
{
    Property::Property property = Property::Empty;
    if (token == QLatin1String("title")) {
        property = Property::Title;
    } else if (token == QLatin1String("artist")) {
        property = Property::Artist;
    } else if (token == QLatin1String("album")) {
        property = Property::Album;
    } else if (token == QLatin1String("year")) {
        property = Property::ReleaseYear;
    } else if (token == QLatin1String("date")) {
        property = Property::PhotoDateTimeOriginal;
    } else if (token == QLatin1String("camera")) {
        const QString make = metadata.value(Property::Manufacturer).toString().trimmed();
        const QString model = metadata.value(Property::Model).toString().trimmed();
        return QString(make + QLatin1Char(' ') + model).trimmed();
    }
    if (property == Property::Empty) {
        return {};
    }
    const QVariant value = metadata.value(property);
    if (value.metaType().id() == QMetaType::QDateTime) {
        return value.toDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    }
    if (value.metaType().id() == QMetaType::QDate) {
        return value.toDate().toString(Qt::ISODate);
    }
    return value.toString().trimmed();
}

bool validName(const QString &name)
{
    return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..") && !name.contains(QLatin1Char('/')) && !name.contains(QChar::Null);
}

bool exists(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

QString targetName(const BatchRename::Input &input, const QString &pattern, qint64 number, int padding, QString &error)
{
    const QFileInfo source(input.path);
    const QString fileName = source.fileName();
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    const bool hasExtension = !source.isDir() && dot > 0 && dot < fileName.size() - 1;
    const QString extension = hasExtension ? fileName.mid(dot + 1) : QString();
    const QString base = hasExtension ? fileName.left(dot) : fileName;
    const QSet<QString> metadataTokens{QStringLiteral("title"),
                                       QStringLiteral("artist"),
                                       QStringLiteral("album"),
                                       QStringLiteral("year"),
                                       QStringLiteral("date"),
                                       QStringLiteral("camera")};
    QString result;
    bool explicitExtension = false;
    for (qsizetype pos = 0; pos < pattern.size();) {
        if (pattern.at(pos) == QLatin1Char('}')) {
            error = i18nc("@info", "Unexpected closing brace in the rename pattern.");
            return {};
        }
        if (pattern.at(pos) != QLatin1Char('{')) {
            result += pattern.at(pos++);
            continue;
        }
        const qsizetype end = pattern.indexOf(QLatin1Char('}'), pos + 1);
        if (end < 0) {
            error = i18nc("@info", "Unclosed token in the rename pattern.");
            return {};
        }
        const QString token = pattern.mid(pos + 1, end - pos - 1);
        if (token == QLatin1String("name")) {
            result += base;
        } else if (token == QLatin1String("ext")) {
            result += extension;
            explicitExtension = true;
        } else if (token == QLatin1String("n")) {
            result += QString::number(number);
        } else if (token == QLatin1String("nn")) {
            result += QString::number(number).rightJustified(padding, QLatin1Char('0'));
        } else if (metadataTokens.contains(token)) {
            const QString value = metadataValue(input.metadata, token);
            if (value.isEmpty()) {
                error = i18nc("@info", "Metadata for token {%1} is unavailable.", token);
                return {};
            }
            result += value;
        } else {
            error = i18nc("@info", "Unknown rename token: {%1}", token);
            return {};
        }
        pos = end + 1;
    }
    if (hasExtension && !explicitExtension) {
        result += QLatin1Char('.') + extension;
    }
    if (!validName(result)) {
        error = i18nc("@info", "The resulting name is empty or contains an invalid character.");
    }
    return result;
}

QString regexReplacement(const QString &replacement, const QRegularExpression &expression, const QRegularExpressionMatch *match, QString &error)
{
    QString result;
    for (qsizetype i = 0; i < replacement.size(); ++i) {
        const QChar character = replacement.at(i);
        if (character != QLatin1Char('$') || i + 1 == replacement.size()) {
            result += character;
            continue;
        }
        if (replacement.at(i + 1) == QLatin1Char('$')) {
            result += QLatin1Char('$');
            ++i;
        } else if (replacement.at(i + 1) >= QLatin1Char('0') && replacement.at(i + 1) <= QLatin1Char('9')) {
            int group = replacement.at(++i).digitValue();
            if (i + 1 < replacement.size() && replacement.at(i + 1) >= QLatin1Char('0') && replacement.at(i + 1) <= QLatin1Char('9')) {
                group = group * 10 + replacement.at(++i).digitValue();
            }
            if (group > expression.captureCount()) {
                error = i18nc("@info", "Replacement refers to capture group $%1, which does not exist.", group);
                return {};
            }
            if (match) {
                result += match->captured(group);
            }
        } else {
            result += character;
        }
    }
    return result;
}

QString regexName(const BatchRename::Input &input, const QRegularExpression &expression, const QString &replacement, QString &error)
{
    const QFileInfo source(input.path);
    const QString fileName = source.fileName();
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    const bool hasExtension = !source.isDir() && dot > 0 && dot < fileName.size() - 1;
    const QString extension = hasExtension ? fileName.mid(dot) : QString();
    const QString base = hasExtension ? fileName.left(dot) : fileName;
    QString result;
    qsizetype last = 0;
    auto matches = expression.globalMatch(base);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        result += base.mid(last, match.capturedStart() - last);
        result += regexReplacement(replacement, expression, &match, error);
        if (!error.isEmpty()) {
            return {};
        }
        last = match.capturedEnd();
    }
    result += base.mid(last);
    result += extension;
    if (!validName(result)) {
        error = i18nc("@info", "The resulting name is empty or contains an invalid character.");
    }
    return result;
}
}

QString BatchRename::ruleError(const Rule &rule)
{
    if (rule.pattern.isEmpty() || rule.start < 0 || rule.start > 1000000000 || rule.padding < 1 || rule.padding > 16) {
        return i18nc("@info", "Enter a pattern, a nonnegative start number and a padding from 1 to 16.");
    }
    if (rule.regularExpression) {
        const QRegularExpression expression(rule.pattern);
        if (!expression.isValid()) {
            return i18nc("@info", "Invalid regular expression: %1", expression.errorString());
        }
        QString error;
        regexReplacement(rule.replacement, expression, nullptr, error);
        return error;
    }
    Input sample{QStringLiteral("/sample.txt"), {}};
    for (const Property::Property property :
         {Property::Title, Property::Artist, Property::Album, Property::ReleaseYear, Property::PhotoDateTimeOriginal, Property::Manufacturer}) {
        sample.metadata.insert(property, QStringLiteral("sample"));
    }
    QString error;
    targetName(sample, rule.pattern, rule.start, rule.padding, error);
    return error;
}

bool BatchRename::Plan::valid() const
{
    if (!error.isEmpty() || rows.isEmpty()) {
        return false;
    }
    for (const Row &row : rows) {
        if (!row.error.isEmpty()) {
            return false;
        }
    }
    return true;
}

BatchRename::Plan BatchRename::plan(const QList<Input> &inputs, const QString &pattern, int start, int padding)
{
    return plan(inputs, Rule{pattern, {}, false, start, padding});
}

BatchRename::Plan BatchRename::plan(const QList<Input> &inputs, const Rule &rule)
{
    Plan result;
    result.error = ruleError(rule);
    if (!result.error.isEmpty()) {
        return result;
    }
    if (inputs.isEmpty()) {
        result.error = i18nc("@info", "Select items to rename.");
        return result;
    }
    const QRegularExpression expression(rule.regularExpression ? rule.pattern : QString());
    QSet<QString> sources;
    QSet<QString> destinations;
    QSet<QString> sourceFolders;
    for (qsizetype i = 0; i < inputs.size(); ++i) {
        const Input &input = inputs.at(i);
        Row row;
        row.source = QDir::cleanPath(input.path);
        if (!QDir::isAbsolutePath(input.path) || row.source == QLatin1String("/") || !exists(row.source)) {
            row.error = i18nc("@info", "The source is not an existing local item: %1", input.path);
        } else if (sources.contains(row.source)) {
            row.error = i18nc("@info", "This source appears more than once.");
        } else {
            struct stat info;
            if (lstat(QFile::encodeName(row.source).constData(), &info) != 0) {
                row.error = i18nc("@info", "Could not inspect %1: %2", row.source, QString::fromLocal8Bit(std::strerror(errno)));
            } else {
                row.device = info.st_dev;
                row.inode = info.st_ino;
            }
            const QString name = rule.regularExpression ? regexName(input, expression, rule.replacement, row.error)
                                                        : targetName(input, rule.pattern, qint64(rule.start) + i, rule.padding, row.error);
            if (row.error.isEmpty()) {
                row.destination = QFileInfo(row.source).dir().absoluteFilePath(name);
                if (destinations.contains(row.destination)) {
                    row.error = i18nc("@info", "Another item has the same destination.");
                } else if (row.destination != row.source && exists(row.destination)) {
                    row.error = i18nc("@info", "The destination already exists.");
                }
                destinations.insert(row.destination);
            }
        }
        sources.insert(row.source);
        if (QFileInfo(row.source).isDir()) {
            sourceFolders.insert(row.source);
        }
        result.rows.append(row);
    }
    for (Row &row : result.rows) {
        QString parent = QFileInfo(row.source).absolutePath();
        while (parent != QLatin1String("/")) {
            if (sourceFolders.contains(parent)) {
                row.error = i18nc("@info", "Select either a folder or items inside it, not both.");
                break;
            }
            parent = QFileInfo(parent).absolutePath();
        }
    }
    return result;
}

BatchRename::Result BatchRename::apply(const Plan &plan, const std::function<bool()> &canceled)
{
    Result result;
    if (!plan.valid()) {
        result.error = i18nc("@info", "The rename preview contains errors.");
        return result;
    }
    QSet<QString> sources;
    QSet<QString> destinations;
    for (const Row &row : plan.rows) {
        const QFileInfo source(row.source);
        const QFileInfo destination(row.destination);
        struct stat info;
        const bool sameSource =
            lstat(QFile::encodeName(row.source).constData(), &info) == 0 && quint64(info.st_dev) == row.device && quint64(info.st_ino) == row.inode;
        if (!source.isAbsolute() || !destination.isAbsolute() || !validName(source.fileName()) || QDir::cleanPath(row.source) != row.source
            || QDir::cleanPath(row.destination) != row.destination || source.absolutePath() != destination.absolutePath() || !validName(destination.fileName())
            || !sameSource || sources.contains(row.source) || destinations.contains(row.destination)
            || (row.source != row.destination && exists(row.destination))) {
            result.error = i18nc("@info", "The rename preview is stale or contains invalid paths. Preview again.");
            return result;
        }
        sources.insert(row.source);
        destinations.insert(row.destination);
    }
    for (const Row &row : plan.rows) {
        QString parent = QFileInfo(row.source).absolutePath();
        while (parent != QLatin1String("/")) {
            if (sources.contains(parent)) {
                result.error = i18nc("@info", "Select either a folder or items inside it, not both.");
                return result;
            }
            parent = QFileInfo(parent).absolutePath();
        }
    }
    for (const Row &row : plan.rows) {
        if (canceled && canceled()) {
            result.error = i18nc("@info", "Rename canceled; listed items were already renamed.");
            return result;
        }
        if (row.source == row.destination) {
            continue;
        }
        const QByteArray source = QFile::encodeName(row.source);
        const QByteArray destination = QFile::encodeName(row.destination);
        struct stat info;
        if (lstat(source.constData(), &info) != 0 || quint64(info.st_dev) != row.device || quint64(info.st_ino) != row.inode) {
            result.error = i18nc("@info", "The source changed since the preview: %1", row.source);
            return result;
        }
        if (::renameat2(AT_FDCWD, source.constData(), AT_FDCWD, destination.constData(), RENAME_NOREPLACE) != 0) {
            result.error = i18nc("@info", "Could not rename %1: %2", row.source, QString::fromLocal8Bit(std::strerror(errno)));
            return result;
        }
        result.renamed.append(row.destination);
    }
    return result;
}
