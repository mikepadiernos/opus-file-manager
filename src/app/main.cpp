// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "opusfm_version.h"
#include "settingsbackup.h"
#include "symbolicactionicons.h"

#include <KAboutData>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KWindowSystem>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>

namespace
{
QList<QUrl> urlsFromArguments(const QStringList &arguments, const QString &workingDirectory)
{
    QList<QUrl> urls;
    for (const QString &argument : arguments) {
        QUrl url = QUrl::fromUserInput(argument, workingDirectory, QUrl::AssumeLocalFile);
        if (url.isValid()) {
            urls.append(url);
        }
    }
    return urls;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.installEventFilter(new SymbolicActionIcons(&app));
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));

    KAboutData aboutData(QStringLiteral("opus-file-manager"),
                         i18n("Opus File Manager"),
                         QStringLiteral(OPUSFM_VERSION_STRING),
                         i18n("Dual-pane file manager for KDE Plasma"),
                         KAboutLicense::GPL_V3,
                         i18n("© 2026 Mike Padiernos"));
    aboutData.addAuthor(i18n("Mike Padiernos"), i18n("Author"));
    aboutData.setDesktopFileName(QStringLiteral("org.kde.opusfilemanager"));
    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));

    KCrash::initialize();

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("path"), i18n("Folders to open; the first two fill the panes"), QStringLiteral("[path…]"));
    parser.process(app);
    aboutData.processCommandLine(&parser);

    KDBusService service(KDBusService::Unique);
    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString restoreError = SettingsBackup::applyPendingRestore(dataDirectory, KSharedConfig::openConfig(), KSharedConfig::openStateConfig());
    if (!restoreError.isEmpty()) {
        QMessageBox::critical(nullptr,
                              i18nc("@title:window", "Could Not Restore Settings"),
                              i18nc("@info",
                                    "%1\n\nThe pending backup was kept at %2. Remove it only if you want to cancel the restore.",
                                    restoreError,
                                    QDir::toNativeSeparators(SettingsBackup::pendingRestorePath(dataDirectory))));
        return 1;
    }

    auto *window = new MainWindow;
    window->openUrls(urlsFromArguments(parser.positionalArguments(), QDir::currentPath()));
    window->show();

    QObject::connect(&service, &KDBusService::activateRequested, window, [window](const QStringList &arguments, const QString &workingDirectory) {
        QCommandLineParser instanceParser;
        instanceParser.addPositionalArgument(QStringLiteral("path"), QString());
        instanceParser.parse(arguments);
        window->openUrls(urlsFromArguments(instanceParser.positionalArguments(), workingDirectory));
        window->show();
        if (window->windowHandle()) {
            KWindowSystem::updateStartupId(window->windowHandle());
            KWindowSystem::activateWindow(window->windowHandle());
        }
    });

    return app.exec();
}
