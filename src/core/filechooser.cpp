// SPDX-License-Identifier: GPL-3.0-or-later

#include "filechooser.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>

namespace
{
QString decodePath(QByteArray bytes)
{
    while (bytes.endsWith('\0')) {
        bytes.chop(1);
    }
    return QFile::decodeName(bytes);
}

QByteArrayList byteArrayList(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        QByteArrayList list;
        const auto argument = value.value<QDBusArgument>();
        argument.beginArray();
        while (!argument.atEnd()) {
            QByteArray bytes;
            argument >> bytes;
            list.append(bytes);
        }
        argument.endArray();
        return list;
    }
    return value.value<QByteArrayList>();
}

template<typename T>
T demarshal(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        return qdbus_cast<T>(value.value<QDBusArgument>());
    }
    return value.value<T>();
}

QUrl localFolderUrl(const QVariant &value)
{
    const QString path = decodePath(value.toByteArray());
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}
}

namespace FileChooser
{
void registerTypes()
{
    qDBusRegisterMetaType<FilterRule>();
    qDBusRegisterMetaType<QList<FilterRule>>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<FilterList>();
    qDBusRegisterMetaType<ChoiceOption>();
    qDBusRegisterMetaType<QList<ChoiceOption>>();
    qDBusRegisterMetaType<Choice>();
    qDBusRegisterMetaType<ChoiceList>();
    qDBusRegisterMetaType<ChoiceValue>();
    qDBusRegisterMetaType<ChoiceValues>();
}

Request parseRequest(Mode mode, const QString &title, const QVariantMap &options)
{
    registerTypes();
    Request request;
    request.mode = mode;
    request.title = title;
    request.acceptLabel = qtMnemonic(options.value(QStringLiteral("accept_label")).toString());
    request.modal = options.value(QStringLiteral("modal"), true).toBool();
    if (mode == Mode::Open) {
        request.multiple = options.value(QStringLiteral("multiple")).toBool();
        request.directory = options.value(QStringLiteral("directory")).toBool();
    }
    if (mode != Mode::SaveFiles) {
        request.filters = demarshal<FilterList>(options.value(QStringLiteral("filters")));
        if (options.contains(QStringLiteral("current_filter"))) {
            const auto current = demarshal<Filter>(options.value(QStringLiteral("current_filter")));
            if (request.filters.isEmpty()) {
                request.fixedFilter = current;
            } else {
                request.currentFilter = request.filters.indexOf(current);
                if (request.currentFilter < 0) {
                    request.filters.append(current);
                    request.currentFilter = request.filters.size() - 1;
                }
            }
        }
        if (request.currentFilter < 0 && !request.filters.isEmpty()) {
            request.currentFilter = 0;
        }
    }
    request.choices = demarshal<ChoiceList>(options.value(QStringLiteral("choices")));
    request.currentFolder = localFolderUrl(options.value(QStringLiteral("current_folder")));
    if (mode == Mode::Save) {
        request.currentName = options.value(QStringLiteral("current_name")).toString();
        const QString file = decodePath(options.value(QStringLiteral("current_file")).toByteArray());
        if (!file.isEmpty()) {
            request.currentFile = QUrl::fromLocalFile(file);
        }
    }
    if (mode == Mode::SaveFiles) {
        const QByteArrayList files = byteArrayList(options.value(QStringLiteral("files")));
        for (const QByteArray &file : files) {
            const QString name = QFileInfo(decodePath(file)).fileName();
            if (!name.isEmpty()) {
                request.files.append(name);
            }
        }
    }
    return request;
}

QVariantMap results(const Request &request, const Result &result)
{
    registerTypes();
    QStringList uris;
    for (const QUrl &url : result.urls) {
        uris.append(QString::fromUtf8(url.toEncoded()));
    }
    QVariantMap map;
    map.insert(QStringLiteral("uris"), uris);
    if (!request.choices.isEmpty()) {
        map.insert(QStringLiteral("choices"), QVariant::fromValue(result.choices));
    }
    if (result.filter >= 0 && result.filter < request.filters.size()) {
        map.insert(QStringLiteral("current_filter"), QVariant::fromValue(request.filters.at(result.filter)));
    }
    if (request.mode == Mode::Open) {
        map.insert(QStringLiteral("writable"), true);
    }
    return map;
}

QString qtMnemonic(const QString &label)
{
    QString converted;
    converted.reserve(label.size());
    for (qsizetype i = 0; i < label.size(); ++i) {
        const QChar c = label.at(i);
        if (c == QLatin1Char('&')) {
            converted += QStringLiteral("&&");
        } else if (c == QLatin1Char('_')) {
            if (i + 1 < label.size() && label.at(i + 1) == QLatin1Char('_')) {
                converted += QLatin1Char('_');
                ++i;
            } else {
                converted += QLatin1Char('&');
            }
        } else {
            converted += c;
        }
    }
    return converted;
}

QStringList globPatterns(const Filter &filter)
{
    QStringList globs;
    const QMimeDatabase database;
    for (const FilterRule &rule : filter.rules) {
        if (rule.type == 0) {
            globs.append(rule.pattern);
            continue;
        }
        if (rule.pattern == QLatin1String("application/octet-stream") || rule.pattern == QLatin1String("*/*")) {
            globs.append(QStringLiteral("*"));
            continue;
        }
        const bool wildcard = rule.pattern.endsWith(QLatin1String("/*"));
        const QString prefix = rule.pattern.chopped(1);
        const QMimeType named = database.mimeTypeForName(rule.pattern);
        for (const QMimeType &type : database.allMimeTypes()) {
            const bool matches = wildcard ? type.name().startsWith(prefix) : (named.isValid() && type.inherits(named.name()));
            if (matches) {
                globs.append(type.globPatterns());
            }
        }
    }
    globs.removeDuplicates();
    return globs;
}

QString withFilterExtension(const QString &name, const QStringList &globs)
{
    if (name.trimmed().isEmpty() || !QFileInfo(name).suffix().isEmpty()) {
        return name;
    }
    static const QRegularExpression extension(QStringLiteral("^\\*(\\.[^*?\\[\\]/]+)$"));
    for (const QString &glob : globs) {
        const QRegularExpressionMatch match = extension.match(glob);
        if (match.hasMatch()) {
            return name + match.captured(1);
        }
        if (QRegularExpression(QRegularExpression::wildcardToRegularExpression(glob)).match(name).hasMatch()) {
            return name;
        }
    }
    return name;
}

QUrl saveTarget(const QUrl &folder, const QString &name)
{
    if (name.trimmed().isEmpty()) {
        return {};
    }
    if (name.startsWith(QLatin1String("~/"))) {
        return QUrl::fromLocalFile(QDir::cleanPath(QDir::homePath() + name.mid(1)));
    }
    if (QDir::isAbsolutePath(name)) {
        return QUrl::fromLocalFile(QDir::cleanPath(name));
    }
    if (folder.isLocalFile()) {
        return QUrl::fromLocalFile(QDir::cleanPath(QDir(folder.toLocalFile()).filePath(name)));
    }
    QUrl target = folder;
    target.setPath(QDir::cleanPath(folder.path() + QLatin1Char('/') + name));
    return target;
}

QList<QUrl> saveFilesTargets(const QUrl &folder, const QStringList &names)
{
    QList<QUrl> targets;
    for (const QString &name : names) {
        targets.append(saveTarget(folder, QFileInfo(name).fileName()));
    }
    return targets;
}

QUrl initialFolder(const Request &request)
{
    if (request.currentFolder.isValid()) {
        return request.currentFolder;
    }
    if (request.currentFile.isValid()) {
        return request.currentFile.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    }
    return {};
}

QString initialName(const Request &request)
{
    if (!request.currentName.isEmpty()) {
        return request.currentName;
    }
    return request.currentFile.fileName();
}
}

using namespace FileChooser;

QDBusArgument &operator<<(QDBusArgument &argument, const FilterRule &rule)
{
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, FilterRule &rule)
{
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Filter &filter)
{
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Filter &filter)
{
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ChoiceOption &option)
{
    argument.beginStructure();
    argument << option.id << option.label;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ChoiceOption &option)
{
    argument.beginStructure();
    argument >> option.id >> option.label;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Choice &choice)
{
    argument.beginStructure();
    argument << choice.id << choice.label << choice.options << choice.initial;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Choice &choice)
{
    argument.beginStructure();
    argument >> choice.id >> choice.label >> choice.options >> choice.initial;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ChoiceValue &value)
{
    argument.beginStructure();
    argument << value.id << value.value;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ChoiceValue &value)
{
    argument.beginStructure();
    argument >> value.id >> value.value;
    argument.endStructure();
    return argument;
}
