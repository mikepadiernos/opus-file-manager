// SPDX-License-Identifier: GPL-3.0-or-later

#include "customcommands.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace
{
const QRegularExpression KeyPattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
const QRegularExpression PlaceholderPattern(QStringLiteral("^\\{([A-Za-z_][A-Za-z0-9_]*)\\}$"));

bool validValue(const QString &value)
{
    return !value.contains(QChar::Null);
}

bool representable(const QString &value)
{
    return validValue(value) && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r'));
}

bool reserved(const QString &key)
{
    return key == QLatin1String("path") || key == QLatin1String("paths") || key == QLatin1String("dir");
}
}

QString CustomCommands::validate(const Command &command)
{
    if (command.program.trimmed().isEmpty() || !representable(command.program)) {
        return i18nc("@info", "Enter an executable or script path.");
    }
    if (command.arguments.size() == 1 && command.arguments.first().isEmpty()) {
        return i18nc("@info", "A single empty argument cannot be represented in the command editor.");
    }
    for (auto it = command.defaults.cbegin(); it != command.defaults.cend(); ++it) {
        if (!KeyPattern.match(it.key()).hasMatch() || reserved(it.key()) || !representable(it.value())) {
            return i18nc("@info", "Invalid or reserved placeholder name: %1", it.key());
        }
    }
    for (const QString &argument : command.arguments) {
        if (!representable(argument)) {
            return i18nc("@info", "Saved argument templates cannot contain NUL or line-break characters.");
        }
        if ((argument.contains(QStringLiteral("{path}")) || argument.contains(QStringLiteral("{paths}")) || argument.contains(QStringLiteral("{dir}")))
            && argument != QLatin1String("{path}") && argument != QLatin1String("{paths}") && argument != QLatin1String("{dir}")) {
            return i18nc("@info", "Path placeholders must each occupy an entire argument.");
        }
    }
    return {};
}

CustomCommands::Prepared CustomCommands::prepare(const Command &command, const QString &folder, const QStringList &selection)
{
    Prepared result;
    result.error = validate(command);
    if (!result.error.isEmpty()) {
        return result;
    }
    if (!QDir::isAbsolutePath(folder) || !QFileInfo(folder).isDir()) {
        result.error = i18nc("@info", "Choose an existing local working folder.");
        return result;
    }
    for (const QString &path : selection) {
        if (!QDir::isAbsolutePath(path) || !validValue(path)) {
            result.error = i18nc("@info", "Selected items must have local absolute paths.");
            return result;
        }
    }
    result.program = command.program;
    result.workingDirectory = folder;
    for (const QString &argument : command.arguments) {
        if (argument == QLatin1String("{dir}") || argument == QLatin1String("%d")) {
            result.arguments.append(folder);
        } else if (argument == QLatin1String("{path}") || argument == QLatin1String("%f")) {
            if (selection.isEmpty()) {
                result.error = i18nc("@info", "The command requires a selected item.");
                return result;
            }
            result.arguments.append(selection.first());
        } else if (argument == QLatin1String("{paths}") || argument == QLatin1String("%F")) {
            if (selection.isEmpty()) {
                result.error = i18nc("@info", "The command requires selected items.");
                return result;
            }
            result.arguments.append(selection);
        } else {
            const auto match = PlaceholderPattern.match(argument);
            if (match.hasMatch()) {
                const QString key = match.captured(1);
                if (!command.defaults.contains(key)) {
                    result.error = i18nc("@info", "No default is defined for placeholder {%1}.", key);
                    return result;
                }
                result.arguments.append(command.defaults.value(key));
            } else {
                result.arguments.append(argument);
            }
        }
    }
    return result;
}

QList<CustomCommands::Command> CustomCommands::load(const KConfigGroup &group, QString *error)
{
    if (error) {
        error->clear();
    }
    const QByteArray json = group.readEntry("Commands", QByteArray());
    if (json.isEmpty()) {
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    QList<Command> commands;
    QSet<QString> names;
    if (parseError.error == QJsonParseError::NoError && document.isArray()) {
        for (const QJsonValue &value : document.array()) {
            const auto item = value.toObject();
            const auto arguments = item.value(QStringLiteral("arguments"));
            const auto defaults = item.value(QStringLiteral("defaults"));
            Command command{item.value(QStringLiteral("name")).toString(), item.value(QStringLiteral("program")).toString(), {}, {}};
            bool validFields = value.isObject() && item.value(QStringLiteral("name")).isString() && item.value(QStringLiteral("program")).isString()
                && arguments.isArray() && defaults.isObject();
            for (const QJsonValue &argument : arguments.toArray()) {
                validFields = validFields && argument.isString();
                command.arguments.append(argument.toString());
            }
            const QJsonObject values = defaults.toObject();
            for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
                validFields = validFields && it.value().isString();
                command.defaults.insert(it.key(), it.value().toString());
            }
            const QString key = command.name.trimmed().toCaseFolded();
            if (!validFields || key.isEmpty() || !representable(command.name) || names.contains(key) || !validate(command).isEmpty()) {
                commands.clear();
                break;
            }
            names.insert(key);
            commands.append(command);
        }
        if (commands.size() == document.array().size()) {
            return commands;
        }
    }
    if (error) {
        *error = i18nc("@info", "Saved commands are invalid and could not be loaded.");
    }
    return {};
}

bool CustomCommands::save(KConfigGroup group, const QList<Command> &commands, QString *error)
{
    if (error) {
        error->clear();
    }
    QJsonArray entries;
    QSet<QString> names;
    for (const Command &command : commands) {
        const QString key = command.name.trimmed().toCaseFolded();
        if (key.isEmpty() || !representable(command.name) || names.contains(key) || !validate(command).isEmpty()) {
            if (error) {
                *error = i18nc("@info", "Command names must be unique and each command must be valid.");
            }
            return false;
        }
        names.insert(key);
        QJsonObject values;
        for (auto it = command.defaults.cbegin(); it != command.defaults.cend(); ++it) {
            values.insert(it.key(), it.value());
        }
        QJsonObject item;
        item.insert(QStringLiteral("name"), command.name);
        item.insert(QStringLiteral("program"), command.program);
        item.insert(QStringLiteral("arguments"), QJsonArray::fromStringList(command.arguments));
        item.insert(QStringLiteral("defaults"), values);
        entries.append(item);
    }
    const QByteArray previous = group.readEntry("Commands", QByteArray());
    group.writeEntry("Commands", QJsonDocument(entries).toJson(QJsonDocument::Compact));
    if (!group.sync()) {
        group.writeEntry("Commands", previous);
        if (error) {
            *error = i18nc("@info", "Could not save commands to configuration.");
        }
        return false;
    }
    return true;
}
