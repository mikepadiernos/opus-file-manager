// SPDX-License-Identifier: GPL-3.0-or-later

#include "atuin.h"

#include <KLocalizedString>
#include <KShell>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace
{
// Atuin resolves its folders itself rather than through Qt, so these follow
// its environment variables and XDG defaults.
QString environmentPath(const char *variable)
{
    return QFile::decodeName(qgetenv(variable));
}

QString xdgFolder(const char *variable, const QString &fallback)
{
    const QString value = environmentPath(variable);
    return QDir::isAbsolutePath(value) ? value : QDir::homePath() + fallback;
}

QString expandHome(const QString &path)
{
    if (path == QLatin1String("~")) {
        return QDir::homePath();
    }
    if (path.startsWith(QLatin1String("~/"))) {
        return QDir::homePath() + path.mid(1);
    }
    return path;
}

QString configuredDatabasePath()
{
    QFile config(Atuin::configFile());
    if (!config.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    static const QRegularExpression entry(QStringLiteral(R"re(^\s*db_path\s*=\s*(?:"([^"]*)"|'([^']*)'))re"));
    while (!config.atEnd()) {
        const QString line = QString::fromUtf8(config.readLine());
        if (line.trimmed().startsWith(QLatin1Char('['))) {
            // Keys after a table header belong to that table.
            return {};
        }
        const QRegularExpressionMatch match = entry.match(line);
        if (match.hasMatch()) {
            return expandHome(match.captured(1).isEmpty() ? match.captured(2) : match.captured(1));
        }
    }
    return {};
}
}

QString Atuin::executable()
{
    const QString name = QStringLiteral("atuin");
    const QString inPath = QStandardPaths::findExecutable(name);
    if (!inPath.isEmpty()) {
        return inPath;
    }
    const QString home = QDir::homePath();
    return QStandardPaths::findExecutable(name,
                                          {home + QStringLiteral("/.atuin/bin"), home + QStringLiteral("/.cargo/bin"), home + QStringLiteral("/.local/bin")});
}

QString Atuin::configFile()
{
    const QString folder = environmentPath("ATUIN_CONFIG_DIR");
    return (folder.isEmpty() ? xdgFolder("XDG_CONFIG_HOME", QStringLiteral("/.config")) + QStringLiteral("/atuin") : folder) + QStringLiteral("/config.toml");
}

QString Atuin::databasePath()
{
    const QString fromEnvironment = environmentPath("ATUIN_DB_PATH");
    if (!fromEnvironment.isEmpty()) {
        return expandHome(fromEnvironment);
    }
    const QString configured = configuredDatabasePath();
    if (!configured.isEmpty()) {
        return configured;
    }
    const QString dataFolder = environmentPath("ATUIN_DATA_DIR");
    return (dataFolder.isEmpty() ? xdgFolder("XDG_DATA_HOME", QStringLiteral("/.local/share")) + QStringLiteral("/atuin") : dataFolder)
        + QStringLiteral("/history.db");
}

QString Atuin::unavailableReason()
{
    if (executable().isEmpty()) {
        return i18nc("@info", "Atuin is not installed. Install it from your distribution, then set it up in your shell to search command history.");
    }
    if (!QFileInfo(databasePath()).isFile()) {
        return i18nc("@info %1 is a file path",
                     "Atuin has no command history at %1 yet. Set it up in your shell with atuin init, or import existing history with atuin import, "
                     "then try again.",
                     databasePath());
    }
    return {};
}

QString Atuin::searchInput(const QString &executable)
{
    return QStringLiteral(" ") + KShell::quoteArg(executable) + QStringLiteral(" search --interactive\n");
}
