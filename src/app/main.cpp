// SPDX-License-Identifier: GPL-3.0-or-later

#include "interfacelanguage.h"
#include "localizednumbers.h"
#include "mainwindow.h"
#include "opusfm_version.h"
#include "settings.h"
#include "settingsbackup.h"
#include "symbolicactionicons.h"

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

    KAboutData aboutData(QStringLiteral("opus-file-manager"),
                         i18n("Plasma Opus"),
                         QStringLiteral(OPUSFM_VERSION_STRING),
                         i18n("Dual-pane file manager for KDE Plasma"),
                         KAboutLicense::GPL_V3,
                         i18n("© 2026 Mike Padiernos"));
    aboutData.addAuthor(i18n("Mike Padiernos"), i18n("Author"));
    aboutData.setDesktopFileName(QStringLiteral("org.kde.opusfilemanager"));
    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("org.kde.opusfilemanager"), QIcon(QStringLiteral(":/icons/org.kde.opusfilemanager.svg"))));

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
