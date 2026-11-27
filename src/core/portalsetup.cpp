// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalsetup.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
const QString PortalDirectory = QStringLiteral("xdg-desktop-portal");

QStringList splitDirs(const QByteArray &value, const QStringList &fallback)
{
    QStringList dirs;
    const QStringList parts = QString::fromLocal8Bit(value).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (QDir::isAbsolutePath(part)) {
            dirs.append(part);
        }
    }
    return dirs.isEmpty() ? fallback : dirs;
}

QStringList tokens(const QString &value)
{
    QStringList list;
    const QStringList parts = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString token = part.trimmed();
        if (!token.isEmpty() && token != PortalSetup::portalName() && !list.contains(token)) {
            list.append(token);
        }
    }
    return list;
}

bool isHeader(const QString &line, QString *name)
{
    static const QRegularExpression header(QStringLiteral("^\\s*\\[([^\\]]*)\\]\\s*$"));
    const QRegularExpressionMatch match = header.match(line);
    if (match.hasMatch() && name) {
        *name = match.captured(1).trimmed();
    }
    return match.hasMatch();
}

/// The key of an entry line, or an empty string for other lines.
QString entryKey(const QString &line, QString *value = nullptr)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')) || trimmed.startsWith(QLatin1Char(';'))) {
        return {};
    }
    const qsizetype equals = trimmed.indexOf(QLatin1Char('='));
    if (equals <= 0) {
        return {};
    }
    if (value) {
        *value = trimmed.mid(equals + 1).trimmed();
    }
    return trimmed.left(equals).trimmed();
}

/// The line range of the [preferred] group: its header and the line after its last line.
std::pair<qsizetype, qsizetype> preferredGroup(const QStringList &lines)
{
    qsizetype start = -1;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        QString name;
        if (!isHeader(lines.at(i), &name)) {
            continue;
        }
        if (start >= 0) {
            return {start, i};
        }
        if (name == QLatin1String("preferred")) {
            start = i;
        }
    }
    return {start, lines.size()};
}

bool readFile(const QString &path, QString *content)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    *content = QString::fromUtf8(file.readAll());
    return true;
}

QString writeFile(const QString &path, const QString &content)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return i18nc("@info", "Could not create the folder %1.", QFileInfo(path).absolutePath());
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content.toUtf8()) < 0 || !file.commit()) {
        return i18nc("@info", "Could not write %1: %2", path, file.errorString());
    }
    return {};
}

QString removeFile(const QString &path)
{
    if (QFile::exists(path) && !QFile::remove(path)) {
        return i18nc("@info", "Could not remove %1.", path);
    }
    return {};
}

bool prefersOpus(const QString &config)
{
    const QString value = PortalSetup::preferredEntry(config, PortalSetup::fileChooserInterface());
    const QStringList parts = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    return !parts.isEmpty() && parts.constFirst().trimmed() == PortalSetup::portalName();
}

bool mentionsOpus(const QString &config)
{
    const QString value = PortalSetup::preferredEntry(config, PortalSetup::fileChooserInterface());
    const QStringList parts = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    return std::any_of(parts.cbegin(), parts.cend(), [](const QString &part) {
        return part.trimmed() == PortalSetup::portalName();
    });
}

const QString HadConfigKey = QStringLiteral("HadConfig");
const QString PreviousConfigKey = QStringLiteral("PreviousConfig");
const QString WrittenConfigKey = QStringLiteral("WrittenConfig");
}

namespace PortalSetup
{
QString portalName()
{
    return QStringLiteral("opusfilemanager");
}

QString dbusName()
{
    return QStringLiteral("org.freedesktop.impl.portal.desktop.opusfilemanager");
}

QString fileChooserInterface()
{
    return QStringLiteral("org.freedesktop.impl.portal.FileChooser");
}

Paths Paths::fromEnvironment(const QString &helperExecutable)
{
    Paths paths;
    paths.dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    paths.configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    paths.configDirs = splitDirs(qgetenv("XDG_CONFIG_DIRS"), {QStringLiteral("/etc/xdg")});
    paths.configDirs.append(QStringLiteral("/etc"));
    paths.dataDirs = splitDirs(qgetenv("XDG_DATA_DIRS"), {QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share")});
    paths.dataDirs.append(QStringLiteral("/usr/share"));
    paths.helperExecutable = helperExecutable;
    return paths;
}

QString Paths::portalFile() const
{
    return dataHome + QLatin1Char('/') + PortalDirectory + QStringLiteral("/portals/") + portalName() + QStringLiteral(".portal");
}

QString Paths::serviceFile() const
{
    return dataHome + QStringLiteral("/dbus-1/services/") + dbusName() + QStringLiteral(".service");
}

QString Paths::configFile() const
{
    return configHome + QLatin1Char('/') + PortalDirectory + QStringLiteral("/kde-portals.conf");
}

QString portalFileContent()
{
    return QStringLiteral("[portal]\nDBusName=%1\nInterfaces=%2;\n").arg(dbusName(), fileChooserInterface());
}

QString serviceFileContent(const QString &executable)
{
    return QStringLiteral("[D-BUS Service]\nName=%1\nExec=%2\n").arg(dbusName(), executable);
}

QString preferredEntry(const QString &config, const QString &key)
{
    const QStringList lines = config.split(QLatin1Char('\n'));
    const auto [start, end] = preferredGroup(lines);
    if (start < 0) {
        return {};
    }
    for (qsizetype i = start + 1; i < end; ++i) {
        QString value;
        if (entryKey(lines.at(i), &value) == key) {
            return value;
        }
    }
    return {};
}

QString setPreferredEntry(const QString &config, const QString &key, const QString &value)
{
    QStringList lines = config.split(QLatin1Char('\n'));
    const auto [start, end] = preferredGroup(lines);
    const QString entry = key + QLatin1Char('=') + value;
    if (start < 0) {
        if (value.isEmpty()) {
            return config;
        }
        QString text = config;
        if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n'))) {
            text += QLatin1Char('\n');
        }
        if (!text.trimmed().isEmpty()) {
            text += QLatin1Char('\n');
        }
        return text + QStringLiteral("[preferred]\n") + entry + QLatin1Char('\n');
    }
    qsizetype last = start;
    for (qsizetype i = start + 1; i < end; ++i) {
        if (entryKey(lines.at(i)) == key) {
            if (value.isEmpty()) {
                lines.removeAt(i);
            } else {
                lines[i] = entry;
            }
            return lines.join(QLatin1Char('\n'));
        }
        if (!lines.at(i).trimmed().isEmpty()) {
            last = i;
        }
    }
    if (!value.isEmpty()) {
        lines.insert(last + 1, entry);
    }
    return lines.join(QLatin1Char('\n'));
}

QString preferOpus(const QString &fallback)
{
    QStringList list = tokens(fallback);
    if (list.isEmpty()) {
        list.append(QStringLiteral("kde"));
    }
    list.prepend(portalName());
    return list.join(QLatin1Char(';'));
}

QString withoutOpus(const QString &value)
{
    return tokens(value).join(QLatin1Char(';'));
}

QString effectiveSystemConfig(const Paths &paths)
{
    QStringList candidates{paths.configHome + QLatin1Char('/') + PortalDirectory + QStringLiteral("/portals.conf")};
    for (const QStringList &dirs : {paths.configDirs, paths.dataDirs}) {
        for (const QString &dir : dirs) {
            candidates.append(dir + QLatin1Char('/') + PortalDirectory + QStringLiteral("/kde-portals.conf"));
            candidates.append(dir + QLatin1Char('/') + PortalDirectory + QStringLiteral("/portals.conf"));
        }
    }
    for (const QString &candidate : std::as_const(candidates)) {
        QString content;
        if (readFile(candidate, &content)) {
            return content;
        }
    }
    return {};
}

State state(const Paths &paths)
{
    QString config;
    readFile(paths.configFile(), &config);
    const bool portal = QFile::exists(paths.portalFile());
    const bool service = QFile::exists(paths.serviceFile());
    if (portal && service && prefersOpus(config)) {
        return State::Installed;
    }
    if (!portal && !service && !mentionsOpus(config)) {
        return State::NotInstalled;
    }
    return State::Partial;
}

QString install(const Paths &paths, KConfigGroup &backup)
{
    const QFileInfo helper(paths.helperExecutable);
    if (!helper.isFile() || !helper.isExecutable()) {
        return i18nc("@info", "The Plasma Opus file chooser service %1 is not installed.", paths.helperExecutable);
    }
    QString error = writeFile(paths.portalFile(), portalFileContent());
    if (error.isEmpty()) {
        error = writeFile(paths.serviceFile(), serviceFileContent(helper.absoluteFilePath()));
    }
    if (!error.isEmpty()) {
        return error;
    }

    QString current;
    const bool hadConfig = readFile(paths.configFile(), &current);
    QString base = hadConfig ? current : effectiveSystemConfig(paths);
    if (base.trimmed().isEmpty()) {
        base = QStringLiteral("[preferred]\ndefault=kde\n");
    }
    QString fallback = preferredEntry(base, fileChooserInterface());
    if (fallback.isEmpty()) {
        fallback = preferredEntry(base, QStringLiteral("default"));
    }
    const QString written = setPreferredEntry(base, fileChooserInterface(), preferOpus(fallback));

    if (!backup.hasKey(WrittenConfigKey)) {
        backup.writeEntry(HadConfigKey, hadConfig);
        backup.writeEntry(PreviousConfigKey, current);
    }
    backup.writeEntry(WrittenConfigKey, written);
    if (!backup.sync()) {
        return i18nc("@info", "Could not save a backup of the portal configuration.");
    }
    return writeFile(paths.configFile(), written);
}

QString uninstall(const Paths &paths, KConfigGroup &backup)
{
    QString current;
    QString error;
    if (readFile(paths.configFile(), &current)) {
        if (backup.hasKey(WrittenConfigKey) && current == backup.readEntry(WrittenConfigKey, QString())) {
            error = backup.readEntry(HadConfigKey, false) ? writeFile(paths.configFile(), backup.readEntry(PreviousConfigKey, QString()))
                                                          : removeFile(paths.configFile());
        } else if (mentionsOpus(current)) {
            const QString value = preferredEntry(current, fileChooserInterface());
            error = writeFile(paths.configFile(), setPreferredEntry(current, fileChooserInterface(), withoutOpus(value)));
        }
    }
    if (error.isEmpty()) {
        error = removeFile(paths.portalFile());
    }
    if (error.isEmpty()) {
        error = removeFile(paths.serviceFile());
    }
    if (!error.isEmpty()) {
        return error;
    }
    backup.deleteGroup();
    if (!backup.sync()) {
        return i18nc("@info", "Could not remove the backup of the portal configuration.");
    }
    return {};
}

bool portalServiceAvailable()
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()) {
        return false;
    }
    const QString service = QStringLiteral("org.freedesktop.portal.Desktop");
    if (bus.interface()->isServiceRegistered(service)) {
        return true;
    }
    const QDBusReply<QStringList> activatable = bus.interface()->activatableServiceNames();
    return activatable.isValid() && activatable.value().contains(service);
}

QString restartPortal()
{
    QProcess process;
    process.start(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("try-restart"), QStringLiteral("xdg-desktop-portal.service")});
    if (!process.waitForStarted()) {
        return i18nc("@info", "Could not restart xdg-desktop-portal because systemctl is not available.");
    }
    if (!process.waitForFinished() || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString details = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        return details.isEmpty() ? i18nc("@info", "Could not restart xdg-desktop-portal.")
                                 : i18nc("@info %1 is an error message", "Could not restart xdg-desktop-portal: %1", details);
    }
    return {};
}
}
