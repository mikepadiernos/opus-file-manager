// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityassociations.h"
#include "activityassociationseditor.h"
#include "activitycontroller.h"
#include "appidentity.h"
#include "defaultfilemanager.h"
#include "desktoptray.h"
#include "favoritesmodel.h"
#include "filechooser.h"
#include "kwinintegration.h"
#include "mainwindow.h"
#include "mountbackend.h"
#include "onlineaccounts.h"
#include "operationjob.h"
#include "portalsetup.h"
#include "runnermatches.h"
#include "settings.h"
#include "settingsdialog.h"
#include "settingswidgets.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KConfig>
#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KKeySequenceWidget>
#include <KMessageWidget>
#include <KPluginMetaData>
#include <KService>
#include <KSharedConfig>
#include <KSycoca>
#include <KUrlRequester>

#include <KToolBar>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScopeGuard>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#ifdef OPUSFM_HAVE_ACCOUNTS
#include <Accounts/Account>
#include <Accounts/Manager>
#include <Accounts/Service>
#endif

extern KSERVICE_EXPORT int ksycoca_ms_between_checks;

namespace
{
class FakeActivities : public ActivitySource
{
public:
    QString current;
    QMap<QString, QString> names;

    bool isAvailable() const override
    {
        return true;
    }
    QString currentActivity() const override
    {
        return current;
    }
    QMap<QString, QString> activities() const override
    {
        return names;
    }
};

struct FakeKWin {
    bool running = true;
    std::optional<bool> loaded = false;
    int installed = -1;
    bool enabled = false;
    int processRequests = 0;
    bool installedAsUpdate = false;
    KWinBackend::Callback pendingInstall;
    KWinBackend::Callback pendingRemove;
};

class FakeKWinBackend : public KWinBackend
{
public:
    explicit FakeKWinBackend(std::shared_ptr<FakeKWin> state)
        : m(std::move(state))
    {
    }

    bool kwinRunning() const override
    {
        return m->running;
    }
    std::optional<bool> scriptLoaded() const override
    {
        return m->loaded;
    }
    int installedVersion() const override
    {
        return m->installed;
    }
    bool pluginEnabled() const override
    {
        return m->enabled;
    }
    void setPluginEnabled(bool enabled) override
    {
        m->enabled = enabled;
    }
    void forgetPluginEnabled() override
    {
        m->enabled = false;
    }
    void startScripts() override
    {
        if (m->enabled && m->installed >= 0) {
            m->loaded = true;
        }
    }
    void unloadScript() override
    {
        m->loaded = false;
    }
    void invokeProcessShortcut() override
    {
        ++m->processRequests;
    }
    void installPackage(const QString &, bool update, const Callback &done) override
    {
        m->installedAsUpdate = update;
        m->pendingInstall = done;
    }
    void removePackage(const Callback &done) override
    {
        m->pendingRemove = done;
    }

private:
    std::shared_ptr<FakeKWin> m;
};
}

class IntegrationSettingsTest : public QObject
{
    Q_OBJECT

private:
    QString m_applications;
    QString m_configHome;

    static KFilePlacesModel *placesOf(MainWindow &window)
    {
        return qobject_cast<KFilePlacesModel *>(window.findChild<KFilePlacesView *>()->model());
    }

    static QString tabTitleOf(const QWidget *widget)
    {
        for (const QWidget *ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            if (const auto *tabs = qobject_cast<const QTabWidget *>(ancestor)) {
                for (int i = 0; i < tabs->count(); ++i) {
                    if (tabs->widget(i)->isAncestorOf(widget)) {
                        return tabs->tabText(i);
                    }
                }
            }
        }
        return {};
    }

    static void write(const QString &path, const QString &content)
    {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content.toUtf8());
    }

    void installApplication(const QString &desktopId)
    {
        write(m_applications + QLatin1Char('/') + desktopId,
              QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=true %u\nMimeType=inode/directory;\n")
                  .arg(desktopId.section(QLatin1Char('.'), -2, -2)));
        KSycoca::self()->ensureCacheValid();
    }

    void setFolderDefault(const QString &desktopId)
    {
        write(m_configHome + QStringLiteral("/mimeapps.list"), QStringLiteral("[Default Applications]\ninode/directory=%1;\n").arg(desktopId));
        KSycoca::self()->ensureCacheValid();
    }

    QString activationFile() const
    {
        return DefaultFileManager::activationFile(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    }

    static QString executable()
    {
        return QCoreApplication::applicationDirPath() + QLatin1Char('/') + AppIdentity::componentName();
    }

    static PortalSetup::Paths portalPaths()
    {
        return PortalSetup::Paths::fromEnvironment(QCoreApplication::applicationDirPath() + QStringLiteral("/opus-file-manager-portal"));
    }

    /// Hooks that find xdg-desktop-portal when @p available, count restarts and answer the restart question with @p confirm.
    void setPortalHooks(SettingsDialog &dialog, bool available, bool confirm = true, const QString &restartError = {})
    {
        m_restarts = 0;
        m_questions.clear();
        dialog.setPortalSetupHooks(
            [available] {
                return available;
            },
            [this, restartError] {
                ++m_restarts;
                return restartError;
            },
            [this, confirm](const QString &question) {
                m_questions.append(question);
                return confirm;
            });
    }

    int m_restarts = 0;
    QStringList m_questions;
    QTemporaryDir m_accountsDir;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        ksycoca_ms_between_checks = 0;
        m_applications = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/applications");
        m_configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        // The desktop-specific list of the developer's session must not apply.
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        // Keep libaccounts away from the real account database.
        QVERIFY(m_accountsDir.isValid());
        qputenv("ACCOUNTS", m_accountsDir.path().toUtf8());
        qputenv("AG_PROVIDERS", m_accountsDir.path().toUtf8());
        qputenv("AG_SERVICES", m_accountsDir.path().toUtf8());
    }

    void init()
    {
        QDir(m_applications).removeRecursively();
        QVERIFY(QDir().mkpath(m_applications));
        QVERIFY(QDir().mkpath(m_configHome));
        QFile::remove(m_configHome + QStringLiteral("/mimeapps.list"));
        QFile::remove(m_configHome + QStringLiteral("/kde-mimeapps.list"));
        QFile::remove(activationFile());
        KSycoca::self()->ensureCacheValid();
        const PortalSetup::Paths paths = portalPaths();
        QFile::remove(paths.portalFile());
        QFile::remove(paths.serviceFile());
        QFile::remove(paths.configFile());
        KSharedConfig::openStateConfig()->deleteGroup(PortalSetup::backupGroupName());
        KSharedConfig::openStateConfig()->deleteGroup(FileChooser::stateGroupName());
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Tray"));
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("KdeConnectShare"));
        KSharedConfig::openStateConfig()->sync();
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
        Settings::self()->setDefaults();
    }

    void fileManagerTabIsOnTheKdePlasmaPage()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("fileManagerIntegrationPage"));
        QVERIFY(page);
        QCOMPARE(tabTitleOf(page->findChild<QWidget *>(QStringLiteral("defaultFileManagerGroup"))), QStringLiteral("File Manager"));
    }

    void setsPlasmaOpusAsTheDefaultFileManager()
    {
        installApplication(QStringLiteral("org.kde.dolphin.desktop"));
        installApplication(AppIdentity::desktopFileName() + QStringLiteral(".desktop"));
        setFolderDefault(QStringLiteral("org.kde.dolphin.desktop"));

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("defaultFileManagerStatus"));
        auto *setDefault = dialog.findChild<QPushButton *>(QStringLiteral("setDefaultFileManager"));
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("fileManagerMessage"));
        QVERIFY(status->text().contains(QStringLiteral("dolphin")));
        QVERIFY(setDefault->isEnabled());

        setDefault->click();
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QVERIFY(message->text().contains(QStringLiteral("activation file")));
        QVERIFY(!setDefault->isEnabled());
        QVERIFY(status->text().contains(QStringLiteral("Plasma Opus is the default")));
        const KConfig list(m_configHome + QStringLiteral("/mimeapps.list"), KConfig::SimpleConfig);
        QCOMPARE(list.group(QStringLiteral("Default Applications")).readXdgListEntry(DefaultFileManager::folderMimeType()),
                 QStringList{AppIdentity::desktopFileName() + QStringLiteral(".desktop")});
    }

    void reportsAnOverridingDesktopList()
    {
        installApplication(QStringLiteral("org.kde.dolphin.desktop"));
        installApplication(AppIdentity::desktopFileName() + QStringLiteral(".desktop"));
        write(m_configHome + QStringLiteral("/kde-mimeapps.list"), QStringLiteral("[Default Applications]\ninode/directory=org.kde.dolphin.desktop;\n"));
        KSycoca::self()->ensureCacheValid();

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("setDefaultFileManager"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("fileManagerMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("kde-mimeapps.list")));
    }

    void cannotSetTheDefaultWhenNotInstalled()
    {
        if (KService::serviceByDesktopName(AppIdentity::desktopFileName())) {
            QSKIP("Plasma Opus is installed on this system.");
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("setDefaultFileManager"))->isEnabled());
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("defaultFileManagerStatus"))->text().contains(QStringLiteral("install.sh")));
    }

    void installsTheActivationFileOnlyWhileDefault()
    {
        installApplication(QStringLiteral("org.kde.dolphin.desktop"));
        installApplication(AppIdentity::desktopFileName() + QStringLiteral(".desktop"));
        setFolderDefault(QStringLiteral("org.kde.dolphin.desktop"));
        {
            MainWindow window;
            SettingsDialog dialog(placesOf(window), &window);
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("installFileManagerActivation"))->isEnabled());
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("removeFileManagerActivation"))->isEnabled());
        }

        setFolderDefault(AppIdentity::desktopFileName() + QStringLiteral(".desktop"));
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *install = dialog.findChild<QPushButton *>(QStringLiteral("installFileManagerActivation"));
        auto *remove = dialog.findChild<QPushButton *>(QStringLiteral("removeFileManagerActivation"));
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("fileManagerMessage"));
        QVERIFY(install->isEnabled());
        install->click();
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QCOMPARE(DefaultFileManager::activationState(activationFile(), executable()), DefaultFileManager::Activation::Installed);
        QVERIFY(!install->isEnabled());
        QVERIFY(remove->isEnabled());

        remove->click();
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QVERIFY(!QFile::exists(activationFile()));
        QVERIFY(!remove->isEnabled());
    }

    void leavesAForeignActivationFileAlone()
    {
        const QString foreign = QStringLiteral("[D-BUS Service]\nName=org.freedesktop.FileManager1\nExec=/usr/bin/dolphin --daemon\n");
        write(activationFile(), foreign);
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("installFileManagerActivation"))->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("removeFileManagerActivation"))->isEnabled());
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("fileManagerActivationStatus"))->text().contains(QStringLiteral("leaves it unchanged")));
    }

    void setsUpAndRestoresTheFileChooser()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        setPortalHooks(dialog, true);
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("fileChooserPortalStatus"));
        auto *install = dialog.findChild<QPushButton *>(QStringLiteral("installFileChooserPortal"));
        auto *restore = dialog.findChild<QPushButton *>(QStringLiteral("restoreFileChooserPortal"));
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("fileChooserPortalMessage"));
        QCOMPARE(tabTitleOf(install), QStringLiteral("File Manager"));
        QVERIFY(status->text().contains(QStringLiteral("system's own file chooser")));
        QVERIFY(!status->text().contains(QStringLiteral("not installed")));
        QCOMPARE(install->text(), QStringLiteral("Set Up"));
        QVERIFY(install->isEnabled());
        QVERIFY(!restore->isEnabled());

        install->click();
        QCOMPARE(m_questions.size(), 1);
        QVERIFY(m_questions.constFirst().contains(QStringLiteral("closes file dialogs")));
        QCOMPARE(m_restarts, 1);
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QCOMPARE(PortalSetup::state(portalPaths()), PortalSetup::State::Installed);
        QVERIFY(status->text().contains(QStringLiteral("Plasma Opus serves the system file chooser")));
        QVERIFY(!install->isEnabled());
        QVERIFY(restore->isEnabled());

        restore->click();
        QCOMPARE(m_restarts, 2);
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QVERIFY(message->text().contains(QStringLiteral("previous system file chooser")));
        QCOMPARE(PortalSetup::state(portalPaths()), PortalSetup::State::NotInstalled);
        QVERIFY(!KSharedConfig::openStateConfig()->hasGroup(PortalSetup::backupGroupName()));
        QVERIFY(install->isEnabled());
        QVERIFY(!restore->isEnabled());
    }

    void changesNothingWhenTheRestartIsCanceled()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        setPortalHooks(dialog, true, false);
        dialog.findChild<QPushButton *>(QStringLiteral("installFileChooserPortal"))->click();
        QCOMPARE(m_questions.size(), 1);
        QCOMPARE(m_restarts, 0);
        QCOMPARE(PortalSetup::state(portalPaths()), PortalSetup::State::NotInstalled);
        QVERIFY(dialog.findChild<KMessageWidget *>(QStringLiteral("fileChooserPortalMessage"))->isHidden());
    }

    void completesAndRestoresAPartialSetupWithoutThePortal()
    {
        write(portalPaths().portalFile(), PortalSetup::portalFileContent());
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        setPortalHooks(dialog, false);
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("fileChooserPortalStatus"));
        auto *install = dialog.findChild<QPushButton *>(QStringLiteral("installFileChooserPortal"));
        auto *restore = dialog.findChild<QPushButton *>(QStringLiteral("restoreFileChooserPortal"));
        QCOMPARE(install->text(), QStringLiteral("Complete Setup"));
        QVERIFY(!install->isEnabled());
        QVERIFY(status->text().contains(QStringLiteral("partly set up")));
        QVERIFY(status->text().contains(QStringLiteral("xdg-desktop-portal is not available")));

        setPortalHooks(dialog, true);
        QVERIFY(install->isEnabled());
        QVERIFY(restore->isEnabled());
        setPortalHooks(dialog, false);
        restore->click();
        QCOMPARE(m_restarts, 1);
        QCOMPARE(PortalSetup::state(portalPaths()), PortalSetup::State::NotInstalled);
    }

    void reportsAFailedRestart()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        setPortalHooks(dialog, true, true, QStringLiteral("Could not restart xdg-desktop-portal."));
        dialog.findChild<QPushButton *>(QStringLiteral("installFileChooserPortal"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("fileChooserPortalMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Warning);
        QVERIFY(message->text().contains(QStringLiteral("Could not restart xdg-desktop-portal.")));
        QVERIFY(message->text().contains(QStringLiteral("Log out")));
        QCOMPARE(PortalSetup::state(portalPaths()), PortalSetup::State::Installed);
    }

    void forgetsTheRememberedFolders()
    {
        {
            MainWindow window;
            SettingsDialog dialog(placesOf(window), &window);
            auto *startFolder = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_FileChooserStartFolder"));
            QVERIFY(startFolder);
            QCOMPARE(startFolder->count(), 2);
            QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("forgetFileChooserFolders"))->isEnabled());
        }

        const KSharedConfigPtr state = KSharedConfig::openStateConfig();
        KConfigGroup group = state->group(FileChooser::stateGroupName());
        FileChooser::rememberFolder(group, QStringLiteral("org.example.App"), QUrl::fromLocalFile(QDir::homePath()));
        QVERIFY(state->sync());

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *forget = dialog.findChild<QPushButton *>(QStringLiteral("forgetFileChooserFolders"));
        QVERIFY(forget->isEnabled());
        forget->click();
        QCOMPARE(dialog.findChild<KMessageWidget *>(QStringLiteral("fileChooserPortalMessage"))->messageType(), KMessageWidget::Positive);
        QVERIFY(!forget->isEnabled());
        state->reparseConfiguration();
        QVERIFY(!state->hasGroup(FileChooser::stateGroupName()));
    }

    void notesASessionOutsidePlasma()
    {
        qputenv("XDG_CURRENT_DESKTOP", "GNOME");
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        setPortalHooks(dialog, true);
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("fileChooserPortalStatus"))->text().contains(QStringLiteral("not a Plasma session")));
    }

    void notificationsTabChoosesTheReportedOperations()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("notificationsIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(page)), QStringLiteral("Notifications"));
        QCOMPARE(tabs->tabText(0), QStringLiteral("File Manager"));

        auto *checksums = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NotifyChecksums"));
        QVERIFY(checksums->isChecked());
        QVERIFY(OperationJob::reports(OperationJob::Kind::Checksums));
        checksums->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::notifyChecksums());
        QVERIFY(!OperationJob::reports(OperationJob::Kind::Checksums));
        QVERIFY(OperationJob::reports(OperationJob::Kind::FolderSync));
    }

    void reportsMissingNotificationSettings()
    {
        if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), QStringLiteral("kcm_notifications")).isValid()) {
            QSKIP("Plasma's Notifications settings are installed on this system.");
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openNotificationSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("notificationsMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("Notifications settings are not installed")));
    }

    void systemTrayTabIsOnTheKdePlasmaPage()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("systemTrayPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 2);
        QCOMPARE(tabs->tabText(2), QStringLiteral("System Tray"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("Notifications"));
        QCOMPARE(tabs->widget(0)->objectName(), QStringLiteral("fileManagerIntegrationPage"));

        auto *show = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTrayIcon"));
        auto *startHidden = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_StartHiddenInTray"));
        QVERIFY(!startHidden->isChecked());
        QVERIFY(startHidden->isEnabled());
        show->setChecked(false);
        QVERIFY(!startHidden->isEnabled());
        QVERIFY(!dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_CloseToTray"))->isEnabled());
        // The offscreen platform is not Wayland.
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("trayWaylandNote"))->isHidden());
    }

    void shortcutsTabTurnsTheGlobalShortcutOffAndOn()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        // As when the window opens Preferences.
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("shortcutsIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 3);
        QCOMPARE(tabs->tabText(3), QStringLiteral("Shortcuts"));

        DesktopTray *tray = window.desktopTray();
        auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_GlobalShortcut"));
        auto *key = dialog.findChild<KKeySequenceWidget *>(QStringLiteral("globalShortcutKey"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("globalShortcutStatus"));
        auto *apply = dialog.button(QDialogButtonBox::Apply);
        QVERIFY(enabled->isChecked());
        QVERIFY(key->isEnabled());
        QVERIFY(tray->globalShortcutRegistered());
        QCOMPARE(key->keySequence(), tray->globalShortcut());
        if (!DesktopTray::globalShortcutServiceAvailable()) {
            QVERIFY(status->text().contains(QStringLiteral("not running")));
        }

        enabled->setChecked(false);
        QVERIFY(!key->isEnabled());
        apply->click();
        QVERIFY(!Settings::globalShortcut());
        QVERIFY(!tray->globalShortcutRegistered());
        QTRY_VERIFY(status->text().contains(QStringLiteral("is off")));

        // A key chosen with the shortcut turned on again is the one it gets.
        const QKeySequence chosen(Qt::CTRL | Qt::ALT | Qt::Key_F12);
        enabled->setChecked(true);
        key->setKeySequence(chosen);
        apply->click();
        QVERIFY(tray->globalShortcutRegistered());
        QTRY_VERIFY(!status->text().contains(QStringLiteral("is off")));
        if (DesktopTray::globalShortcutServiceAvailable()) {
            QCOMPARE(tray->globalShortcut(), chosen);
        } else {
            // Without KDE's service, no key is assigned, and the widget shows that.
            QTRY_COMPARE(key->keySequence(), QKeySequence());
        }

        key->setKeySequence(QKeySequence());
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(key->keySequence(), DesktopTray::defaultGlobalShortcut());
    }

    void reportsMissingShortcutSettings()
    {
        if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), QStringLiteral("kcm_keys")).isValid()) {
            QSKIP("Plasma's Shortcuts settings are installed on this system.");
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openShortcutSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("shortcutsMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("Shortcuts settings are not installed")));
    }

    void activitiesTabEditsTheAssociations()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            KSharedConfig::openConfig()->deleteGroup(QLatin1String(ActivityAssociations::Group));
        });
        const QUrl folder = QUrl::fromLocalFile(QDir::tempPath());
        ActivityAssociations::Association work;
        work.activityId = QStringLiteral("work");
        work.activityName = QStringLiteral("Work");
        work.secondaryFolder = folder;
        ActivityAssociations::save(KSharedConfig::openConfig()->group(QLatin1String(ActivityAssociations::Group)), {work});

        MainWindow window;
        auto *source = new FakeActivities;
        source->current = QStringLiteral("home");
        source->names = {{QStringLiteral("home"), QStringLiteral("Home")}, {QStringLiteral("work"), QStringLiteral("Work")}};
        window.activityController()->setSource(source);
        // View › Activity Associations opens the tab.
        window.showActivityAssociations();
        auto *dialog = qobject_cast<SettingsDialog *>(KConfigDialog::exists(QLatin1String(SettingsDialog::Name)));
        QVERIFY(dialog);
        auto *page = dialog->findChild<QWidget *>(QStringLiteral("workspaceActivitiesPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->currentWidget(), page);
        QCOMPARE(tabs->indexOf(page), 4);
        QCOMPARE(tabs->tabText(4), QStringLiteral("Activities"));
        QCOMPARE(tabs->widget(0)->objectName(), QStringLiteral("fileManagerIntegrationPage"));

        auto *enabled = dialog->findChild<QCheckBox *>(QStringLiteral("kcfg_ActivityAssociations"));
        auto *offer = dialog->findChild<QCheckBox *>(QStringLiteral("kcfg_OfferActivityAssociation"));
        auto *list = page->findChild<QListWidget *>(QStringLiteral("activityList"));
        auto *mode = page->findChild<QComboBox *>(QStringLiteral("activityBehaviorMode"));
        auto *apply = dialog->button(QDialogButtonBox::Apply);
        QVERIFY(!enabled->isChecked());
        QVERIFY(!offer->isEnabled());
        QVERIFY(!list->isEnabled());
        enabled->setChecked(true);
        QVERIFY(offer->isEnabled());
        QVERIFY(list->isEnabled());
        QCOMPARE(list->count(), 2);
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(), QStringLiteral("home"));

        // Edits apply with the other settings.
        apply->click();
        QVERIFY(Settings::activityAssociations());
        QVERIFY(!apply->isEnabled());
        page->findChild<KUrlRequester *>(QStringLiteral("activityPrimaryFolder"))->setUrl(folder);
        mode->setCurrentIndex(mode->findData(int(WorkspaceCatalog::BehaviorMode::None)));
        QVERIFY(apply->isEnabled());
        apply->click();
        QVERIFY(!apply->isEnabled());
        auto stored = ActivityAssociations::load(KSharedConfig::openConfig()->group(QLatin1String(ActivityAssociations::Group)));
        QCOMPARE(stored.size(), 2);
        const auto *home = ActivityAssociations::find(stored, QStringLiteral("home"));
        QVERIFY(home);
        QCOMPARE(home->primaryFolder, folder);
        QVERIFY(!home->inheritBehaviors);
        QCOMPARE(home->behaviors.mode, WorkspaceCatalog::BehaviorMode::None);
        QCOMPARE(ActivityAssociations::find(stored, QStringLiteral("work"))->secondaryFolder, folder);

        // Restore Defaults turns associations off but keeps them, like workspaces.
        dialog->button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(!enabled->isChecked());
        QVERIFY(!list->isEnabled());
        QCOMPARE(page->findChild<ActivityAssociationsEditor *>()->associations().size(), 2);

        // Clearing an association saves its removal.
        enabled->setChecked(true);
        list->setCurrentRow(list->currentRow() == 0 ? 1 : 0);
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(), QStringLiteral("work"));
        page->findChild<QPushButton *>(QStringLiteral("activityClearAssociation"))->click();
        apply->click();
        stored = ActivityAssociations::load(KSharedConfig::openConfig()->group(QLatin1String(ActivityAssociations::Group)));
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored.at(0).activityId, QStringLiteral("home"));
    }

    void reportsMissingActivitySettings()
    {
        if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), QStringLiteral("kcm_activities")).isValid()) {
            QSKIP("Plasma's Activities settings are installed on this system.");
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openActivitySettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("activitiesMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("Activities settings are not installed")));
    }

    void kdeConnectTabTurnsSharingAndTheRememberedDeviceOff()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("kdeConnectIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 5);
        QCOMPARE(tabs->tabText(5), QStringLiteral("KDE Connect"));

        KConfigGroup state = KSharedConfig::openStateConfig()->group(QStringLiteral("KdeConnectShare"));
        state.writeEntry("LastDevice", QStringLiteral("phone"));
        QAction *send = window.actionCollection()->action(QStringLiteral("kdeconnect_send"));
        auto *enabled = page->findChild<QCheckBox *>(QStringLiteral("kcfg_KdeConnectSharing"));
        auto *remember = page->findChild<QCheckBox *>(QStringLiteral("kcfg_KdeConnectRememberDevice"));
        auto *apply = dialog.button(QDialogButtonBox::Apply);
        QVERIFY(enabled->isChecked());
        QVERIFY(remember->isChecked());
        QVERIFY(send->isVisible());

        enabled->setChecked(false);
        QVERIFY(!remember->isEnabled());
        apply->click();
        QVERIFY(!Settings::kdeConnectSharing());
        QVERIFY(!send->isVisible());
        QVERIFY(!send->isEnabled());
        // Only turning remembering off forgets the device.
        QCOMPARE(state.readEntry("LastDevice"), QStringLiteral("phone"));

        enabled->setChecked(true);
        remember->setChecked(false);
        apply->click();
        QVERIFY(send->isVisible());
        QVERIFY(!Settings::kdeConnectRememberDevice());
        QVERIFY(!state.hasKey("LastDevice"));

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(enabled->isChecked());
        QVERIFY(remember->isChecked());
    }

    void reportsMissingKdeConnectSettings()
    {
        for (const QString &kind : {QStringLiteral("systemsettings"), QStringLiteral("systemsettings_qwidgets")}) {
            if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/") + kind, QStringLiteral("kcm_kdeconnect")).isValid()) {
                QSKIP("KDE Connect's settings are installed on this system.");
            }
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openKdeConnectSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("kdeConnectSettingsMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("KDE Connect's settings are not installed")));
    }

    void krunnerTabChoosesWhatIsOffered()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Workspaces"));
            KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Favorites"));
            KSharedConfig::openConfig()->sync();
        });
        const KSharedConfig::Ptr config = KSharedConfig::openConfig();
        config->deleteGroup(QStringLiteral("Workspaces"));
        config->deleteGroup(QStringLiteral("Favorites"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Photos"), QStringLiteral("PH"), error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Clients"), QStringLiteral("CL"), error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::save(config->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        const QUrl home = QUrl::fromLocalFile(QDir::homePath());
        const QUrl temp = QUrl::fromLocalFile(QDir::tempPath());
        {
            FavoritesModel favorites(config->group(QStringLiteral("Favorites")));
            QVERIFY(favorites.add(home));
        }
        config->sync();

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("plasmaSearchPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 6);
        QCOMPARE(tabs->tabText(6), QStringLiteral("KRunner"));

        auto *enabled = page->findChild<QCheckBox *>(QStringLiteral("kcfg_RunnerEnabled"));
        auto *offerWorkspaces = page->findChild<QCheckBox *>(QStringLiteral("kcfg_RunnerWorkspaces"));
        auto *offerFavorites = page->findChild<QCheckBox *>(QStringLiteral("kcfg_RunnerFavorites"));
        auto *workspaces = page->findChild<QListWidget *>(QStringLiteral("kcfg_RunnerHiddenWorkspaces"));
        auto *favorites = page->findChild<QListWidget *>(QStringLiteral("kcfg_RunnerHiddenFavorites"));
        auto *apply = dialog.button(QDialogButtonBox::Apply);
        QVERIFY(enabled->isChecked());
        QCOMPARE(workspaces->count(), catalog.workspaces.size());
        QCOMPARE(workspaces->item(0)->data(Qt::UserRole).toString(), catalog.workspaces.at(0).id);
        QCOMPARE(favorites->count(), 1);
        QCOMPARE(favorites->item(0)->data(Qt::UserRole).toString(), RunnerMatches::locationId(home));

        // Favorites added while the dialog is open are listed and offered.
        QVERIFY(window.favorites()->add(temp));
        QCOMPARE(favorites->count(), 2);
        QCOMPARE(favorites->item(1)->checkState(), Qt::Checked);

        workspaces->item(0)->setCheckState(Qt::Unchecked);
        favorites->item(1)->setCheckState(Qt::Unchecked);
        QVERIFY(apply->isEnabled());
        apply->click();
        QCOMPARE(Settings::runnerHiddenWorkspaces(), QStringList{catalog.workspaces.at(0).id});
        QCOMPARE(Settings::runnerHiddenFavorites(), QStringList{RunnerMatches::locationId(temp)});

        // A Favorite that is removed stays hidden if it comes back.
        QVERIFY(window.favorites()->remove(temp));
        QCOMPARE(favorites->count(), 1);
        QVERIFY(!apply->isEnabled());
        QVERIFY(window.favorites()->add(temp));
        QCOMPARE(favorites->item(1)->checkState(), Qt::Unchecked);

        offerWorkspaces->setChecked(false);
        QVERIFY(!workspaces->isEnabled());
        QVERIFY(favorites->isEnabled());
        enabled->setChecked(false);
        QVERIFY(!offerWorkspaces->isEnabled());
        QVERIFY(!offerFavorites->isEnabled());
        QVERIFY(!favorites->isEnabled());
        apply->click();
        QVERIFY(!Settings::runnerEnabled());

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(enabled->isChecked());
        QVERIFY(offerWorkspaces->isChecked());
        QVERIFY(workspaces->isEnabled());
        QCOMPARE(workspaces->item(0)->checkState(), Qt::Checked);
        QCOMPARE(favorites->item(1)->checkState(), Qt::Checked);
    }

    void reportsMissingPlasmaSearchSettings()
    {
        for (const QString &kind : {QStringLiteral("systemsettings"), QStringLiteral("systemsettings_qwidgets")}) {
            if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/") + kind, QStringLiteral("kcm_plasmasearch")).isValid()) {
                QSKIP("Plasma's search settings are installed on this system.");
            }
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openPlasmaSearchSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("plasmaSearchMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("search settings are not installed")));
    }

    void onlineAccountsTabChoosesTheShownAccounts()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
#ifdef OPUSFM_HAVE_ACCOUNTS
        const auto write = [this](const QString &name, const QByteArray &content) {
            QFile file(m_accountsDir.filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
        };
        QVERIFY(write(QStringLiteral("nextcloud.provider"), "<?xml version=\"1.0\"?><provider id=\"nextcloud\"><name>Nextcloud</name></provider>"));
        QVERIFY(write(QStringLiteral("google.provider"), "<?xml version=\"1.0\"?><provider id=\"google\"><name>Google</name></provider>"));
        QVERIFY(write(QStringLiteral("mail.provider"), "<?xml version=\"1.0\"?><provider id=\"mail\"><name>Mail</name></provider>"));
        QVERIFY(write(QStringLiteral("nextcloud-storage.service"),
                      "<?xml version=\"1.0\"?><service id=\"nextcloud-storage\"><type>dav-storage</type><name>Storage</name>"
                      "<provider>nextcloud</provider></service>"));
        QVERIFY(write(QStringLiteral("google-drive.service"),
                      "<?xml version=\"1.0\"?><service id=\"google-drive\"><type>google-drive</type><name>Drive</name>"
                      "<provider>google</provider></service>"));
        QString googleId;
        {
            Accounts::Manager manager(Accounts::Manager::DisableNotifications);
            const Accounts::AccountIdList existing = manager.accountList();
            for (const Accounts::AccountId id : existing) {
                manager.account(id)->remove();
                QVERIFY(manager.account(id)->syncAndBlock());
            }
            Accounts::Account *nextcloud = manager.createAccount(QStringLiteral("nextcloud"));
            nextcloud->setDisplayName(QStringLiteral("alex"));
            nextcloud->setEnabled(true);
            nextcloud->setValue(QStringLiteral("dav/host"), QStringLiteral("cloud.example.org"));
            QVERIFY(nextcloud->syncAndBlock());
            Accounts::Account *google = manager.createAccount(QStringLiteral("google"));
            google->setDisplayName(QStringLiteral("someone@gmail.com"));
            google->setEnabled(true);
            QVERIFY(google->syncAndBlock());
            googleId = QString::number(google->id());
            Accounts::Account *mail = manager.createAccount(QStringLiteral("mail"));
            mail->setDisplayName(QStringLiteral("letters"));
            QVERIFY(mail->syncAndBlock());
        }
#endif
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("onlineAccountsIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 7);
        QCOMPARE(tabs->tabText(7), QStringLiteral("Online Accounts"));

        auto *menu = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("online_accounts")));
        auto *enabled = page->findChild<QCheckBox *>(QStringLiteral("kcfg_OnlineAccountsEnabled"));
        auto *accounts = page->findChild<QListWidget *>(QStringLiteral("kcfg_OnlineAccountsHidden"));
        auto *refresh = page->findChild<QPushButton *>(QStringLiteral("refreshOnlineAccounts"));
        auto *status = page->findChild<QLabel *>(QStringLiteral("onlineAccountsStatus"));
        auto *apply = dialog.button(QDialogButtonBox::Apply);
        // Nothing is read or offered before opting in.
        QVERIFY(!enabled->isChecked());
        QVERIFY(!accounts->isEnabled());
        QVERIFY(!refresh->isEnabled());
        QCOMPARE(accounts->count(), 0);
        QVERIFY(!menu->isVisible());

        enabled->setChecked(true);
        QVERIFY(accounts->isEnabled());
        QVERIFY(refresh->isEnabled());
        apply->click();
        QVERIFY(Settings::onlineAccountsEnabled());
        QVERIFY(menu->isVisible());
#ifdef OPUSFM_HAVE_ACCOUNTS
        // Only the accounts with file storage are listed, in provider order.
        QCOMPARE(accounts->count(), 2);
        QCOMPARE(accounts->item(0)->text(), QStringLiteral("someone@gmail.com (Google)"));
        QCOMPARE(accounts->item(0)->data(Qt::UserRole).toString(), googleId);
        QCOMPARE(accounts->item(1)->text(), QStringLiteral("alex (Nextcloud)"));
        QVERIFY(status->text().contains(QStringLiteral("letters")));

        accounts->item(0)->setCheckState(Qt::Unchecked);
        apply->click();
        QCOMPARE(Settings::onlineAccountsHidden(), QStringList{googleId});
        Q_EMIT menu->menu()->aboutToShow();
        QStringList sections;
        for (const QAction *action : menu->menu()->actions()) {
            if (action->isSeparator() && !action->text().isEmpty()) {
                sections.append(action->text());
            }
        }
        QCOMPARE(sections, QStringList{QStringLiteral("alex (Nextcloud)")});
        // Hiding an account leaves it in KDE Online Accounts.
        QString error;
        QCOMPARE(OnlineAccounts::readAccounts(&error).size(), 3);

        // Refreshing keeps the choice.
        refresh->click();
        QCOMPARE(accounts->item(0)->checkState(), Qt::Unchecked);
#else
        QVERIFY(status->text().contains(QStringLiteral("without KDE Online Accounts support")));
#endif

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(!enabled->isChecked());
        QVERIFY(!accounts->isEnabled());
        apply->click();
        QVERIFY(!menu->isVisible());
        QVERIFY(Settings::onlineAccountsHidden().isEmpty());
    }

    void reportsMissingOnlineAccountsSettings()
    {
        for (const QString &kind : {QStringLiteral("systemsettings"), QStringLiteral("systemsettings_qwidgets")}) {
            if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/") + kind, QStringLiteral("kcm_kaccounts")).isValid()) {
                QSKIP("KDE's Online Accounts settings are installed on this system.");
            }
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openOnlineAccountsSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("onlineAccountsMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("Online Accounts settings are not installed")));
    }

    void mountManagerTabChoosesTheListedFileSystems()
    {
        const auto config = KSharedConfig::openConfig();
        KConfigGroup customization = config->group(QStringLiteral("ToolbarCustomization"));
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto backup = KSharedConfig::openConfig(root.filePath(QStringLiteral("toolbar-backup")));
        KConfigGroup savedCustomization = backup->group(QStringLiteral("ToolbarCustomization"));
        customization.copyTo(&savedCustomization);
        customization.deleteGroup();
        const auto restore = qScopeGuard([&] {
            customization.deleteGroup();
            savedCustomization.copyTo(&customization);
            config->sync();
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("mountManagerIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 8);
        QCOMPARE(tabs->tabText(8), QStringLiteral("Mount Manager"));

        auto *removable = page->findChild<QCheckBox *>(QStringLiteral("kcfg_MountRemovableOnly"));
        auto *hideSystem = page->findChild<QCheckBox *>(QStringLiteral("kcfg_MountHideSystem"));
        auto *toolbar = page->findChild<QCheckBox *>(QStringLiteral("mountPointsToolbarButton"));
        auto *apply = dialog.button(QDialogButtonBox::Apply);
        QVERIFY(removable && hideSystem && toolbar);
        QVERIFY(removable->isChecked());
        QVERIFY(hideSystem->isChecked());
        QVERIFY(toolbar->isEnabled());
        QVERIFY(toolbar->isChecked());

        // System file systems are listed, but never offered for mounting or unmounting.
        const auto systemDevices = [&window] {
            QList<MountDevice> devices;
            for (const MountDevice &device : window.mountBackend()->devices()) {
                if (device.system) {
                    devices.append(device);
                }
            }
            return devices;
        };
        window.mountBackend()->refresh();
        QVERIFY(systemDevices().isEmpty());
        removable->setChecked(false);
        hideSystem->setChecked(false);
        apply->click();
        QVERIFY(!Settings::mountRemovableOnly());
        QVERIFY(!Settings::mountHideSystem());
        const QList<MountDevice> shown = systemDevices();
        QVERIFY(std::any_of(shown.cbegin(), shown.cend(), [](const MountDevice &device) {
            return device.mountPoint == QLatin1String("/proc");
        }));
        QVERIFY(std::none_of(shown.cbegin(), shown.cend(), [](const MountDevice &device) {
            return device.canMount || device.canUnmount;
        }));
        hideSystem->setChecked(true);
        apply->click();
        QVERIFY(systemDevices().isEmpty());

        // The Mount Points menu leaves the main toolbar and comes back in its place.
        QAction *menu = window.actionCollection()->action(QStringLiteral("mount_points"));
        QAction *preview = window.actionCollection()->action(QStringLiteral("show_preview_panel"));
        QList<QAction *> entries = window.toolBar()->actions();
        QVERIFY(entries.contains(menu));
        const qsizetype position = entries.indexOf(menu);
        toolbar->setChecked(false);
        apply->click();
        QVERIFY(!window.toolBar()->actions().contains(menu));
        QTRY_VERIFY(customization.hasKey("Actions"));
        QVERIFY(!customization.readEntry("Actions", QStringList()).contains(QStringLiteral("action:mount_points")));
        toolbar->setChecked(true);
        apply->click();
        entries = window.toolBar()->actions();
        QCOMPARE(entries.indexOf(menu), position);
        QCOMPARE(entries.indexOf(preview) + 1, position);
        QTRY_VERIFY(customization.readEntry("Actions", QStringList()).contains(QStringLiteral("action:mount_points")));

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(removable->isChecked());
        QVERIFY(hideSystem->isChecked());
        QVERIFY(toolbar->isChecked());

        // Without a main window the toolbar option can't apply.
        KFilePlacesModel model;
        SettingsDialog detached(&model, nullptr);
        QVERIFY(!detached.findChild<QCheckBox *>(QStringLiteral("mountPointsToolbarButton"))->isEnabled());
    }

    void kwinTabManagesTheScriptAndWindowManagement()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Workspaces"));
            KSharedConfig::openConfig()->deleteGroup(QStringLiteral("KWinVirtualDesktops"));
            KSharedConfig::openConfig()->sync();
        });
        const KSharedConfig::Ptr config = KSharedConfig::openConfig();
        config->deleteGroup(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Photos"), QStringLiteral("PH"), error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Clients"), QStringLiteral("CL"), error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::save(config->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        const QString photos = catalog.workspaces.at(0).id;
        const QString clients = catalog.workspaces.at(1).id;
        // A desktop that KWin no longer lists keeps its workspace.
        KConfigGroup assigned = config->group(QStringLiteral("KWinVirtualDesktops"));
        KWinScript::writeDesktopWorkspaces(assigned, {{QStringLiteral("gone"), photos}});
        config->sync();

        auto fake = std::make_shared<FakeKWin>();
        MainWindow window;
        window.setKWinBackend(std::make_unique<FakeKWinBackend>(fake));
        KWinIntegration *kwin = window.kwinIntegration();
        kwin->DesktopsChanged(QStringLiteral(R"([{"id":"d1","name":"Work"},{"id":"d2","name":"Play"}])"));
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        int questions = 0;
        bool confirm = false;
        dialog.setKWinRemovalConfirmation([&] {
            ++questions;
            return confirm;
        });
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("kwinIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 9);
        QCOMPARE(tabs->tabText(9), QStringLiteral("KWin"));

        auto *status = page->findChild<QLabel *>(QStringLiteral("kwinStatus"));
        auto *install = page->findChild<QPushButton *>(QStringLiteral("kwinInstall"));
        auto *enable = page->findChild<QPushButton *>(QStringLiteral("kwinEnable"));
        auto *disable = page->findChild<QPushButton *>(QStringLiteral("kwinDisable"));
        auto *remove = page->findChild<QPushButton *>(QStringLiteral("kwinRemove"));
        auto *refresh = page->findChild<QPushButton *>(QStringLiteral("kwinRefresh"));
        auto *message = page->findChild<KMessageWidget *>(QStringLiteral("kwinMessage"));
        auto *apply = dialog.button(QDialogButtonBox::Apply);
        QVERIFY(status && install && enable && disable && remove && refresh && message);

        // Nothing is installed until asked.
        QCOMPARE(status->text(), kwin->statusText());
        QVERIFY(!install->isHidden());
        QVERIFY(install->isEnabled());
        QCOMPARE(install->text(), QStringLiteral("Install and Enable"));
        QVERIFY(enable->isHidden());
        QVERIFY(disable->isHidden());
        QVERIFY(!remove->isEnabled());
        QVERIFY(!fake->pendingInstall);

        install->click();
        QVERIFY(fake->pendingInstall);
        QVERIFY(!fake->installedAsUpdate);
        QVERIFY(!install->isEnabled());
        fake->installed = KWinIntegration::bundledVersion();
        std::exchange(fake->pendingInstall, {})(QString());
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QVERIFY(message->text().contains(QStringLiteral("installed and enabled")));
        QVERIFY(fake->enabled);
        QVERIFY(install->isHidden());
        QVERIFY(!disable->isHidden());
        QVERIFY(enable->isHidden());
        QVERIFY(remove->isEnabled());
        QCOMPARE(status->text(), kwin->statusText());

        disable->click();
        QVERIFY(!fake->enabled);
        QVERIFY(!enable->isHidden());
        QVERIFY(disable->isHidden());
        enable->click();
        QVERIFY(fake->enabled);
        QVERIFY(enable->isHidden());

        // An older installed script is offered an update, and a failure is reported.
        fake->installed = KWinIntegration::bundledVersion() - 1;
        refresh->click();
        QVERIFY(!install->isHidden());
        QCOMPARE(install->text(), QStringLiteral("Update"));
        install->click();
        QVERIFY(fake->installedAsUpdate);
        std::exchange(fake->pendingInstall, {})(QStringLiteral("kpackagetool6 failed"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QCOMPARE(message->text(), QStringLiteral("kpackagetool6 failed"));
        QVERIFY(install->isEnabled());

        // Removal asks first.
        remove->click();
        QCOMPARE(questions, 1);
        QVERIFY(!fake->pendingRemove);
        confirm = true;
        remove->click();
        QCOMPARE(questions, 2);
        QVERIFY(fake->pendingRemove);
        QVERIFY(!fake->enabled);
        fake->installed = -1;
        std::exchange(fake->pendingRemove, {})(QString());
        QCOMPARE(message->messageType(), KMessageWidget::Positive);
        QVERIFY(message->text().contains(QStringLiteral("removed")));
        QCOMPARE(install->text(), QStringLiteral("Install and Enable"));
        QVERIFY(!remove->isEnabled());

        fake->running = false;
        refresh->click();
        QVERIFY(!install->isEnabled());
        QCOMPARE(status->text(), kwin->statusText());
        fake->running = true;
        refresh->click();
        QVERIFY(install->isEnabled());

        // The window management options reach the script at its next sync.
        auto *restoreWindows = page->findChild<QCheckBox *>(QStringLiteral("kcfg_KWinRestoreWindows"));
        auto *tile = page->findChild<QCheckBox *>(QStringLiteral("kcfg_KWinTileCompanions"));
        auto *follow = page->findChild<QCheckBox *>(QStringLiteral("kcfg_KWinFollowDesktops"));
        auto *editor = page->findChild<DesktopWorkspacesEditor *>(QStringLiteral("kwinDesktopWorkspaces"));
        QVERIFY(restoreWindows && tile && follow && editor);
        QVERIFY(restoreWindows->isChecked());
        QVERIFY(!tile->isChecked());
        QVERIFY(!follow->isChecked());
        QVERIFY(!editor->isEnabled());
        QVERIFY(editor->findChild<QLabel *>(QStringLiteral("desktopWorkspacesEmpty"))->isHidden());
        auto *play = editor->findChild<WorkspaceCombo *>(QStringLiteral("desktopWorkspace_d2"));
        QVERIFY(play);
        QCOMPARE(play->count(), 3);
        QCOMPARE(play->workspace(), QString());

        restoreWindows->setChecked(false);
        tile->setChecked(true);
        follow->setChecked(true);
        QVERIFY(editor->isEnabled());
        play->setCurrentIndex(play->findData(clients));
        QVERIFY(apply->isEnabled());
        fake->enabled = true;
        QTest::qWait(10);
        const int syncs = fake->processRequests;
        apply->click();
        QVERIFY(!apply->isEnabled());
        QVERIFY(!kwin->options().restoreWindows);
        QVERIFY(kwin->options().tileCompanions);
        QVERIFY(kwin->options().followDesktops);
        QTRY_COMPARE(fake->processRequests, syncs + 1);
        const QJsonObject options = QJsonDocument::fromJson(kwin->Sync(QStringLiteral("1")).toUtf8()).object().value(QStringLiteral("options")).toObject();
        QVERIFY(!options.value(QStringLiteral("restoreWindows")).toBool());
        QVERIFY(options.value(QStringLiteral("tileCompanions")).toBool());
        QVERIFY(options.value(QStringLiteral("followDesktops")).toBool());
        const QMap<QString, QString> saved{{QStringLiteral("d2"), clients}, {QStringLiteral("gone"), photos}};
        QCOMPARE(kwin->desktopWorkspaces(), saved);

        // Desktops added in KWin are listed while the dialog is open.
        kwin->DesktopsChanged(QStringLiteral(R"([{"id":"d1","name":"Work"},{"id":"d2","name":"Play"},{"id":"d3","name":"Archive"}])"));
        QVERIFY(editor->findChild<WorkspaceCombo *>(QStringLiteral("desktopWorkspace_d3")));
        play = editor->findChild<WorkspaceCombo *>(QStringLiteral("desktopWorkspace_d2"));
        QCOMPARE(play->workspace(), clients);
        QVERIFY(!apply->isEnabled());

        // Restore Defaults resets the options and keeps the assignments.
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(restoreWindows->isChecked());
        QVERIFY(!tile->isChecked());
        QVERIFY(!follow->isChecked());
        QVERIFY(!editor->isEnabled());
        QCOMPARE(editor->assignments(), saved);
        apply->click();
        QVERIFY(kwin->options().restoreWindows);
        QVERIFY(!kwin->options().followDesktops);
        QCOMPARE(kwin->desktopWorkspaces(), saved);

        play->setCurrentIndex(0);
        QVERIFY(apply->isEnabled());
        apply->click();
        const QMap<QString, QString> remaining{{QStringLiteral("gone"), photos}};
        QCOMPARE(kwin->desktopWorkspaces(), remaining);

        // Without a main window there is no script to manage.
        KFilePlacesModel model;
        SettingsDialog detached(&model, nullptr);
        QVERIFY(detached.findChild<QLabel *>(QStringLiteral("kwinStatus"))->text().contains(QStringLiteral("Open Preferences from a Plasma Opus window")));
        for (const QString &name : {QStringLiteral("kwinInstall"), QStringLiteral("kwinRemove"), QStringLiteral("kwinRefresh")}) {
            QVERIFY2(!detached.findChild<QPushButton *>(name)->isEnabled(), qPrintable(name));
        }
        QVERIFY(!detached.findChild<QLabel *>(QStringLiteral("desktopWorkspacesEmpty"))->isHidden());
    }

    void kwinTabChoosesTheDropDownWindow()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Workspaces"));
            KSharedConfig::openConfig()->sync();
        });
        const KSharedConfig::Ptr config = KSharedConfig::openConfig();
        config->deleteGroup(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::create(catalog, QStringLiteral("Photos"), QStringLiteral("PH"), error), qPrintable(error));
        QVERIFY2(WorkspaceCatalog::save(config->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        config->sync();
        const QString photos = catalog.workspaces.at(0).id;

        auto fake = std::make_shared<FakeKWin>();
        fake->installed = KWinIntegration::bundledVersion();
        fake->loaded = true;
        fake->enabled = true;
        MainWindow window;
        window.setKWinBackend(std::make_unique<FakeKWinBackend>(fake));
        KWinIntegration *kwin = window.kwinIntegration();
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        // The container has no KDE global shortcut service.
        QKeySequence assigned;
        int writes = 0;
        dialog.setDropDownShortcutHooks(
            [&] {
                return assigned;
            },
            [&](const QKeySequence &key) {
                ++writes;
                assigned = key;
            });

        auto *page = dialog.findChild<QWidget *>(QStringLiteral("kwinIntegrationPage"));
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("dropDownGroup"));
        QVERIFY(page && group);
        QVERIFY(page->isAncestorOf(group));
        auto *enabled = group->findChild<QCheckBox *>(QStringLiteral("kcfg_DropDownEnabled"));
        auto *edge = group->findChild<QComboBox *>(QStringLiteral("kcfg_DropDownEdge"));
        auto *width = group->findChild<QSpinBox *>(QStringLiteral("kcfg_DropDownWidth"));
        auto *height = group->findChild<QSpinBox *>(QStringLiteral("kcfg_DropDownHeight"));
        auto *screen = group->findChild<ScreenCombo *>(QStringLiteral("kcfg_DropDownScreen"));
        auto *slide = group->findChild<QCheckBox *>(QStringLiteral("kcfg_DropDownSlide"));
        auto *hide = group->findChild<QCheckBox *>(QStringLiteral("kcfg_DropDownHideOnFocusLoss"));
        auto *screenEdge = group->findChild<QCheckBox *>(QStringLiteral("kcfg_DropDownScreenEdge"));
        auto *touchEdge = group->findChild<QCheckBox *>(QStringLiteral("kcfg_DropDownTouchEdge"));
        auto *workspace = group->findChild<WorkspaceCombo *>(QStringLiteral("kcfg_DropDownWorkspace"));
        auto *shortcut = group->findChild<KKeySequenceWidget *>(QStringLiteral("dropDownShortcut"));
        auto *apply = dialog.button(QDialogButtonBox::Apply);
        QVERIFY(enabled && edge && width && height && screen && slide && hide && screenEdge && touchEdge && workspace && shortcut);
        const QList<QWidget *> controls{edge, width, height, screen, slide, hide, screenEdge, touchEdge, workspace, shortcut};

        // The defaults are those of the drop-down window, and only the switch is enabled until it is on.
        QVERIFY(!enabled->isChecked());
        QCOMPARE(edge->currentIndex(), int(Settings::EnumDropDownEdge::Top));
        QCOMPARE(edge->count(), 4);
        QCOMPARE(width->value(), 100);
        QCOMPARE(height->value(), 50);
        QCOMPARE(width->minimum(), 10);
        QCOMPARE(screen->screen(), QString());
        QCOMPARE(screen->currentText(), QStringLiteral("Active screen"));
        QVERIFY(slide->isChecked());
        QVERIFY(!hide->isChecked());
        QCOMPARE(workspace->workspace(), QString());
        QCOMPARE(workspace->currentText(), QStringLiteral("Keep its own layout"));
        QVERIFY(workspace->findData(photos) > 0);
        QVERIFY(shortcut->keySequence().isEmpty());
        for (QWidget *control : controls) {
            QVERIFY2(!control->isEnabled(), qPrintable(control->objectName()));
        }
        enabled->setChecked(true);
        for (QWidget *control : controls) {
            QVERIFY2(control->isEnabled(), qPrintable(control->objectName()));
        }

        edge->setCurrentIndex(int(Settings::EnumDropDownEdge::Bottom));
        width->setValue(80);
        height->setValue(30);
        // A screen that is not connected stays chosen.
        screen->setScreen(QStringLiteral("HDMI-A-9"));
        QCOMPARE(screen->currentText(), QStringLiteral("HDMI-A-9 (not connected)"));
        slide->setChecked(false);
        hide->setChecked(true);
        screenEdge->setChecked(true);
        touchEdge->setChecked(true);
        workspace->setWorkspace(photos);
        const QKeySequence key(QStringLiteral("Meta+F12"));
        shortcut->setKeySequence(key);
        QVERIFY(apply->isEnabled());
        QTest::qWait(10);
        const int syncs = fake->processRequests;
        apply->click();
        QVERIFY(!apply->isEnabled());
        QCOMPARE(writes, 1);
        QCOMPARE(assigned, key);
        const KWinScript::DropDown dropDown = kwin->dropDown();
        QVERIFY(dropDown.enabled);
        QCOMPARE(dropDown.edge, KWinScript::Edge::Bottom);
        QCOMPARE(dropDown.width, 80);
        QCOMPARE(dropDown.height, 30);
        QCOMPARE(dropDown.screen, QStringLiteral("HDMI-A-9"));
        QVERIFY(!dropDown.slide);
        QVERIFY(dropDown.hideOnFocusLoss);
        QVERIFY(dropDown.screenEdge);
        QVERIFY(dropDown.touchEdge);
        QCOMPARE(dropDown.workspaceId, photos);
        // The script reads them at its next sync.
        QTRY_COMPARE(fake->processRequests, syncs + 1);
        const QJsonObject sent = QJsonDocument::fromJson(kwin->Sync(QStringLiteral("1")).toUtf8()).object().value(QStringLiteral("dropDown")).toObject();
        QVERIFY(sent.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(sent.value(QStringLiteral("edge")).toString(), QStringLiteral("bottom"));
        QCOMPARE(sent.value(QStringLiteral("height")).toInt(), 30);
        QCOMPARE(sent.value(QStringLiteral("screen")).toString(), QStringLiteral("HDMI-A-9"));

        // A key KDE does not assign shows as none.
        dialog.setDropDownShortcutHooks(
            [] {
                return QKeySequence();
            },
            [&](const QKeySequence &) {
                ++writes;
            });
        shortcut->setKeySequence(QKeySequence(QStringLiteral("Meta+F11")));
        QVERIFY(apply->isEnabled());
        apply->click();
        QCOMPARE(writes, 2);
        QVERIFY(shortcut->keySequence().isEmpty());
        QVERIFY(!apply->isEnabled());

        // Restore Defaults turns it off and clears the shortcut.
        dialog.setDropDownShortcutHooks(
            [&] {
                return assigned;
            },
            [&](const QKeySequence &key) {
                ++writes;
                assigned = key;
            });
        QCOMPARE(shortcut->keySequence(), key);
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(!enabled->isChecked());
        QCOMPARE(edge->currentIndex(), int(Settings::EnumDropDownEdge::Top));
        QCOMPARE(height->value(), 50);
        QCOMPARE(screen->screen(), QString());
        QCOMPARE(workspace->workspace(), QString());
        QVERIFY(shortcut->keySequence().isEmpty());
        QVERIFY(!shortcut->isEnabled());
        apply->click();
        QCOMPARE(writes, 3);
        QVERIFY(assigned.isEmpty());
        QCOMPARE(kwin->dropDown(), KWinScript::DropDown());

        // Without a main window there is no action to assign the shortcut to.
        KFilePlacesModel model;
        SettingsDialog detached(&model, nullptr);
        detached.findChild<QCheckBox *>(QStringLiteral("kcfg_DropDownEnabled"))->setChecked(true);
        QVERIFY(detached.findChild<QComboBox *>(QStringLiteral("kcfg_DropDownEdge"))->isEnabled());
        QVERIFY(!detached.findChild<KKeySequenceWidget *>(QStringLiteral("dropDownShortcut"))->isEnabled());
        QVERIFY(detached.findChild<QLabel *>(QStringLiteral("dropDownShortcutStatus"))
                    ->text()
                    .contains(QStringLiteral("Open Preferences from a Plasma Opus window")));
    }

    void reportsMissingKWinSettings()
    {
        for (const QString &kind : {QStringLiteral("systemsettings"), QStringLiteral("systemsettings_qwidgets")}) {
            for (const QString &module : {QStringLiteral("kcm_kwin_scripts"), QStringLiteral("kcm_kwinrules")}) {
                if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/") + kind, module).isValid()) {
                    QSKIP("KWin's settings are installed on this system.");
                }
            }
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openKWinScriptsSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("kwinMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("script settings are not installed")));
        dialog.findChild<QPushButton *>(QStringLiteral("openKWinRulesSettings"))->click();
        message = dialog.findChild<KMessageWidget *>(QStringLiteral("kwinRulesMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("window rules settings are not installed")));
    }

    void balooTabChoosesTheOfferedSearchExtras()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("balooIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), tabs->count() - 1);
        QCOMPARE(tabs->tabText(tabs->indexOf(page)), QStringLiteral("Baloo"));
        QCOMPARE(tabs->tabText(tabs->indexOf(page) - 1), QStringLiteral("KWin"));

        auto *group = page->findChild<QGroupBox *>(QStringLiteral("balooSearchGroup"));
        auto *ranking = page->findChild<QCheckBox *>(QStringLiteral("kcfg_SearchOfferRanking"));
        auto *fallback = page->findChild<QCheckBox *>(QStringLiteral("kcfg_SearchOfferLiveFallback"));
        auto *status = page->findChild<QLabel *>(QStringLiteral("balooIndexStatus"));
        auto *rank = window.findChild<QCheckBox *>(QStringLiteral("searchRank"));
        QVERIFY(group && ranking && fallback && status && rank);
        QVERIFY(ranking->isChecked());
        QVERIFY(fallback->isChecked());
        QVERIFY(!rank->isHidden());

        dialog.showKWinPage();
        tabs->setCurrentWidget(page);
        dialog.show();
        QTRY_VERIFY(group->isVisible());
        QVERIFY(!status->text().isEmpty());

        ranking->setChecked(false);
        fallback->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::searchOfferRanking());
        QVERIFY(!Settings::searchOfferLiveFallback());
        QVERIFY(rank->isHidden());

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(ranking->isChecked());
        QVERIFY(fallback->isChecked());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!rank->isHidden());

        // The search defaults stay under Tools › Search rather than being duplicated here.
        dialog.findChild<QPushButton *>(QStringLiteral("showSearchSettings"))->click();
        auto *search = dialog.findChild<QWidget *>(QStringLiteral("searchSettingsPage"));
        QVERIFY(search->isVisible());
        QCOMPARE(tabTitleOf(search->findChild<QWidget *>(QStringLiteral("kcfg_DefaultSearchScope"))), QStringLiteral("Search"));
    }

    void reportsMissingFileSearchSettings()
    {
        for (const QString &kind : {QStringLiteral("systemsettings"), QStringLiteral("systemsettings_qwidgets")}) {
            if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/") + kind, QStringLiteral("kcm_baloofile")).isValid()) {
                QSKIP("KDE File Search settings are installed on this system.");
            }
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openBalooSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("balooMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("File Search settings are not installed")));
    }
};

QTEST_MAIN(IntegrationSettingsTest)

#include "integrationsettingstest.moc"
