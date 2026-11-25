// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearancedefaults.h"
#include "appidentity.h"
#include "interfacelanguage.h"
#include "localizednumbers.h"
#include "mainwindow.h"
#include "opusfm_version.h"
#include "settings.h"
#include "settingsbackup.h"
#include "symbolicactionicons.h"
#include "workspacestartup.h"

#include <KAboutData>
#include <KConfigGroup>
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
#include <QStatusBar>
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
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("opus-file-manager"));
    const QString configuredLanguage = KSharedConfig::openConfig()->group(QStringLiteral("Translation")).readEntry("InterfaceLanguage", QString());
    const QString configuredLocale = KSharedConfig::openConfig()->group(QStringLiteral("Translation")).readEntry("RegionalLocale", QString());
    const bool languageAvailable = configuredLanguage.isEmpty() || InterfaceLanguage::apply(configuredLanguage);
    applyRegionalLocale(configuredLocale, false);
    if (!reverseRequested) {
        QApplication::setLayoutDirection(InterfaceLanguage::layoutDirection(KLocalizedString::languages()));
    }

    KAboutData aboutData = AppIdentity::aboutData(QStringLiteral(OPUSFM_VERSION_STRING));
    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(AppIdentity::desktopFileName(), QIcon(QStringLiteral(":/icons/%1.svg").arg(AppIdentity::desktopFileName()))));

    KCrash::initialize();

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("path"), i18n("Folders to open; the first two fill the panes"), QStringLiteral("[path…]"));
    const QCommandLineOption resetAppearanceOption(QStringLiteral("reset-appearance"),
                                                   i18n("Reset the appearance options to follow Plasma, keeping folder colour rules"));
    const QCommandLineOption noWorkspaceAppearanceOption(QStringLiteral("no-workspace-appearance"),
                                                         i18n("Ignore appearance options attached to workspaces in this session"));
    parser.addOptions({resetAppearanceOption, noWorkspaceAppearanceOption});
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
    window->openUrls(urlsFromArguments(parser.positionalArguments(), QDir::currentPath()));
    window->show();
    if (!workspaceWarning.isEmpty()) {
        window->statusBar()->showMessage(workspaceWarning);
    } else if (!appearanceMessage.isEmpty()) {
        window->statusBar()->showMessage(appearanceMessage);
    }

    QObject::connect(&service, &KDBusService::activateRequested, window, [window](const QStringList &arguments, const QString &workingDirectory) {
        QCommandLineParser instanceParser;
        instanceParser.addPositionalArgument(QStringLiteral("path"), QString());
        const QCommandLineOption resetAppearance(QStringLiteral("reset-appearance"));
        instanceParser.addOptions({resetAppearance, QCommandLineOption(QStringLiteral("no-workspace-appearance"))});
        instanceParser.parse(arguments);
        if (instanceParser.isSet(resetAppearance)) {
            window->resetAppearance();
        }
        window->openUrls(urlsFromArguments(instanceParser.positionalArguments(), workingDirectory));
        window->show();
        if (window->windowHandle()) {
            KWindowSystem::updateStartupId(window->windowHandle());
            KWindowSystem::activateWindow(window->windowHandle());
        }
    });

    return app.exec();
}
