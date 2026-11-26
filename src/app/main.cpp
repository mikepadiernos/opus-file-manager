// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearancedefaults.h"
#include "appidentity.h"
#include "filemanagerrequest.h"
#include "filemanagerservice.h"
#include "interfacelanguage.h"
#include "localizednumbers.h"
#include "mainwindow.h"
#include "operationjob.h"
#include "opusfm_version.h"
#include "settings.h"
#include "settingsbackup.h"
#include "startuppresentation.h"
#include "symbolicactionicons.h"
#include "workspacestartup.h"

#include <KAboutData>
#include <KConfigGroup>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KUiServerV2JobTracker>
#include <KWindowSystem>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

#include <memory>

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

bool applyRegionalLocale(const QString &name, bool reportError)
{
    const QLocale locale = name.isEmpty() ? QLocale::system() : QLocale(name);
    if (!name.isEmpty() && locale.language() == QLocale::C && name != QLatin1String("C")) {
        if (reportError) {
            QMessageBox::critical(
                nullptr,
                i18nc("@title:window", "Invalid Regional Locale"),
                i18nc("@info", "The configured regional locale %1 is not available. Correct it in the Plasma Opus configuration before starting.", name));
        }
        return false;
    }
    QLocale::setDefault(locale);
    return true;
}

void applyLayoutDirection(bool reverseRequested, int direction)
{
    if (reverseRequested) {
        return;
    }
    QApplication::setLayoutDirection(InterfaceLanguage::layoutDirection(KLocalizedString::languages(), direction));
}
}

int main(int argc, char **argv)
{
    bool reverseRequested = false;
    for (int i = 1; i < argc; ++i) {
        reverseRequested |= qstrcmp(argv[i], "-reverse") == 0;
    }
    QApplication app(argc, argv);
    app.installEventFilter(new SymbolicActionIcons(&app));
    // Plasma Opus-owned operations report to Plasma's job view directly. KIO's
    // dynamic tracker would fall back to a separate progress window outside
    // Plasma, duplicating the progress the dialogs already show.
    OperationJob::setDefaultTracker(new KUiServerV2JobTracker(&app));
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));
    const QString configuredLanguage = KSharedConfig::openConfig()->group(QStringLiteral("Translation")).readEntry("InterfaceLanguage", QString());
    const QString configuredLocale = KSharedConfig::openConfig()->group(QStringLiteral("Translation")).readEntry("RegionalLocale", QString());
    const bool languageAvailable = InterfaceLanguage::apply(configuredLanguage);
    applyRegionalLocale(configuredLocale, false);
    if (!reverseRequested) {
        QApplication::setLayoutDirection(InterfaceLanguage::layoutDirection(KLocalizedString::languages()));
    }

    KAboutData aboutData = AppIdentity::aboutData(QStringLiteral(OPUSFM_VERSION_STRING));
    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(AppIdentity::applicationIcon());

    KCrash::initialize();

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("path"), i18n("Folders to open; the first two fill the panes"), QStringLiteral("[path…]"));
    const QCommandLineOption resetAppearanceOption(QStringLiteral("reset-appearance"),
                                                   i18n("Reset the appearance options to follow Plasma, keeping folder colour rules"));
    const QCommandLineOption noWorkspaceAppearanceOption(QStringLiteral("no-workspace-appearance"),
                                                         i18n("Ignore appearance options attached to workspaces in this session"));
    const QCommandLineOption fileManagerServiceOption(QStringLiteral("file-manager-service"),
                                                      i18n("Start without a window to answer an org.freedesktop.FileManager1 request"));
    const QCommandLineOption workspaceOption(QStringLiteral("workspace"), i18n("Switch to the saved workspace with this id"), QStringLiteral("id"));
    parser.addOptions({resetAppearanceOption, noWorkspaceAppearanceOption, fileManagerServiceOption, workspaceOption});
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

    bool effectiveLanguageAvailable = languageAvailable;
    if (Settings::interfaceLanguage() != configuredLanguage) {
        effectiveLanguageAvailable = InterfaceLanguage::apply(Settings::interfaceLanguage());
    }
    if (!effectiveLanguageAvailable) {
        QMessageBox::warning(nullptr,
                             i18nc("@title:window", "Interface Language Unavailable"),
                             i18nc("@info",
                                   "The selected interface language cannot be used because no compatible UTF-8 system message locale is installed. "
                                   "Plasma Opus will use the system language instead."));
    }
    if (!applyRegionalLocale(Settings::regionalLocale(), true)) {
        return 1;
    }
    if (Settings::numeralStyle() == Settings::EnumNumeralStyle::CustomDigits && !LocalizedNumbers::validCustomNumerals(Settings::customNumerals())) {
        QMessageBox::critical(
            nullptr,
            i18nc("@title:window", "Invalid Custom Digits"),
            i18nc("@info", "Custom digits must contain ten distinct printable symbols. Correct the Plasma Opus configuration before starting."));
        return 1;
    }
    applyLayoutDirection(reverseRequested, Settings::translationDirection());

    QString workspaceWarning;
    {
        // A separate configuration object, like the window's, so pending
        // shared changes cannot block or be mixed into the catalog update.
        const auto shared = KSharedConfig::openConfig();
        KConfig workspaceConfig(shared->name(), shared->openFlags(), shared->locationType());
        WorkspaceStartup::apply(workspaceConfig.group(QStringLiteral("Workspaces")), Settings::startupWorkspace(), &workspaceWarning);
        if (!shared->isDirty()) {
            shared->reparseConfiguration();
        }
    }

    AppearanceDefaults::setWorkspaceOverridesDisabled(parser.isSet(noWorkspaceAppearanceOption));
    QString appearanceMessage;
    if (parser.isSet(resetAppearanceOption)) {
        // Reset before the window is created, so it never opens with the
        // appearance that is being reset.
        if (AppearanceDefaults::reset(Settings::self()).isEmpty()) {
            appearanceMessage = i18nc("@info:status", "The appearance options already follow Plasma.");
        } else {
            Settings::self()->save();
            appearanceMessage = i18nc("@info:status", "The appearance options were reset to follow Plasma.");
        }
    }

    auto *window = new MainWindow;
    // When D-Bus activation starts Plasma Opus for a FileManager1 request,
    // the request decides what to open, so the window waits for it.
    const bool serviceMode = parser.isSet(fileManagerServiceOption);
    auto *fileManager = new FileManagerService(window);
    fileManager->registerOnBus(serviceMode || FileManagerRequest::shouldClaim(FileManagerService::defaultFolderHandler()));
    const auto showStartupMessage = [window, workspaceWarning, appearanceMessage] {
        if (!workspaceWarning.isEmpty()) {
            window->statusBar()->showMessage(workspaceWarning);
        } else if (!appearanceMessage.isEmpty()) {
            window->statusBar()->showMessage(appearanceMessage);
        }
    };
    if (serviceMode) {
        QObject::connect(fileManager, &FileManagerService::requestHandled, window, showStartupMessage, Qt::SingleShotConnection);
        // Quit only when no request arrived; a handled request's window may
        // since have been hidden to the tray.
        auto requestHandled = std::make_shared<bool>(false);
        QObject::connect(fileManager, &FileManagerService::requestHandled, window, [requestHandled] {
            *requestHandled = true;
        });
        QTimer::singleShot(std::chrono::seconds(30), window, [window, requestHandled] {
            if (!*requestHandled && !window->isVisible()) {
                QApplication::quit();
            }
        });
    } else {
        window->openUrls(urlsFromArguments(parser.positionalArguments(), QDir::currentPath()));
        window->show();
        StartupPresentation::presentIfEnabled(window);
        showStartupMessage();
        if (parser.isSet(workspaceOption)) {
            QTimer::singleShot(0, window, [window, id = parser.value(workspaceOption)] {
                window->openWorkspace(id);
            });
        }
    }

    QObject::connect(&service, &KDBusService::activateRequested, window, [window](const QStringList &arguments, const QString &workingDirectory) {
        QCommandLineParser instanceParser;
        instanceParser.addPositionalArgument(QStringLiteral("path"), QString());
        const QCommandLineOption resetAppearance(QStringLiteral("reset-appearance"));
        const QCommandLineOption fileManagerService(QStringLiteral("file-manager-service"));
        const QCommandLineOption workspace(QStringLiteral("workspace"), QString(), QStringLiteral("id"));
        instanceParser.addOptions({resetAppearance, QCommandLineOption(QStringLiteral("no-workspace-appearance")), fileManagerService, workspace});
        instanceParser.parse(arguments);
        if (instanceParser.isSet(fileManagerService)) {
            // Another process was activated for a request this instance
            // does not serve; the request itself arrives separately.
            return;
        }
        if (instanceParser.isSet(resetAppearance)) {
            window->resetAppearance();
        }
        window->openUrls(urlsFromArguments(instanceParser.positionalArguments(), workingDirectory));
        // Also restores a window that is minimized or hidden in the tray.
        window->setWindowState(window->windowState() & ~Qt::WindowMinimized);
        window->show();
        if (window->windowHandle()) {
            KWindowSystem::updateStartupId(window->windowHandle());
            KWindowSystem::activateWindow(window->windowHandle());
        }
        if (instanceParser.isSet(workspace)) {
            window->openWorkspace(instanceParser.value(workspace));
        }
    });

    return app.exec();
}
