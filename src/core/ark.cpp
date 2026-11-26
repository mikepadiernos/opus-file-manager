// SPDX-License-Identifier: GPL-3.0-or-later

#include "ark.h"

#include <KPluginMetaData>

#include <QJsonArray>
#include <QMimeDatabase>
#include <QStandardPaths>

#include <algorithm>

namespace
{
QStringList stringList(const QJsonValue &value)
{
    QStringList list;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        list.append(entry.toString());
    }
    return list;
}

bool allFound(const QStringList &executables, const std::function<bool(const QString &)> &hasExecutable)
{
    return std::all_of(executables.cbegin(), executables.cend(), hasExecutable);
}
}

namespace Ark
{
Backends Backends::fromPlugins(const QList<QJsonObject> &plugins, const std::function<bool(const QString &)> &hasExecutable)
{
    Backends backends;
    for (const QJsonObject &plugin : plugins) {
        const QJsonObject info = plugin.value(QLatin1String("KPlugin")).toObject();
        if (!info.value(QLatin1String("EnabledByDefault")).toBool(true)) {
            continue;
        }
        if (!allFound(stringList(plugin.value(QLatin1String("X-KDE-Kerfuffle-ReadOnlyExecutables"))), hasExecutable)) {
            continue;
        }
        const bool writes = plugin.value(QLatin1String("X-KDE-Kerfuffle-ReadWrite")).toBool()
            && allFound(stringList(plugin.value(QLatin1String("X-KDE-Kerfuffle-ReadWriteExecutables"))), hasExecutable);
        const QString id = info.value(QLatin1String("Id")).toString();
        const QStringList mimeTypes = stringList(info.value(QLatin1String("MimeTypes")));
        for (const QString &mimeType : mimeTypes) {
            const QJsonObject details = plugin.value(mimeType).toObject();
            FormatSupport &format = backends.m_formats[mimeType];
            format.read = true;
            format.plugins.append(id);
            if (!writes) {
                continue;
            }
            format.write = true;
            format.encryption = format.encryption || details.value(QLatin1String("Encryption")).toBool()
                || !details.value(QLatin1String("EncryptionMethods")).toArray().isEmpty();
            format.headerEncryption = format.headerEncryption || details.value(QLatin1String("HeaderEncryption")).toBool();
            format.multiVolume = format.multiVolume || details.value(QLatin1String("SupportsMultiVolume")).toBool();
            format.testing = format.testing || details.value(QLatin1String("SupportsTesting")).toBool();
        }
    }
    return backends;
}

Backends Backends::installed()
{
    QList<QJsonObject> plugins;
    const QList<KPluginMetaData> found = KPluginMetaData::findPlugins(QStringLiteral("kf6/kerfuffle"));
    for (const KPluginMetaData &plugin : found) {
        plugins.append(plugin.rawData());
    }
    return fromPlugins(plugins, [](const QString &executable) {
        return !QStandardPaths::findExecutable(executable).isEmpty();
    });
}

FormatSupport Backends::support(const QString &mimeType) const
{
    const auto exact = m_formats.constFind(mimeType);
    if (exact != m_formats.cend()) {
        return *exact;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    if (!type.isValid()) {
        return {};
    }
    const auto known = m_formats.constFind(type.name());
    if (known != m_formats.cend()) {
        return *known;
    }
    for (auto it = m_formats.cbegin(); it != m_formats.cend(); ++it) {
        if (type.inherits(it.key())) {
            return it.value();
        }
    }
    return {};
}

bool Backends::canRead(const QString &mimeType) const
{
    return support(mimeType).read;
}

bool Backends::canWrite(const QString &mimeType) const
{
    return support(mimeType).write;
}

QStringList Backends::readableMimeTypes() const
{
    QStringList types = m_formats.keys();
    types.sort();
    return types;
}

QStringList Backends::readOnlyMimeTypes() const
{
    QStringList types;
    for (auto it = m_formats.cbegin(); it != m_formats.cend(); ++it) {
        if (it->read && !it->write) {
            types.append(it.key());
        }
    }
    types.sort();
    return types;
}

QStringList Backends::writableMimeTypes() const
{
    QStringList types;
    for (auto it = m_formats.cbegin(); it != m_formats.cend(); ++it) {
        if (it->write) {
            types.append(it.key());
        }
    }
    types.sort();
    return types;
}

bool Backends::isEmpty() const
{
    return m_formats.isEmpty();
}

bool isInstalled()
{
    return !QStandardPaths::findExecutable(QStringLiteral("ark")).isEmpty();
}

QStringList openArguments(const QString &archive)
{
    return {QStringLiteral("--"), archive};
}

QStringList extractArguments(const QString &archive, const QString &destination)
{
    return {QStringLiteral("--batch"), QStringLiteral("--destination"), destination, QStringLiteral("--"), archive};
}
}
