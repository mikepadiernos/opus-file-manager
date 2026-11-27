// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalproviders.h"

#include <KLocalizedString>
#include <KPluginMetaData>
#include <KShell>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

namespace TerminalProviders
{
QList<Provider> separateProviders()
{
    return {Provider::SystemDefault, Provider::Konsole, Provider::Yakuake, Provider::Kwi};
}

QList<Provider> embeddedProviders()
{
    return {Provider::Konsole, Provider::Kwi};
}

QString id(Provider provider)
{
    switch (provider) {
    case Provider::SystemDefault:
        return QStringLiteral("default");
    case Provider::Konsole:
        return QStringLiteral("konsole");
    case Provider::Yakuake:
        return QStringLiteral("yakuake");
    case Provider::Kwi:
        return QStringLiteral("kwi");
    }
    return {};
}

QString name(Provider provider)
{
    switch (provider) {
    case Provider::SystemDefault:
        return i18nc("@item terminal application", "Default Terminal");
    case Provider::Konsole:
        return i18nc("@item terminal application", "Konsole");
    case Provider::Yakuake:
        return i18nc("@item terminal application", "Yakuake");
    case Provider::Kwi:
        return i18nc("@item terminal application", "Kwi");
    }
    return {};
}

QString iconName(Provider provider)
{
    return provider == Provider::Yakuake ? QStringLiteral("yakuake") : QStringLiteral("utilities-terminal");
}

QString executable(Provider provider)
{
    return provider == Provider::SystemDefault ? QString() : id(provider);
}

QString desktopName(Provider provider)
{
    return provider == Provider::SystemDefault ? QString() : QStringLiteral("org.kde.") + id(provider);
}

QString partPluginId(Provider provider)
{
    switch (provider) {
    case Provider::Konsole:
        return QStringLiteral("konsolepart");
    case Provider::Kwi:
        return QStringLiteral("kwipart");
    case Provider::SystemDefault:
    case Provider::Yakuake:
        break;
    }
    return {};
}

bool isInstalled(Provider provider)
{
    return provider == Provider::SystemDefault || !QStandardPaths::findExecutable(executable(provider)).isEmpty();
}

bool isPartInstalled(Provider provider)
{
    const QString plugin = partPluginId(provider);
    return !plugin.isEmpty() && KPluginMetaData::findPluginById(QStringLiteral("kf6/parts"), plugin).isValid();
}

QString unavailableReason(Provider provider)
{
    if (isInstalled(provider)) {
        return {};
    }
    return i18nc("@info:tooltip %1 is a terminal application", "%1 is not installed or is not in PATH.", name(provider));
}

QString partUnavailableReason(Provider provider)
{
    if (partPluginId(provider).isEmpty()) {
        return i18nc("@info:tooltip %1 is a terminal application", "%1 cannot be embedded in the terminal panel.", name(provider));
    }
    if (isPartInstalled(provider)) {
        return {};
    }
    return i18nc("@info:tooltip %1 is a terminal application", "The %1 terminal component is not installed.", name(provider));
}

QString localDirectory(const QUrl &folder)
{
    if (!folder.isLocalFile()) {
        return {};
    }
    const QFileInfo info(folder.toLocalFile());
    if (!info.isAbsolute() || !info.isDir()) {
        return {};
    }
    return QDir::cleanPath(info.absoluteFilePath());
}

QStringList launchArguments(Provider provider, const QString &directory)
{
    switch (provider) {
    case Provider::Konsole:
    case Provider::Kwi:
        return {QStringLiteral("--workdir"), directory};
    case Provider::SystemDefault:
    case Provider::Yakuake:
        break;
    }
    return {};
}

QString yakuakeService()
{
    return QStringLiteral("org.kde.yakuake");
}

QString yakuakeChangeDirectoryCommand(const QString &directory)
{
    return QStringLiteral(" cd -- ") + KShell::quoteArg(directory);
}

int firstYakuakeTerminalId(const QString &terminalIds)
{
    bool ok = false;
    const int terminal = terminalIds.section(QLatin1Char(','), 0, 0).trimmed().toInt(&ok);
    return ok && terminal >= 0 ? terminal : -1;
}
}
