// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityassociations.h"
#include "activityassociationseditor.h"
#include "activitycontroller.h"
#include "appidentity.h"
#include "apppackages.h"
#include "atuin.h"
#include "cloudactivity.h"
#include "cloudactivitydialog.h"
#include "cloudsync.h"
#include "cloudsyncdialog.h"
#include "cloudsyncrunner.h"
#include "defaultfilemanager.h"
#include "desktoptray.h"
#include "documentformats.h"
#include "editorpanel.h"
#include "editorproviders.h"
#include "favoritesmodel.h"
#include "filechooser.h"
#include "fileencryptiondialog.h"
#include "filetypeplaces.h"
#include "foldergates.h"
#include "foldersyncdialog.h"
#include "folderview.h"
#include "fontpreview.h"
#include "kwinintegration.h"
#include "lanshare.h"
#include "lansharedialog.h"
#include "mailhandoff.h"
#include "mainwindow.h"
#include "mcpsettings.h"
#include "mediabackend.h"
#include "mediaview.h"
#include "mountbackend.h"
#include "nextcloudaccountsdialog.h"
#include "officeapps.h"
#include "onlineaccounts.h"
#include "operationjob.h"
#include "pane.h"
#include "plasmavaultmenu.h"
#include "portalsetup.h"
#include "rclone.h"
#include "remotemountdialog.h"
#include "remotemounts.h"
#include "remoteprofiles.h"
#include "remoteprofilesdialog.h"
#include "rsyncsync.h"
#include "runnermatches.h"
#include "sendbyemaildialog.h"
#include "settings.h"
#include "settingsdialog.h"
#include "settingswidgets.h"
#include "terminalproviders.h"
#include "workspacecatalog.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KConfig>
#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KKeySequenceWidget>
#include <KMessageWidget>
#include <KPageWidget>
#include <KPageWidgetModel>
#include <KPasswordDialog>
#include <KPluginMetaData>
#include <KService>
#include <KSharedConfig>
#include <KSycoca>
#include <KUrlRequester>

#include <KToolBar>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

#include <sys/stat.h>

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

class FakeVaultClient : public VaultClient
{
public:
    using VaultClient::VaultClient;

    QList<PlasmaVaults::Vault> list;
    mutable int reads = 0;

    QList<PlasmaVaults::Vault> vaults(QString *error) const override
    {
        ++reads;
        if (error) {
            error->clear();
        }
        return list;
    }
    void request(Request, const QString &) override
    {
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
        // Each main window then adds the file-type places the settings show.
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("FileTypePlaces"));
        KSharedConfig::openStateConfig()->sync();
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("RemoteProfiles"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("RemoteMounts"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("CloudSync"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("CloudSyncPairs"));
        KSharedConfig::openConfig()->sync();
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

    void balooTabChoosesTheFileTypeCategories()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        KFilePlacesModel *places = placesOf(window);
        SettingsDialog dialog(places, &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        const auto listed = [places] {
            QStringList ids;
            for (int row = 0; row < places->rowCount(); ++row) {
                FileTypePlaces::Category category = FileTypePlaces::Category::Documents;
                if (FileTypePlaces::fromPlaceUrl(places->url(places->index(row, 0)), &category)) {
                    ids.append(FileTypePlaces::id(category));
                }
            }
            ids.sort();
            return ids;
        };
        const auto indexOf = [places](const QString &id) {
            for (int row = 0; row < places->rowCount(); ++row) {
                const QModelIndex index = places->index(row, 0);
                FileTypePlaces::Category category = FileTypePlaces::Category::Documents;
                if (FileTypePlaces::fromPlaceUrl(places->url(index), &category) && FileTypePlaces::id(category) == id) {
                    return index;
                }
            }
            return QModelIndex();
        };
        QCOMPARE(listed().size(), FileTypePlaces::categories().size());

        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("fileTypeCategoriesGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Baloo"));
        auto *list = group->findChild<HiddenItemsList *>(QStringLiteral("kcfg_FileTypeCategoriesHidden"));
        auto *size = group->findChild<QSpinBox *>(QStringLiteral("kcfg_LargeFileSizeMiB"));
        QVERIFY(list && size);
        QCOMPARE(list->count(), FileTypePlaces::categories().size());
        QCOMPARE(size->value(), 100);
        const auto item = [list](const QString &id) -> QListWidgetItem * {
            for (int row = 0; row < list->count(); ++row) {
                if (list->item(row)->data(Qt::UserRole).toString() == id) {
                    return list->item(row);
                }
            }
            return nullptr;
        };
        QCOMPARE(item(QStringLiteral("large-files"))->text(), QStringLiteral("Large Files"));

        item(QStringLiteral("audio"))->setCheckState(Qt::Unchecked);
        item(QStringLiteral("large-files"))->setCheckState(Qt::Unchecked);
        size->setValue(5);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::fileTypeCategoriesHidden(), (QStringList{QStringLiteral("audio"), QStringLiteral("large-files")}));
        QCOMPARE(FileTypePlaces::liveQuery(FileTypePlaces::Category::LargeFiles), QStringLiteral("size>5M"));
        QTRY_VERIFY(!listed().contains(QStringLiteral("audio")));
        QVERIFY(!listed().contains(QStringLiteral("large-files")));
        QCOMPARE(listed().size(), FileTypePlaces::categories().size() - 2);

        // Removing an entry from Places clears it here too, keeping unsaved changes to the others.
        item(QStringLiteral("audio"))->setCheckState(Qt::Checked);
        places->removePlace(indexOf(QStringLiteral("images")));
        QTRY_COMPARE(Settings::fileTypeCategoriesHidden(), (QStringList{QStringLiteral("images"), QStringLiteral("audio"), QStringLiteral("large-files")}));
        QCOMPARE(item(QStringLiteral("images"))->checkState(), Qt::Unchecked);
        QCOMPARE(item(QStringLiteral("audio"))->checkState(), Qt::Checked);
        QCOMPARE(item(QStringLiteral("large-files"))->checkState(), Qt::Unchecked);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::fileTypeCategoriesHidden(), (QStringList{QStringLiteral("images"), QStringLiteral("large-files")}));
        QTRY_VERIFY(listed().contains(QStringLiteral("audio")));
        QVERIFY(!listed().contains(QStringLiteral("images")));

        {
            // Another window keeps the removed entries removed.
            MainWindow second;
            QCOMPARE(listed().size(), FileTypePlaces::categories().size() - 2);
            QCOMPARE(Settings::fileTypeCategoriesHidden(), (QStringList{QStringLiteral("images"), QStringLiteral("large-files")}));
        }

        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(size->value(), 100);
        dialog.button(QDialogButtonBox::Apply)->click();
        QTRY_COMPARE(listed().size(), FileTypePlaces::categories().size());
        QVERIFY(Settings::fileTypeCategoriesHidden().isEmpty());
    }

    void terminalTabChoosesTheSeparateTerminal()
    {
        QTemporaryDir bin;
        QTemporaryDir folder;
        QVERIFY(bin.isValid() && folder.isValid());
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path] {
            qputenv("PATH", path);
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        // Only a fake Konsole, which records how it was started, is installed.
        qputenv("PATH", bin.path().toLocal8Bit());
        const QString output = bin.filePath(QStringLiteral("arguments"));
        QFile konsole(bin.filePath(QStringLiteral("konsole")));
        QVERIFY(konsole.open(QIODevice::WriteOnly));
        konsole.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > '" + output.toLocal8Bit() + "'\n");
        konsole.close();
        QVERIFY(konsole.setPermissions(konsole.permissions() | QFileDevice::ExeOwner));

        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("terminalProviderGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Terminal"));
        QVERIFY(dialog.findChild<QWidget *>(QStringLiteral("terminalIntegrationPage"))->isAncestorOf(group));
        auto *separate = group->findChild<QComboBox *>(QStringLiteral("kcfg_SeparateTerminal"));
        auto *embedded = group->findChild<QComboBox *>(QStringLiteral("kcfg_EmbeddedTerminal"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("terminalProviderStatus"));
        QVERIFY(separate && embedded && status);
        QCOMPARE(separate->count(), TerminalProviders::separateProviders().size());
        QCOMPARE(embedded->count(), TerminalProviders::embeddedProviders().size());
        QCOMPARE(separate->itemText(0), QStringLiteral("Default Terminal"));
        QCOMPARE(separate->itemText(1), QStringLiteral("Konsole"));
        QCOMPARE(separate->itemText(2), QStringLiteral("Yakuake (not installed)"));
        QCOMPARE(separate->itemText(3), QStringLiteral("Kwi (not installed)"));
        QCOMPARE(separate->itemData(3, Qt::ToolTipRole).toString(), QStringLiteral("Kwi is not installed or is not in PATH."));
        QVERIFY(!separate->itemData(1, Qt::ToolTipRole).isValid());
        for (int index = 0; index < embedded->count(); ++index) {
            const TerminalProviders::Provider provider = TerminalProviders::embeddedProviders().at(index);
            QCOMPARE(embedded->itemText(index).endsWith(QStringLiteral("(not installed)")), !TerminalProviders::isPartInstalled(provider));
        }
        QCOMPARE(separate->currentIndex(), 0);
        if (TerminalProviders::isPartInstalled(TerminalProviders::Provider::Konsole)) {
            QCOMPARE(status->text(), QStringLiteral("Open Terminal Here opens Default Terminal, and new terminal panel sessions load the Konsole component."));
        } else {
            QVERIFY(status->text().contains(QStringLiteral("The Konsole terminal component is not installed.")));
        }

        const auto choose = [&dialog, separate](int index) {
            separate->setCurrentIndex(index);
            // The dialog tracks user selection in plain combo boxes.
            Q_EMIT separate->activated(index);
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };
        choose(3);
        QVERIFY(status->text().contains(QStringLiteral("Kwi is not installed or is not in PATH. Open Terminal Here reports this")));
        QCOMPARE(Settings::separateTerminal(), int(Settings::EnumSeparateTerminal::Kwi));
        QCOMPARE(TerminalProviders::separateProvider(), TerminalProviders::Provider::Kwi);
        QAction *openTerminal = window.actionCollection()->action(QStringLiteral("open_terminal"));
        QVERIFY(openTerminal->isEnabled());
        openTerminal->trigger();
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Kwi is not installed or is not in PATH."));
        QVERIFY(!QFile::exists(output));

        choose(1);
        QCOMPARE(TerminalProviders::separateProvider(), TerminalProviders::Provider::Konsole);
        openTerminal->trigger();
        QFile arguments(output);
        QTRY_VERIFY(arguments.exists() && arguments.size() > 0);
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromLocal8Bit(arguments.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts),
                 (QStringList{QStringLiteral("--workdir"), QDir::cleanPath(folder.path())}));

        // The panel's placement and folder following stay under Tools › Embedded Terminal.
        dialog.show();
        dialog.findChild<QPushButton *>(QStringLiteral("showEmbeddedTerminalSettings"))->click();
        auto *placement = dialog.findChild<QWidget *>(QStringLiteral("terminalSettingsPage"));
        QTRY_VERIFY(placement->isVisible());
        QCOMPARE(tabTitleOf(placement->findChild<QWidget *>(QStringLiteral("kcfg_TerminalPlacement"))), QStringLiteral("Embedded Terminal"));
    }

    void terminalTabChoosesTheAtuinHistory()
    {
        QTemporaryDir bin;
        QTemporaryDir data;
        QVERIFY(bin.isValid() && data.isValid());
        const QByteArray path = qgetenv("PATH");
        const bool hadDatabasePath = qEnvironmentVariableIsSet("ATUIN_DB_PATH");
        const QByteArray databasePath = qgetenv("ATUIN_DB_PATH");
        const auto restore = qScopeGuard([&] {
            qputenv("PATH", path);
            if (hadDatabasePath) {
                qputenv("ATUIN_DB_PATH", databasePath);
            } else {
                qunsetenv("ATUIN_DB_PATH");
            }
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        qputenv("PATH", bin.path().toLocal8Bit());
        const QString database = data.filePath(QStringLiteral("history.db"));
        qputenv("ATUIN_DB_PATH", database.toLocal8Bit());
        QFile atuin(bin.filePath(QStringLiteral("atuin")));
        QVERIFY(atuin.open(QIODevice::WriteOnly));
        atuin.write("#!/bin/sh\n");
        atuin.close();
        QVERIFY(atuin.setPermissions(atuin.permissions() | QFileDevice::ExeOwner));

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("atuinHistoryGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Terminal"));
        auto *enabled = group->findChild<QCheckBox *>(QStringLiteral("kcfg_AtuinHistory"));
        auto *session = group->findChild<QComboBox *>(QStringLiteral("kcfg_AtuinSession"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("atuinHistoryStatus"));
        auto *embedded = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_EmbeddedTerminal"));
        QVERIFY(enabled && session && status && embedded);
        QVERIFY(enabled->isChecked());
        QCOMPARE(session->currentIndex(), 0);
        QCOMPARE(session->count(), 3);
        QVERIFY(session->isEnabled());
        QVERIFY(status->text().contains(QStringLiteral("Atuin has no command history at ") + database));

        QFile history(database);
        QVERIFY(history.open(QIODevice::WriteOnly));
        history.close();
        // The status is read again when the group is shown or the option toggled.
        enabled->click();
        enabled->click();
        const bool konsoleInstalled = TerminalProviders::isPartInstalled(TerminalProviders::Provider::Konsole);
        QCOMPARE(status->text().contains(QStringLiteral("Atuin is installed and has command history at ") + database), konsoleInstalled);
        if (!TerminalProviders::isPartInstalled(TerminalProviders::Provider::Kwi)) {
            embedded->setCurrentIndex(1);
            QVERIFY(status->text().contains(QStringLiteral("Kwi")));
            QVERIFY(status->text().contains(QStringLiteral("not installed")));
            embedded->setCurrentIndex(0);
        }

        QAction *search = window.actionCollection()->action(QStringLiteral("search_terminal_history"));
        QVERIFY(search->isVisible() && search->isEnabled());
        enabled->click();
        QVERIFY(!session->isEnabled());
        QCOMPARE(status->text(), QStringLiteral("Search Command History with Atuin is hidden."));
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::atuinHistory());
        QVERIFY(!search->isVisible() && !search->isEnabled());

        enabled->click();
        session->setCurrentIndex(1);
        // The dialog tracks user selection in plain combo boxes.
        Q_EMIT session->activated(1);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::atuinHistory());
        QCOMPARE(Settings::atuinSession(), int(Settings::EnumAtuinSession::NewWhenBusy));
        QCOMPARE(Atuin::sessionMode(), Atuin::SessionMode::NewWhenBusy);
        QVERIFY(search->isVisible() && search->isEnabled());
    }

    void editorTabChoosesTheTextEditor()
    {
        QTemporaryDir bin;
        QTemporaryDir folder;
        QVERIFY(bin.isValid() && folder.isValid());
        const QByteArray path = qgetenv("PATH");
        KConfigGroup general = KSharedConfig::openConfig()->group(QStringLiteral("General"));
        const auto restore = qScopeGuard([&path, &general] {
            qputenv("PATH", path);
            general.deleteEntry("TerminalApplication");
            general.sync();
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        // Fake Kate, Neovim and terminal application record how they were started; KWrite is missing.
        qputenv("PATH", bin.path().toLocal8Bit());
        const QString output = bin.filePath(QStringLiteral("arguments"));
        const auto install = [&bin, &output](const QString &name) {
            QFile script(bin.filePath(name));
            QVERIFY(script.open(QIODevice::WriteOnly));
            script.write("#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\" \"$PWD\" > '" + output.toLocal8Bit() + "'\n");
            script.close();
            QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
        };
        install(QStringLiteral("kate"));
        install(QStringLiteral("nvim"));
        install(QStringLiteral("fake-terminal"));
        general.writeEntry("TerminalApplication", bin.filePath(QStringLiteral("fake-terminal")));
        general.sync();
        const auto recorded = [&output] {
            QFile file(output);
            if (!file.open(QIODevice::ReadOnly)) {
                return QStringList();
            }
            return QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        };
        const QString notes = folder.filePath(QStringLiteral("notes.txt"));
        QFile note(notes);
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.close();

        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(notes)});
        QAction *edit = window.actionCollection()->action(QStringLiteral("edit_file"));
        QTRY_VERIFY(edit->isEnabled());
        QCOMPARE(edit->toolTip(), QStringLiteral("Edit the selected file with the built-in editor"));

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("editorProviderGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Editor"));
        QVERIFY(dialog.findChild<QWidget *>(QStringLiteral("editorIntegrationPage"))->isAncestorOf(group));
        auto *provider = group->findChild<QComboBox *>(QStringLiteral("kcfg_TextEditorProvider"));
        auto *session = group->findChild<QComboBox *>(QStringLiteral("kcfg_TerminalEditorSession"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("editorProviderStatus"));
        QVERIFY(provider && session && status);
        QCOMPARE(provider->count(), EditorProviders::editors().size() + 1);
        QCOMPARE(provider->itemText(0), QStringLiteral("Built-in editor"));
        QCOMPARE(provider->itemText(1), QStringLiteral("KWrite (not installed)"));
        QCOMPARE(provider->itemData(1, Qt::ToolTipRole).toString(), QStringLiteral("KWrite is not installed, or its kwrite command is not in PATH."));
        QCOMPARE(provider->itemText(2), QStringLiteral("Kate"));
        QCOMPARE(provider->itemText(3), QStringLiteral("Neovim"));
        QCOMPARE(provider->itemText(6), QStringLiteral("nano (not installed)"));
        QCOMPARE(status->text(), QStringLiteral("Edit opens text files in the built-in editor."));

        const auto choose = [&dialog](QComboBox *combo, int index) {
            combo->setCurrentIndex(index);
            // The dialog tracks user selection in plain combo boxes.
            Q_EMIT combo->activated(index);
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };
        // An unavailable editor is reported rather than replaced.
        choose(provider, 1);
        QCOMPARE(EditorProviders::preferredEditor(), std::optional(EditorProviders::Editor::KWrite));
        QVERIFY(status->text().contains(QStringLiteral("KWrite is not installed, or its kwrite command is not in PATH. Edit reports this")));
        view->selectUrls({});
        view->selectUrls({QUrl::fromLocalFile(notes)});
        QTRY_COMPARE(edit->toolTip(), QStringLiteral("KWrite is not installed, or its kwrite command is not in PATH."));
        edit->trigger();
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("KWrite is not installed, or its kwrite command is not in PATH."));
        QVERIFY(!QFile::exists(output));

        choose(provider, 2);
        QCOMPARE(status->text(), QStringLiteral("Edit opens text files in Kate."));
        view->selectUrls({});
        view->selectUrls({QUrl::fromLocalFile(notes)});
        QTRY_COMPARE(edit->toolTip(), QStringLiteral("Edit the selected file with Kate"));
        edit->trigger();
        QTRY_VERIFY(recorded().size() == 3);
        QCOMPARE(recorded().mid(0, 2), (QStringList{bin.filePath(QStringLiteral("kate")), notes}));
        QFile::remove(output);

        // A terminal editor runs in KDE's terminal application when the session is separate.
        choose(provider, 3);
        QVERIFY(status->text().contains(QStringLiteral("Neovim")));
        choose(session, 1);
        QVERIFY(EditorProviders::usesSeparateTerminal());
        QCOMPARE(status->text(), QStringLiteral("Edit opens text files with Neovim in a separate terminal window."));
        edit->trigger();
        QTRY_VERIFY(recorded().size() >= 4);
        const QStringList terminal = recorded();
        QCOMPARE(terminal.first(), bin.filePath(QStringLiteral("fake-terminal")));
        QVERIFY(terminal.contains(QStringLiteral("-e")));
        QCOMPARE(terminal.mid(terminal.indexOf(QStringLiteral("-e")) + 1, 2), (QStringList{bin.filePath(QStringLiteral("nvim")), notes}));
        QCOMPARE(QFileInfo(terminal.last()).canonicalFilePath(), QFileInfo(folder.path()).canonicalFilePath());

        // The built-in editor's settings stay on the Editor page.
        dialog.show();
        dialog.findChild<QPushButton *>(QStringLiteral("showBuiltInEditorSettings"))->click();
        auto *builtIn = dialog.findChild<QWidget *>(QStringLiteral("editorSettingsPage"));
        QTRY_VERIFY(builtIn->isVisible());
        QVERIFY(builtIn->isAncestorOf(dialog.findChild<QWidget *>(QStringLiteral("kcfg_EditorMarkdownMode"))));
    }

    void editorTabChoosesTheMarkdownVault()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        const QString vault = QFileInfo(folder.path()).canonicalFilePath();
        QFile note(vault + QStringLiteral("/Home.md"));
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.write("[[Home]] #idea\n");
        note.close();

        MainWindow window;
        window.actionCollection()->action(QStringLiteral("open_editor"))->trigger();
        auto *editor = window.findChild<EditorPanel *>(QStringLiteral("editorPanel"));
        QVERIFY(editor);
        QCOMPARE(editor->vaultRoot(), QString());
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("editorMarkdownGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Editor"));
        auto *folderChooser = group->findChild<KUrlRequester *>(QStringLiteral("kcfg_EditorVaultFolder"));
        auto *dialect = group->findChild<QComboBox *>(QStringLiteral("kcfg_EditorMarkdownDialect"));
        auto *tags = group->findChild<QCheckBox *>(QStringLiteral("kcfg_EditorVaultTags"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("editorMarkdownStatus"));
        QVERIFY(folderChooser && dialect && tags && status);
        QVERIFY2(status->text().startsWith(QStringLiteral("No vault folder is chosen")), qPrintable(status->text()));
        const auto apply = [&dialog] {
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };

        // A missing folder is reported.
        folderChooser->setUrl(QUrl::fromLocalFile(vault + QStringLiteral("/Missing")));
        QVERIFY2(status->text().contains(QStringLiteral("doesn't exist")), qPrintable(status->text()));
        folderChooser->setUrl(QUrl::fromLocalFile(vault));
        QCOMPARE(status->text(), QStringLiteral("Wiki links, backlinks and tags use the Markdown notes in %1.").arg(vault));
        apply();
        QCOMPARE(Settings::editorVaultFolder(), QUrl::fromLocalFile(vault));
        QCOMPARE(editor->vaultRoot(), vault);

        // The editor follows the dialect and the vault panel's lists.
        dialect->setCurrentIndex(1);
        Q_EMIT dialect->activated(1);
        tags->setChecked(false);
        apply();
        QCOMPARE(editor->findChild<QComboBox *>(QStringLiteral("editorMarkdownDialect"))->currentIndex(), 1);
        auto *vaultTabs = editor->findChild<QTabWidget *>(QStringLiteral("editorVaultTabs"));
        QVERIFY(!vaultTabs->isTabVisible(vaultTabs->indexOf(editor->findChild<QWidget *>(QStringLiteral("editorTags")))));

        // The editor's Vault menu chooses the same folder.
        editor->setVault({});
        QVERIFY(Settings::editorVaultFolder().isEmpty());

        // The Markdown mode and draft settings stay on the Editor page.
        dialog.show();
        dialog.findChild<QPushButton *>(QStringLiteral("showMarkdownModeSettings"))->click();
        auto *builtIn = dialog.findChild<QWidget *>(QStringLiteral("editorSettingsPage"));
        QTRY_VERIFY(builtIn->isVisible());
        QVERIFY(builtIn->isAncestorOf(dialog.findChild<QWidget *>(QStringLiteral("kcfg_EditorMarkdownMode"))));
    }

    void editorTabSetsTheWorkspaceVault()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString vault = QFileInfo(folder.path()).canonicalFilePath() + QStringLiteral("/vault");
        QVERIFY(QDir().mkpath(vault));
        for (const QString &name : {QStringLiteral("Home.md"), QStringLiteral("Ideas.md")}) {
            QFile note(vault + QLatin1Char('/') + name);
            QVERIFY(note.open(QIODevice::WriteOnly));
            note.write("[[Home]] #idea\n");
        }
        const QUrl url = QUrl::fromLocalFile(vault);
        const QString locationId = WorkspaceCatalog::locationId(url);
        const auto config = KSharedConfig::openConfig();
        KConfigGroup workspaces = config->group(QStringLiteral("Workspaces"));
        config->group(QStringLiteral("Favorites")).deleteGroup();
        workspaces.deleteGroup();
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Notes"), QStringLiteral("N"), error));
        const QString id = catalog.workspaces.first().id;
        QVERIFY(WorkspaceCatalog::assignLocation(catalog, locationId, id, error));
        QVERIFY(WorkspaceCatalog::setVault(catalog, id, locationId, error));
        QVERIFY(WorkspaceCatalog::activate(catalog, id, error));
        QVERIFY2(WorkspaceCatalog::save(workspaces, catalog, error), qPrintable(error));
        QVERIFY(config->sync());
        const auto restore = qScopeGuard([config] {
            config->reparseConfiguration();
            config->group(QStringLiteral("Favorites")).deleteGroup();
            config->group(QStringLiteral("Workspaces")).deleteGroup();
            config->sync();
        });

        MainWindow window;
        QVERIFY(window.favorites()->add(url));
        QTRY_COMPARE(EditorPanel::workspaceVault(), vault);
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("editorWorkspaceVaultGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Editor"));
        auto *workspace = group->findChild<QComboBox *>(QStringLiteral("editorWorkspaceVaultWorkspace"));
        auto *location = group->findChild<QComboBox *>(QStringLiteral("editorWorkspaceVaultLocation"));
        auto *enabled = group->findChild<QCheckBox *>(QStringLiteral("editorWorkspaceVaultEnabled"));
        auto *ownOptions = group->findChild<QCheckBox *>(QStringLiteral("editorWorkspaceVaultOwnOptions"));
        auto *tags = group->findChild<QCheckBox *>(QStringLiteral("editorWorkspaceVaultTags"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("editorWorkspaceVaultStatus"));
        QVERIFY(workspace && location && enabled && ownOptions && tags && status);
        QCOMPARE(workspace->currentData().toString(), id);
        QCOMPARE(location->currentData().toString(), locationId);
        QVERIFY(enabled->isChecked());
        QVERIFY(!ownOptions->isChecked());
        QTRY_VERIFY2(status->text().contains(QStringLiteral("has 2 Markdown notes")), qPrintable(status->text()));
        const auto apply = [&dialog] {
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };
        const auto stored = [&config, &error] {
            config->reparseConfiguration();
            WorkspaceCatalog::Catalog current;
            WorkspaceCatalog::load(config->group(QStringLiteral("Workspaces")), current, error);
            return current.workspaces.value(0);
        };

        // Turning the vault off keeps its location for later.
        enabled->setChecked(false);
        QVERIFY2(status->text().contains(QStringLiteral("turned off")), qPrintable(status->text()));
        apply();
        QVERIFY(!stored().vaultEnabled);
        QCOMPARE(stored().vaultLocationId, locationId);
        QVERIFY(!EditorPanel::workspaceVaultState().isActive());

        // The workspace's own options reach the editor.
        enabled->setChecked(true);
        ownOptions->setChecked(true);
        QVERIFY(tags->isEnabled());
        tags->setChecked(false);
        apply();
        QVERIFY(stored().vaultEnabled);
        QVERIFY(stored().vaultOptions && !stored().vaultOptions->tags);
        QCOMPARE(EditorPanel::workspaceVault(), vault);
        QVERIFY(EditorPanel::workspaceVaultState().options && !EditorPanel::workspaceVaultState().options->tags);

        // A removed location is reported, and the workspace keeps it as its vault.
        QVERIFY(window.favorites()->remove(url));
        QTRY_VERIFY2(status->text().contains(QStringLiteral("no longer in Favorites")), qPrintable(status->text()));
        QCOMPARE(location->currentData().toString(), locationId);
        QVERIFY(!dialog.button(QDialogButtonBox::Apply)->isEnabled());
    }

    void editorTabChoosesPreviewEditing()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("editorPreviewGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Editor"));
        auto *offer = group->findChild<QCheckBox *>(QStringLiteral("kcfg_PreviewEditing"));
        auto *doubleClick = group->findChild<QCheckBox *>(QStringLiteral("kcfg_PreviewEditOnDoubleClick"));
        auto *returnAfterSave = group->findChild<QCheckBox *>(QStringLiteral("kcfg_PreviewReturnAfterSave"));
        QVERIFY(offer && doubleClick && returnAfterSave);
        QVERIFY(offer->isChecked() && !doubleClick->isChecked() && returnAfterSave->isChecked());
        QVERIFY(doubleClick->isEnabled() && returnAfterSave->isEnabled());

        doubleClick->setChecked(true);
        offer->setChecked(false);
        QVERIFY(!doubleClick->isEnabled() && !returnAfterSave->isEnabled());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::previewEditing());
        QVERIFY(Settings::previewEditOnDoubleClick());
        auto *edit = window.findChild<QToolButton *>(QStringLiteral("previewEdit"));
        QVERIFY(edit && edit->isHidden());

        offer->setChecked(true);
        QVERIFY(doubleClick->isEnabled() && returnAfterSave->isEnabled());
    }

    void officeTabChoosesTheOfficeApplication()
    {
        QTemporaryDir bin;
        QTemporaryDir folder;
        QVERIFY(bin.isValid() && folder.isValid());
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path] {
            qputenv("PATH", path);
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        // A fake LibreOffice records how it was started; Calligra has only its word processor, and ONLYOFFICE is missing.
        qputenv("PATH", bin.path().toLocal8Bit());
        const QString output = bin.filePath(QStringLiteral("arguments"));
        const auto install = [&bin, &output](const QString &name) {
            QFile script(bin.filePath(name));
            QVERIFY(script.open(QIODevice::WriteOnly));
            script.write("#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\" > '" + output.toLocal8Bit() + "'\n");
            script.close();
            QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
        };
        install(QStringLiteral("libreoffice"));
        install(QStringLiteral("calligrawords"));
        const QString report = folder.filePath(QStringLiteral("report.odt"));
        QFile document(report);
        QVERIFY(document.open(QIODevice::WriteOnly));
        document.write("PK");
        document.close();

        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->trigger();
        view->selectUrls({QUrl::fromLocalFile(report)});
        QAction *edit = window.actionCollection()->action(QStringLiteral("edit_file"));
        QTRY_VERIFY(edit->isEnabled());
        QCOMPARE(edit->toolTip(), QStringLiteral("Edit the selected file with LibreOffice"));

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("officeEditorGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Office"));
        QVERIFY(dialog.findChild<QWidget *>(QStringLiteral("officeIntegrationPage"))->isAncestorOf(group));
        auto *provider = group->findChild<QComboBox *>(QStringLiteral("kcfg_OfficeEditor"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("officeEditorStatus"));
        QVERIFY(provider && status);
        QCOMPARE(provider->count(), OfficeApps::apps().size() + 1);
        QCOMPARE(provider->itemText(1), QStringLiteral("LibreOffice"));
        QCOMPARE(provider->itemText(2), QStringLiteral("Calligra"));
        QCOMPARE(provider->itemData(2, Qt::ToolTipRole).toString(), QStringLiteral("Calligra is not installed, or its calligrasheets command is not in PATH."));
        QCOMPARE(provider->itemText(3), QStringLiteral("ONLYOFFICE (not installed)"));
        QCOMPARE(status->text(), QStringLiteral("Edit opens office documents in the first installed application in this list: LibreOffice, Calligra."));

        const auto choose = [&dialog](QComboBox *combo, int index) {
            combo->setCurrentIndex(index);
            Q_EMIT combo->activated(index);
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };
        // A missing application is reported rather than replaced.
        choose(provider, 3);
        QCOMPARE(OfficeApps::preferredApp(), std::optional(OfficeApps::App::OnlyOffice));
        QVERIFY2(status->text().endsWith(QStringLiteral("Edit reports this instead of opening another application.")), qPrintable(status->text()));
        const QString missing = QStringLiteral(
            "ONLYOFFICE, chosen under Preferences › Applications › Office, is not installed, or its onlyoffice-desktopeditors command is not in PATH.");
        view->selectUrls({});
        view->selectUrls({QUrl::fromLocalFile(report)});
        QTRY_COMPARE(edit->toolTip(), missing);
        edit->trigger();
        QCOMPARE(window.statusBar()->currentMessage(), missing);
        QVERIFY(!QFile::exists(output));

        // A partly installed application opens the kinds it has.
        choose(provider, 2);
        QVERIFY2(
            status->text().startsWith(QStringLiteral("Edit opens office documents in Calligra where it can. Calligra is not installed, or its calligrasheets")),
            qPrintable(status->text()));
        view->selectUrls({});
        view->selectUrls({QUrl::fromLocalFile(report)});
        QTRY_COMPARE(edit->toolTip(), QStringLiteral("Edit the selected file with Calligra"));
        edit->trigger();
        // The script creates the file before writing to it.
        const auto recorded = [&output] {
            QFile file(output);
            return file.open(QIODevice::ReadOnly) ? QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts) : QStringList();
        };
        QTRY_COMPARE(recorded(), (QStringList{bin.filePath(QStringLiteral("calligrawords")), report}));

        // Turning previews off reloads the preview with a message.
        auto *previews = dialog.findChild<QGroupBox *>(QStringLiteral("officePreviewGroup"));
        QVERIFY(previews);
        QCOMPARE(tabTitleOf(previews), QStringLiteral("Office"));
        auto *enabled = previews->findChild<QCheckBox *>(QStringLiteral("kcfg_OfficePreviews"));
        auto *previewStatus = previews->findChild<QLabel *>(QStringLiteral("officePreviewStatus"));
        QVERIFY(enabled && enabled->isChecked() && previewStatus);
        QVERIFY2(previewStatus->text().contains(QStringLiteral("Spreadsheets: ")), qPrintable(previewStatus->text()));
        auto *panel = window.findChild<QWidget *>(QStringLiteral("previewPanel"));
        auto *message = panel->findChild<QLabel *>(QStringLiteral("previewMessage"));
        QVERIFY(message);
        enabled->setChecked(false);
        QVERIFY(previewStatus->text().startsWith(QStringLiteral("Office document previews are off")));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::officePreviews());
        QTRY_VERIFY2(message->text().startsWith(QStringLiteral("Office document previews are off.")), qPrintable(message->text()));
    }

    void reportsMissingFileAssociationsSettings()
    {
        for (const QString &kind : {QStringLiteral("systemsettings"), QStringLiteral("systemsettings_qwidgets")}) {
            if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/") + kind, QStringLiteral("kcm_filetypes")).isValid()) {
                QSKIP("KDE's File Associations settings are installed on this system.");
            }
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openFileAssociationsSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("officePreviewMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("File Associations settings are not installed")));
    }

    void mediaTabChoosesThePlaybackBackend()
    {
        QTemporaryDir bin;
        QTemporaryDir folder;
        QVERIFY(bin.isValid() && folder.isValid());
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path] {
            qputenv("PATH", path);
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        // Without the mpv application in PATH.
        qputenv("PATH", bin.path().toLocal8Bit());
        const QString audio = folder.filePath(QStringLiteral("empty.wav"));
        QFile file(audio);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("mediaBackendGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Media"));
        QVERIFY(dialog.findChild<QWidget *>(QStringLiteral("mediaIntegrationPage"))->isAncestorOf(group));
        // The combo moved here from Interface › Previews.
        QCOMPARE(dialog.findChildren<QComboBox *>(QStringLiteral("kcfg_MediaBackend")).size(), 1);
        QVERIFY(!dialog.findChild<QWidget *>(QStringLiteral("previewsPage"))->findChild<QComboBox *>(QStringLiteral("kcfg_MediaBackend")));
        auto *backend = group->findChild<QComboBox *>(QStringLiteral("kcfg_MediaBackend"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("mediaBackendStatus"));
        auto *application = group->findChild<QLabel *>(QStringLiteral("mpvApplicationStatus"));
        QVERIFY(backend && status && application);
        QCOMPARE(backend->currentIndex(), int(Settings::EnumMediaBackend::QtMultimedia));
        QVERIFY2(application->text().startsWith(QStringLiteral("The mpv application is not installed")), qPrintable(application->text()));
        if (!MediaBackend::mpvBuilt()) {
            QCOMPARE(backend->itemText(1), QStringLiteral("mpv (not built in)"));
            QVERIFY(status->text().endsWith(QStringLiteral("so mpv can't be chosen.")));
            return;
        }
        QCOMPARE(backend->itemText(1), QStringLiteral("mpv"));
        QCOMPARE(status->text(), QStringLiteral("Qt Multimedia plays audio and video."));

        MediaView view;
        view.open(QUrl::fromLocalFile(audio), false, false);
        QVERIFY(view.player());
        backend->setCurrentIndex(Settings::EnumMediaBackend::Mpv);
        QVERIFY2(status->text().startsWith(QStringLiteral("mpv plays audio")), qPrintable(status->text()));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::mediaBackend(), Settings::EnumMediaBackend::Mpv);
        // An open preview reopens its file with the chosen backend.
        QVERIFY(view.mpvPlayer());
        QCOMPARE(view.findChild<QToolButton *>(QStringLiteral("mediaBackend"))->text(), QStringLiteral("mpv"));

        // The backend button in a preview changes the setting shown here.
        view.findChild<QAction *>(QStringLiteral("mediaBackendQtMultimedia"))->trigger();
        QCOMPARE(backend->currentIndex(), int(Settings::EnumMediaBackend::QtMultimedia));
        QTRY_VERIFY(!dialog.button(QDialogButtonBox::Apply)->isEnabled());
    }

    void ebookFontTabSetsPreviewsAndSampleText()
    {
        QTemporaryDir bin;
        QVERIFY(bin.isValid());
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path] {
            qputenv("PATH", path);
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        // Without kfontinst in PATH.
        qputenv("PATH", bin.path().toLocal8Bit());

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *documents = dialog.findChild<QGroupBox *>(QStringLiteral("ebookPreviewGroup"));
        auto *fonts = dialog.findChild<QGroupBox *>(QStringLiteral("fontPreviewGroup"));
        QVERIFY(documents && fonts);
        QCOMPARE(tabTitleOf(documents), QStringLiteral("Ebooks and Fonts"));
        QVERIFY(dialog.findChild<QWidget *>(QStringLiteral("ebookFontIntegrationPage"))->isAncestorOf(fonts));
        const auto apply = [&dialog] {
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };

        auto *status = documents->findChild<QLabel *>(QStringLiteral("ebookPreviewStatus"));
        auto *comics = documents->findChild<QCheckBox *>(QStringLiteral("kcfg_ComicPreviews"));
        QVERIFY(status && comics && comics->isChecked());
        QVERIFY(documents->findChild<QCheckBox *>(QStringLiteral("kcfg_EbookPreviews")));
        QVERIFY(documents->findChild<QCheckBox *>(QStringLiteral("kcfg_HelpFilePreviews")));
        for (const QString &kind :
             {QStringLiteral("Ebooks: "), QStringLiteral("DjVu documents: "), QStringLiteral("Comic books: "), QStringLiteral("Help files: ")}) {
            QVERIFY2(status->text().contains(kind), qPrintable(status->text()));
        }
        comics->setChecked(false);
        QVERIFY2(status->text().contains(QStringLiteral("Comic books: previews are off.")), qPrintable(status->text()));
        apply();
        QVERIFY(!Settings::comicPreviews());
        QVERIFY(Settings::ebookPreviews());
        QVERIFY(!DocumentFormats::previewEnabled(DocumentFormats::Kind::Comic));

        auto *enabled = fonts->findChild<QCheckBox *>(QStringLiteral("kcfg_FontPreviews"));
        auto *sample = fonts->findChild<QLineEdit *>(QStringLiteral("kcfg_FontSampleText"));
        auto *fontStatus = fonts->findChild<QLabel *>(QStringLiteral("fontPreviewStatus"));
        QVERIFY(enabled && sample && fontStatus);
        QVERIFY(sample->text().isEmpty());
        QCOMPARE(sample->placeholderText(), DocumentFormats::defaultSampleText());
        QVERIFY2(fontStatus->text().contains(QStringLiteral("kfontinst")), qPrintable(fontStatus->text()));
        sample->setText(QStringLiteral("Sphinx of black quartz"));
        apply();
        QCOMPARE(Settings::fontSampleText(), QStringLiteral("Sphinx of black quartz"));
        FontPreview preview;
        QCOMPARE(preview.sampleText(), QStringLiteral("Sphinx of black quartz"));
        auto *previewSample = preview.findChild<QLineEdit *>(QStringLiteral("fontPreviewSample"));
        QCOMPARE(previewSample->text(), QStringLiteral("Sphinx of black quartz"));

        // Text edited in a preview while the dialog is open is shown there.
        previewSample->setText(QStringLiteral("Pack my box"));
        QCOMPARE(sample->text(), QStringLiteral("Pack my box"));
        QVERIFY(!dialog.button(QDialogButtonBox::Apply)->isEnabled());

        enabled->setChecked(false);
        QVERIFY(!sample->isEnabled());
        QVERIFY(fontStatus->text().startsWith(QStringLiteral("Font previews are off")));
        apply();
        QVERIFY(!Settings::fontPreviews());
        QCOMPARE(Settings::fontSampleText(), QStringLiteral("Pack my box"));
    }

    void packagesTabTurnsOnPackageActions()
    {
        QTemporaryDir bin;
        QVERIFY(bin.isValid());
        const QString applications = AppPackages::Locations::system().applications;
        const QString entry = applications + QStringLiteral("/appimagekit_0123-myapp.desktop");
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path, &entry] {
            qputenv("PATH", path);
            QFile::remove(entry);
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        // Only flatpak is installed.
        QFile flatpak(bin.filePath(QStringLiteral("flatpak")));
        QVERIFY(flatpak.open(QIODevice::WriteOnly));
        flatpak.write("#!/bin/sh\n");
        flatpak.close();
        flatpak.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        qputenv("PATH", bin.path().toLocal8Bit());
        QFile::remove(entry);

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *actions = dialog.findChild<QGroupBox *>(QStringLiteral("packageActionsGroup"));
        auto *menu = dialog.findChild<QGroupBox *>(QStringLiteral("appImageIntegrationGroup"));
        QVERIFY(actions && menu);
        QCOMPARE(tabTitleOf(actions), QStringLiteral("Packages"));
        QVERIFY(dialog.findChild<QWidget *>(QStringLiteral("packagesIntegrationPage"))->isAncestorOf(menu));
        const auto apply = [&dialog] {
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };

        auto *appImages = actions->findChild<QCheckBox *>(QStringLiteral("kcfg_AppImageActions"));
        auto *flatpaks = actions->findChild<QCheckBox *>(QStringLiteral("kcfg_FlatpakActions"));
        auto *snaps = actions->findChild<QCheckBox *>(QStringLiteral("kcfg_SnapActions"));
        auto *tools = actions->findChild<QLabel *>(QStringLiteral("packageToolsStatus"));
        QVERIFY(appImages && flatpaks && snaps && tools);
        QVERIFY(appImages->isChecked() && flatpaks->isChecked() && snaps->isChecked());
        QVERIFY2(tools->text().contains(QStringLiteral("appimageupdatetool, which is not installed")), qPrintable(tools->text()));
        QVERIFY2(tools->text().contains(QStringLiteral("Flatpak: uses flatpak.")), qPrintable(tools->text()));
        QVERIFY2(tools->text().contains(QStringLiteral("Snap: snap is not installed")), qPrintable(tools->text()));
        QVERIFY2(tools->text().contains(QStringLiteral("Discover is not installed")), qPrintable(tools->text()));

        snaps->setChecked(false);
        QVERIFY2(tools->text().contains(QStringLiteral("Snap: actions are off.")), qPrintable(tools->text()));
        apply();
        QVERIFY(!Settings::snapActions());
        QVERIFY(!AppPackages::isPackage(QStringLiteral("/tmp/hello_42.snap"), QStringLiteral("application/vnd.snap")));
        QVERIFY(AppPackages::isPackage(QStringLiteral("/tmp/MyApp.AppImage"), QStringLiteral("application/vnd.appimage")));

        auto *integration = menu->findChild<QCheckBox *>(QStringLiteral("kcfg_AppImageMenuIntegration"));
        auto *details = menu->findChild<QCheckBox *>(QStringLiteral("kcfg_AppImageReadDetails"));
        auto *status = menu->findChild<QLabel *>(QStringLiteral("appImageIntegrationStatus"));
        QVERIFY(integration && details && status);
        QCOMPARE(status->text(), QStringLiteral("No AppImages are in the application menu."));
        QDir().mkpath(applications);
        QFile desktop(entry);
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.close();
        details->setChecked(false);
        integration->setChecked(false);
        QVERIFY(!details->isEnabled());
        QVERIFY2(status->text().startsWith(QStringLiteral("1 AppImage is in the application menu.")), qPrintable(status->text()));
        apply();
        QVERIFY(!Settings::appImageMenuIntegration());
        QVERIFY(!Settings::appImageReadDetails());
        AppPackages::Package package;
        package.kind = AppPackages::Kind::AppImage;
        QCOMPARE(AppPackages::actions(package), (QList<AppPackages::Action>{AppPackages::Action::Run, AppPackages::Action::Update}));

        appImages->setChecked(false);
        QVERIFY(!integration->isEnabled());
        QVERIFY(!details->isEnabled());
        QVERIFY(tools->text().contains(QStringLiteral("AppImages: actions are off.")));
        apply();
        QVERIFY(!AppPackages::isPackage(QStringLiteral("/tmp/MyApp.AppImage"), QStringLiteral("application/vnd.appimage")));
        QVERIFY(AppPackages::actions(package).isEmpty());
    }

    void mailTabChoosesTheClientAndSendByEmail()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        const QString file = folder.filePath(QStringLiteral("notes.txt"));
        write(file, QStringLiteral("notes"));
        const auto installClient = [this](const QString &desktopId, const QString &name) {
            write(m_applications + QLatin1Char('/') + desktopId,
                  QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=true %u\nMimeType=x-scheme-handler/mailto;\n").arg(name));
            write(m_configHome + QStringLiteral("/mimeapps.list"), QStringLiteral("[Default Applications]\nx-scheme-handler/mailto=%1;\n").arg(desktopId));
            KSycoca::self()->ensureCacheValid();
        };

        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(file)});
        QAction *send = window.actionCollection()->action(QStringLiteral("send_by_email"));
        QTRY_VERIFY(send->isEnabled());
        QVERIFY(send->isVisible());

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("mailIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        const int index = tabs->indexOf(page);
        QCOMPARE(tabs->tabText(index), QStringLiteral("Mail"));
        QCOMPARE(tabs->tabText(index - 1), QStringLiteral("Packages"));
        auto *group = page->findChild<QGroupBox *>(QStringLiteral("mailClientGroup"));
        QVERIFY(group);
        auto *enabled = group->findChild<QCheckBox *>(QStringLiteral("kcfg_SendByEmail"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("mailClientStatus"));
        QVERIFY(enabled && status && group->findChild<QPushButton *>(QStringLiteral("openDefaultApplicationsSettings")));
        QVERIFY(enabled->isChecked());
        if (!MailHandoff::preferredClient()) {
            QCOMPARE(status->text(), QStringLiteral("No default mail client is set, so Send by Email can only list the files for dragging into a message."));
        }

        // The status reads the client again when it changes.
        const auto reread = [enabled] {
            enabled->setChecked(false);
            enabled->setChecked(true);
        };
        installClient(QStringLiteral("fakemail.desktop"), QStringLiteral("Fake Mail"));
        reread();
        QCOMPARE(status->text(), QStringLiteral("Default mail client: Fake Mail."));
        installClient(QStringLiteral("io.github.mikepadiernos.plasma_impresario.desktop"), QStringLiteral("Plasma Impresario"));
        reread();
        QVERIFY2(status->text().startsWith(QStringLiteral("Default mail client: Plasma Impresario. It cannot receive attached files")),
                 qPrintable(status->text()));

        enabled->setChecked(false);
        QVERIFY2(status->text().startsWith(QStringLiteral("Default mail client: Plasma Impresario.")), qPrintable(status->text()));
        QVERIFY(status->text().endsWith(QStringLiteral("Send by Email is off, so the File menu and context menus don't offer it.")));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::sendByEmail());
        QVERIFY(!send->isVisible());
        QVERIFY(!send->isEnabled());
        // The selection changing doesn't turn it back on.
        view->selectUrls({});
        view->selectUrls({QUrl::fromLocalFile(file)});
        QTRY_VERIFY(window.actionCollection()->action(QStringLiteral("delete"))->isEnabled());
        QVERIFY(!send->isEnabled());
        window.sendByEmail();
        QVERIFY(!window.findChild<SendByEmailDialog *>());

        enabled->setChecked(true);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(send->isVisible());
        QVERIFY(send->isEnabled());
    }

    void mailTabSkipsOrAsksAboutRemoteFiles()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("mailIntegrationPage"));
        QVERIFY(page);
        auto *group = page->findChild<QGroupBox *>(QStringLiteral("mailRemoteFilesGroup"));
        QVERIFY(group);
        auto *choice = group->findChild<QComboBox *>(QStringLiteral("kcfg_MailRemoteFiles"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("mailRemoteFilesStatus"));
        QVERIFY(choice && status);
        QCOMPARE(choice->currentIndex(), int(Settings::EnumMailRemoteFiles::Ask));
        QVERIFY2(status->text().contains(QStringLiteral("only when you choose to")), qPrintable(status->text()));

        // Remote files only matter while Send by Email is offered.
        auto *enabled = page->findChild<QCheckBox *>(QStringLiteral("kcfg_SendByEmail"));
        QVERIFY(group->isEnabled());
        enabled->setChecked(false);
        QVERIFY(!group->isEnabled());
        enabled->setChecked(true);
        QVERIFY(group->isEnabled());

        choice->setCurrentIndex(Settings::EnumMailRemoteFiles::Skip);
        QVERIFY2(status->text().contains(QStringLiteral("without downloading them")), qPrintable(status->text()));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::mailRemoteFiles(), int(Settings::EnumMailRemoteFiles::Skip));
        QCOMPARE(MailHandoff::remoteFiles(), MailHandoff::RemoteFiles::Skip);

        // Send by Email then skips remote files without offering to download them.
        const QUrl remote(QStringLiteral("sftp://host/notes.txt"));
        SendByEmailDialog send({KFileItem(remote, QStringLiteral("text/plain"), S_IFREG)});
        QVERIFY(send.findChild<QCheckBox *>(QStringLiteral("mailPrepareCopies"))->isHidden());
        auto *items = send.findChild<QListWidget *>(QStringLiteral("mailItems"));
        QCOMPARE(items->count(), 1);
        QVERIFY(!(items->item(0)->flags() & Qt::ItemIsEnabled));
        QVERIFY(items->item(0)->text().contains(QStringLiteral("Preferences › Applications › Mail")));
    }

    void mailTabTurnsMailPreviewsOff()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        const QString message = folder.filePath(QStringLiteral("notes.eml"));
        write(message, QStringLiteral("From: Ada <ada@example.org>\r\nSubject: Notes\r\n\r\nHello Grace\r\n"));

        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->trigger();
        view->selectUrls({QUrl::fromLocalFile(message)});
        auto *panel = window.findChild<QWidget *>(QStringLiteral("previewPanel"));
        auto *content = panel->findChild<QStackedWidget *>();
        QVERIFY(content);
        QTRY_VERIFY(content->currentWidget() && content->currentWidget()->objectName() == QLatin1String("previewMail"));

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("mailPreviewGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Mail"));
        auto *enabled = group->findChild<QCheckBox *>(QStringLiteral("kcfg_MailPreviews"));
        auto *status = group->findChild<QLabel *>(QStringLiteral("mailPreviewStatus"));
        QVERIFY(enabled && enabled->isChecked() && status);
        QVERIFY2(status->text().startsWith(QStringLiteral("Local mail files up to 64 MiB")), qPrintable(status->text()));
        QVERIFY(status->text().contains(QStringLiteral("Preferences › Interface › Previews")));

        // Turning previews off shows the message as plain text.
        enabled->setChecked(false);
        QVERIFY2(status->text().startsWith(QStringLiteral("Mail files show as plain text")), qPrintable(status->text()));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::mailPreviews());
        QTRY_VERIFY(content->currentWidget() && content->currentWidget()->objectName() == QLatin1String("previewText"));
        auto *text = qobject_cast<QPlainTextEdit *>(content->currentWidget());
        QVERIFY(text);
        QVERIFY2(text->toPlainText().startsWith(QStringLiteral("From: Ada")), qPrintable(text->toPlainText()));
    }

    void reportsMissingDefaultApplicationsSettings()
    {
        for (const QString &kind : {QStringLiteral("systemsettings"), QStringLiteral("systemsettings_qwidgets")}) {
            if (KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/") + kind, QStringLiteral("kcm_componentchooser")).isValid()) {
                QSKIP("Plasma's Default Applications settings are installed on this system.");
            }
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.findChild<QPushButton *>(QStringLiteral("openDefaultApplicationsSettings"))->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("mailClientMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("Default Applications settings are not installed")));
    }

    void connectionsTabSavesRemoteConnectionsImmediately()
    {
        KConfigGroup profilesGroup = KSharedConfig::openConfig()->group(QStringLiteral("RemoteProfiles"));
        KConfigGroup mountsGroup = KSharedConfig::openConfig()->group(QStringLiteral("RemoteMounts"));
        RemoteMounts::saveOptions(mountsGroup, QStringLiteral("stale"), RemoteMounts::Options());
        QVERIFY(mountsGroup.hasGroup(QStringLiteral("stale")));

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("remoteConnectionsIntegrationPage"));
        QVERIFY(page);
        auto *group = page->findChild<QGroupBox *>(QStringLiteral("remoteConnectionsProfilesGroup"));
        QVERIFY(group);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Connections"));
        auto *model = qobject_cast<KPageWidgetModel *>(dialog.findChild<KPageWidget *>()->model());
        KPageWidgetItem *item = nullptr;
        for (int row = 0; row < model->rowCount(); ++row) {
            KPageWidgetItem *candidate = model->item(model->index(row, 0));
            if (candidate->widget()->isAncestorOf(page)) {
                item = candidate;
            }
        }
        QVERIFY(item);
        QCOMPARE(item->name(), QStringLiteral("Remote and Cloud"));
        QVERIFY2(item->icon().name().startsWith(QStringLiteral("folder-cloud")), qPrintable(item->icon().name()));

        // A connection saved elsewhere shows when the tab is shown again.
        RemoteProfiles::Profile build;
        build.name = QStringLiteral("Build");
        build.host = QStringLiteral("build.example.org");
        QList<RemoteProfiles::Profile> external{build};
        QVERIFY(RemoteProfiles::save(profilesGroup, external));
        auto *list = page->findChild<QListWidget *>(QStringLiteral("remoteProfileList"));
        QCOMPARE(list->count(), 0);
        dialog.setCurrentPage(item);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QTRY_COMPARE(list->count(), 1);
        QCOMPARE(list->item(0)->text(), QStringLiteral("Build"));
        // A connection saved in another window while the tab is open is kept.
        RemoteProfiles::Profile mirror = build;
        mirror.name = QStringLiteral("Mirror");
        external.append(mirror);
        QVERIFY(RemoteProfiles::save(profilesGroup, external));

        // Save takes effect without Apply, in the Remote Connections menu too.
        page->findChild<QPushButton *>(QStringLiteral("remoteProfileAdd"))->click();
        auto *protocol = page->findChild<QComboBox *>(QStringLiteral("remoteProfileProtocol"));
        protocol->setCurrentIndex(protocol->findData(static_cast<int>(RemoteProfiles::Protocol::Smb)));
        page->findChild<QLineEdit *>(QStringLiteral("remoteProfileName"))->setText(QStringLiteral("NAS"));
        page->findChild<QLineEdit *>(QStringLiteral("remoteProfileHost"))->setText(QStringLiteral("nas.local"));
        page->findChild<QPushButton *>(QStringLiteral("remoteProfileSave"))->click();
        QList<RemoteProfiles::Profile> saved = RemoteProfiles::load(profilesGroup);
        QCOMPARE(saved.size(), 3);
        QCOMPARE(saved.at(1).name, QStringLiteral("Mirror"));
        QCOMPARE(saved.at(2).name, QStringLiteral("NAS"));
        QCOMPARE(saved.at(2).protocol, RemoteProfiles::Protocol::Smb);
        QCOMPARE(list->count(), 3);
        // Saving forgets the mount options of connections that no longer exist.
        QVERIFY(!mountsGroup.hasGroup(QStringLiteral("stale")));

        auto *remote = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("remote_connections")));
        Q_EMIT remote->menu()->aboutToShow();
        QStringList names;
        for (QAction *entry : remote->menu()->actions()) {
            names.append(entry->text());
        }
        QVERIFY2(names.contains(QStringLiteral("NAS")), qPrintable(names.join(QLatin1Char(','))));

        // Remove takes effect immediately too.
        QCOMPARE(list->currentItem()->text(), QStringLiteral("NAS"));
        page->findChild<QPushButton *>(QStringLiteral("remoteProfileRemove"))->click();
        saved = RemoteProfiles::load(profilesGroup);
        QCOMPARE(saved.size(), 2);
        QCOMPARE(saved.at(0).name, QStringLiteral("Build"));
        QCOMPARE(saved.at(1).name, QStringLiteral("Mirror"));
        QCOMPARE(list->currentItem()->text(), QStringLiteral("Mirror"));
    }

    void reportsMissingWalletManager()
    {
        for (const QString &name : {QStringLiteral("org.kde.kwalletmanager"), QStringLiteral("org.kde.kwalletmanager5")}) {
            if (KService::serviceByDesktopName(name)) {
                QSKIP("KDE Wallet Manager is installed on this system.");
            }
        }
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *wallet = dialog.findChild<QPushButton *>(QStringLiteral("openWalletManager"));
        QVERIFY(wallet);
        QCOMPARE(tabTitleOf(wallet), QStringLiteral("Connections"));
        wallet->click();
        auto *message = dialog.findChild<KMessageWidget *>(QStringLiteral("remoteConnectionsMessage"));
        QCOMPARE(message->messageType(), KMessageWidget::Error);
        QVERIFY(message->text().contains(QStringLiteral("kwalletmanager")));
    }

    void connectionDefaultsStartNewRemoteConnections()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("remoteConnectionsIntegrationPage"));
        QVERIFY(page);
        auto *protocol = page->findChild<QComboBox *>(QStringLiteral("kcfg_RemoteDefaultProtocol"));
        auto *user = page->findChild<QLineEdit *>(QStringLiteral("kcfg_RemoteDefaultUser"));
        auto *authentication = page->findChild<QComboBox *>(QStringLiteral("kcfg_RemoteDefaultAuthentication"));
        auto *status = page->findChild<QLabel *>(QStringLiteral("remoteConnectionsDefaultsStatus"));
        QVERIFY(protocol && user && authentication && status);
        QCOMPARE(tabTitleOf(protocol), QStringLiteral("Connections"));
        QCOMPARE(protocol->count(), RemoteProfiles::protocols().size());
        QCOMPARE(protocol->currentIndex(), int(Settings::EnumRemoteDefaultProtocol::Sftp));
        auto *formProtocol = page->findChild<QComboBox *>(QStringLiteral("remoteProfileProtocol"));
        auto *formUser = page->findChild<QLineEdit *>(QStringLiteral("remoteProfileUser"));
        auto *formAuthentication = page->findChild<QComboBox *>(QStringLiteral("remoteProfileAuthentication"));

        // The new connection on the tab follows the defaults as they are edited.
        authentication->setCurrentIndex(Settings::EnumRemoteDefaultAuthentication::KeyOrAgent);
        user->setText(QStringLiteral("ada"));
        QCOMPARE(formUser->text(), QStringLiteral("ada"));
        QCOMPARE(formAuthentication->currentData().toInt(), static_cast<int>(RemoteProfiles::Authentication::KeyOrAgent));
        protocol->setCurrentIndex(Settings::EnumRemoteDefaultProtocol::Smb);
        QVERIFY(!authentication->isEnabled());
        QCOMPARE(formProtocol->currentData().toInt(), static_cast<int>(RemoteProfiles::Protocol::Smb));
        QCOMPARE(formAuthentication->currentData().toInt(), static_cast<int>(RemoteProfiles::Authentication::Password));
        user->setText(QStringLiteral("a b"));
        QVERIFY(!status->isHidden());
        QVERIFY2(status->text().contains(QStringLiteral("without a user name")), qPrintable(status->text()));
        QVERIFY(formUser->text().isEmpty());
        user->setText(QStringLiteral("ada"));
        QVERIFY(status->isHidden());
        QCOMPARE(formUser->text(), QStringLiteral("ada"));

        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::remoteDefaultProtocol(), int(Settings::EnumRemoteDefaultProtocol::Smb));
        QCOMPARE(Settings::remoteDefaultUser(), QStringLiteral("ada"));

        // Manage Remote Connections… starts new connections with the applied defaults.
        window.actionCollection()->action(QStringLiteral("manage_remote_connections"))->trigger();
        auto *manager = window.findChild<RemoteProfilesDialog *>();
        QVERIFY(manager);
        const RemoteProfiles::Profile profile = manager->formProfile();
        QCOMPARE(profile.protocol, RemoteProfiles::Protocol::Smb);
        QCOMPARE(profile.user, QStringLiteral("ada"));
        QCOMPARE(profile.authentication, RemoteProfiles::Authentication::Password);
        manager->close();
    }

    void discoverySettingShowsOrHidesTheNetwork()
    {
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *shown = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RemoteShowNetwork"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("remoteConnectionsDiscoveryStatus"));
        QVERIFY(shown && status);
        QCOMPARE(tabTitleOf(shown), QStringLiteral("Connections"));
        QVERIFY(shown->isChecked());
        const QUrl url = RemoteProfiles::networkUrl();
        if (url.isEmpty()) {
            QVERIFY2(status->text().contains(QStringLiteral("kio-extras")), qPrintable(status->text()));
        } else {
            QVERIFY2(status->text().contains(url.toDisplayString()), qPrintable(status->text()));
        }

        QAction *network = window.actionCollection()->action(QStringLiteral("network_locations"));
        auto *remote = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("remote_connections")));
        QVERIFY(network->isVisible());
        Q_EMIT remote->menu()->aboutToShow();
        QVERIFY(remote->menu()->actions().contains(network));

        shown->setChecked(false);
        QVERIFY2(status->text().contains(QStringLiteral("hidden")), qPrintable(status->text()));
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::remoteShowNetwork());
        QVERIFY(!network->isVisible());
        Q_EMIT remote->menu()->aboutToShow();
        QVERIFY(!remote->menu()->actions().contains(network));
        // Manage Remote Connections… stays.
        QVERIFY(remote->menu()->actions().contains(window.actionCollection()->action(QStringLiteral("manage_remote_connections"))));

        shown->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(network->isVisible());
        Q_EMIT remote->menu()->aboutToShow();
        QVERIFY(remote->menu()->actions().contains(network));
    }

    void rcloneTabChoosesRemotesAndHowToOpenThem()
    {
        QTemporaryDir bin;
        QVERIFY(bin.isValid());
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path] {
            qputenv("PATH", path);
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        // A fake rclone lists two remotes and records how it was started.
        const QString output = bin.filePath(QStringLiteral("arguments"));
        QFile rclone(bin.filePath(QStringLiteral("rclone")));
        QVERIFY(rclone.open(QIODevice::WriteOnly));
        rclone.write(
            "#!/bin/sh\n"
            "if [ \"$1\" = listremotes ]; then printf 'gdrive:   drive\\nwork docs:   box\\n'; exit 0; fi\n"
            "printf '%s' \"$RCLONE_PASS\" > '"
            + output.toLocal8Bit() + ".pass'\nprintf '%s\\n' \"$@\" > '" + output.toLocal8Bit() + "'\nexec /bin/sleep 30\n");
        rclone.close();
        rclone.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        qputenv("PATH", bin.path().toLocal8Bit());

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RcloneEnabled"));
        auto *remotes = dialog.findChild<HiddenItemsList *>(QStringLiteral("kcfg_RcloneHiddenRemotes"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("rcloneStatus"));
        auto *refresh = dialog.findChild<QPushButton *>(QStringLiteral("refreshRcloneRemotes"));
        auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_RcloneOpenMode"));
        auto *modeStatus = dialog.findChild<QLabel *>(QStringLiteral("rcloneOpenModeStatus"));
        QVERIFY(enabled && remotes && status && refresh && mode && modeStatus);
        QCOMPARE(tabTitleOf(enabled), QStringLiteral("rclone"));
        QVERIFY(enabled->isChecked());
        QCOMPARE(mode->currentIndex(), int(Settings::EnumRcloneOpenMode::Mount));
        QVERIFY2(modeStatus->text().contains(QStringLiteral("mount")) || modeStatus->text().contains(QStringLiteral("FUSE")), qPrintable(modeStatus->text()));

        refresh->click();
        QTRY_COMPARE(remotes->count(), 2);
        QCOMPARE(remotes->item(0)->text(), QStringLiteral("gdrive (drive)"));
        QCOMPARE(remotes->item(1)->text(), QStringLiteral("work docs (box)"));
        QVERIFY2(status->text().contains(QStringLiteral("2 remotes")), qPrintable(status->text()));

        // A cleared remote is left out of Go › Cloud Storage.
        auto *cloud = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("cloud_storage")));
        QVERIFY(cloud && cloud->isVisible());
        QSignalSpy listed(window.rcloneMounts(), &RcloneMounts::remotesListed);
        const auto remoteEntries = [&] {
            listed.clear();
            Q_EMIT cloud->menu()->aboutToShow();
            if (!listed.wait(5000)) {
                return QList<QAction *>();
            }
            window.populateCloudStorage();
            return cloud->menu()->actions();
        };
        QCOMPARE(remoteEntries().size(), 2);
        remotes->item(0)->setCheckState(Qt::Unchecked);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::rcloneHiddenRemotes(), QStringList{QStringLiteral("gdrive")});
        QList<QAction *> entries = remoteEntries();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first()->text(), QStringLiteral("work docs (box)"));
        remotes->item(1)->setCheckState(Qt::Unchecked);
        dialog.button(QDialogButtonBox::Apply)->click();
        entries = remoteEntries();
        QCOMPARE(entries.size(), 1);
        QVERIFY(!entries.first()->isEnabled());
        QVERIFY(entries.first()->text().contains(QStringLiteral("hidden")));
        remotes->item(1)->setCheckState(Qt::Checked);
        dialog.button(QDialogButtonBox::Apply)->click();

        // Serving over WebDAV opens the remote through KIO, with the password kept out of the location.
        mode->setCurrentIndex(int(Settings::EnumRcloneOpenMode::WebDav));
        // The dialog tracks user selection in plain combo boxes.
        Q_EMIT mode->activated(mode->currentIndex());
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        QVERIFY2(modeStatus->text().contains(QStringLiteral("127.0.0.1")) || modeStatus->text().contains(QStringLiteral("HTTP worker")),
                 qPrintable(modeStatus->text()));
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::rcloneOpenMode(), Settings::EnumRcloneOpenMode::WebDav);
        window.rcloneMounts()->setServeCheck([output](quint16) {
            return QFile::exists(output);
        });
        window.rcloneMounts()->setPasswordProvider([](const QString &, const QUrl &, const std::function<void(const QString &)> &done) {
            done(QStringLiteral("secret"));
        });
        entries = remoteEntries();
        QCOMPARE(entries.size(), 1);
        QVERIFY(entries.first()->toolTip().contains(QStringLiteral("WebDAV")));
        QSignalSpy served(window.rcloneMounts(), &RcloneMounts::served);
        entries.first()->trigger();
        QVERIFY(served.wait(5000));
        const QUrl url = served.first().at(1).toUrl();
        QCOMPARE(window.activePane()->url(), url);
        QCOMPARE(url.scheme(), QStringLiteral("webdav"));
        QCOMPARE(url.host(), QStringLiteral("127.0.0.1"));
        QCOMPARE(url.userName(), QStringLiteral("rclone"));
        QVERIFY(url.password().isEmpty());
        QFile arguments(output);
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        const QStringList started = QString::fromLocal8Bit(arguments.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(started.mid(0, 2), (QStringList{QStringLiteral("serve"), QStringLiteral("webdav")}));
        QVERIFY(started.contains(QStringLiteral("127.0.0.1:%1").arg(url.port())));
        QVERIFY(started.contains(QStringLiteral("work docs:")));
        QVERIFY(!started.contains(QStringLiteral("secret")));
        QFile pass(output + QStringLiteral(".pass"));
        QVERIFY(pass.open(QIODevice::ReadOnly));
        QCOMPARE(pass.readAll(), QByteArray("secret"));
        QVERIFY(window.rcloneMounts()->mountedRemotes().isEmpty());

        // Stop Serving leaves the location.
        entries = remoteEntries();
        QVERIFY(entries.first()->isChecked());
        QCOMPARE(entries.size(), 3);
        QVERIFY(entries.at(1)->isSeparator());
        QCOMPARE(entries.at(1)->text(), QStringLiteral("Stop Serving"));
        QSignalSpy stopped(window.rcloneMounts(), &RcloneMounts::serveStopped);
        entries.at(2)->trigger();
        QCOMPARE(window.activePane()->url(), QUrl::fromLocalFile(QDir::homePath()));
        QVERIFY(stopped.wait(5000));
        QVERIFY(window.rcloneMounts()->servedRemotes().isEmpty());

        // Turning the integration off hides Go › Cloud Storage without running rclone.
        enabled->setChecked(false);
        QVERIFY(!remotes->isEnabled() && !mode->isEnabled());
        QVERIFY2(status->text().contains(QStringLiteral("hidden")), qPrintable(status->text()));
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::rcloneEnabled());
        QVERIFY(!cloud->isVisible());
        enabled->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(cloud->isVisible());
    }

    void remoteDrivesTabChoosesBackendsFolderAndAutomaticMounting()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString drives = folder.filePath(QStringLiteral("drives"));
        const auto config = KSharedConfig::openConfig();
        const QStringList groups{QStringLiteral("RemoteProfiles"), QStringLiteral("RemoteMounts"), QStringLiteral("Favorites"), QStringLiteral("Workspaces")};
        for (const QString &name : groups) {
            config->group(name).deleteGroup();
        }
        QVERIFY(RemoteMounts::setMountAtLogin(false, {}, {}));
        RemoteProfiles::Profile server;
        server.id = QStringLiteral("9a4e7c1b-3f2d-4e8a-b6c5-1d0f2e3a4b5c");
        server.name = QStringLiteral("Server");
        server.protocol = RemoteProfiles::Protocol::Sftp;
        server.host = QStringLiteral("example.org");
        server.path = QStringLiteral("/srv/data");
        QList<RemoteProfiles::Profile> profiles{server};
        QVERIFY(RemoteProfiles::save(config->group(QStringLiteral("RemoteProfiles")), profiles));
        // A Favorite on the connection is assigned to a workspace.
        const QUrl location(QStringLiteral("sftp://example.org/srv/data/docs"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString workspace = catalog.workspaces.first().id;
        QVERIFY(WorkspaceCatalog::assignLocation(catalog, WorkspaceCatalog::locationId(location), workspace, error));
        QVERIFY2(WorkspaceCatalog::save(config->group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        QVERIFY(config->sync());
        const auto restore = qScopeGuard([config, groups] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            RemoteMounts::setMountAtLogin(false, {}, {});
            config->reparseConfiguration();
            for (const QString &name : groups) {
                config->group(name).deleteGroup();
            }
            config->sync();
        });

        MainWindow window;
        QVERIFY(window.favorites()->add(location));
        QUrl requested;
        window.remoteMounts()->setKioFuseMount([&requested](const QUrl &url, QObject *, const RemoteMountManager::KioFuseReply &reply) {
            requested = url;
            reply(QStringLiteral("/run/user/1000/kio-fuse-x/sftp/example.org/srv/data"), {});
        });
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *sftp = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_RemoteDriveSftpBackend"));
        auto *webDav = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_RemoteDriveWebDavBackend"));
        auto *smb = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_RemoteDriveSmbBackend"));
        auto *mountFolder = dialog.findChild<KUrlRequester *>(QStringLiteral("kcfg_RemoteDriveMountFolder"));
        auto *readOnly = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RemoteDriveReadOnly"));
        auto *atLogin = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RemoteDriveMountAtLogin"));
        auto *onWorkspace = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RemoteDriveMountOnWorkspace"));
        auto *unmountOnExit = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RemoteDriveUnmountOnExit"));
        auto *backendsStatus = dialog.findChild<QLabel *>(QStringLiteral("remoteDrivesBackendsStatus"));
        auto *folderStatus = dialog.findChild<QLabel *>(QStringLiteral("remoteDrivesFolderStatus"));
        auto *automaticStatus = dialog.findChild<QLabel *>(QStringLiteral("remoteDrivesAutomaticStatus"));
        QVERIFY(sftp && webDav && smb && mountFolder && readOnly && atLogin && onWorkspace && unmountOnExit);
        QVERIFY(backendsStatus && folderStatus && automaticStatus);
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("openAutostartSettings")));
        QCOMPARE(tabTitleOf(sftp), QStringLiteral("Remote Drives"));
        const auto apply = [&dialog] {
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };
        const auto choose = [](QComboBox *combo, int index) {
            combo->setCurrentIndex(index);
            // The dialog tracks user selection in plain combo boxes.
            Q_EMIT combo->activated(index);
        };

        // KIO FUSE mounts every protocol until another backend is chosen.
        for (QComboBox *combo : {sftp, webDav, smb}) {
            QCOMPARE(combo->count(), 2);
            QCOMPARE(combo->currentIndex(), 0);
            QCOMPARE(combo->itemText(0), QStringLiteral("KIO FUSE"));
        }
        QCOMPARE(sftp->itemText(1).contains(QStringLiteral("not installed")), !RemoteMounts::unavailableReason(RemoteMounts::Backend::Sshfs).isEmpty());
        QVERIFY2(backendsStatus->text().contains(QStringLiteral("installed")), qPrintable(backendsStatus->text()));
        QVERIFY2(folderStatus->text().contains(RemoteMounts::mountRoot()), qPrintable(folderStatus->text()));
        QVERIFY2(automaticStatus->text().contains(QStringLiteral("No saved connection mounts at startup")), qPrintable(automaticStatus->text()));
        QVERIFY(unmountOnExit->isChecked());
        QVERIFY(window.remoteMounts()->unmountsOnExit());
        QVERIFY(window.remoteMounts()->mountFolder().isEmpty());

        // The chosen backends, folder and read-only access start connections without mount options of their own.
        choose(sftp, 1);
        choose(smb, 1);
        QVERIFY2(backendsStatus->text().contains(QStringLiteral("/etc/fstab")), qPrintable(backendsStatus->text()));
        mountFolder->setUrl(QUrl::fromLocalFile(drives));
        QVERIFY2(folderStatus->text().contains(drives), qPrintable(folderStatus->text()));
        readOnly->setChecked(true);
        apply();
        QCOMPARE(Settings::remoteDriveSftpBackend(), Settings::EnumRemoteDriveSftpBackend::Sshfs);
        QCOMPARE(Settings::remoteDriveSmbBackend(), Settings::EnumRemoteDriveSmbBackend::Cifs);
        const RemoteMounts::Defaults defaults = RemoteMounts::defaultsFromSettings();
        QCOMPARE(defaults.backends.value(RemoteProfiles::Protocol::Sftp), RemoteMounts::Backend::Sshfs);
        QCOMPARE(defaults.backends.value(RemoteProfiles::Protocol::Smb), RemoteMounts::Backend::Cifs);
        QVERIFY(!defaults.backends.contains(RemoteProfiles::Protocol::WebDav));
        QVERIFY(defaults.readOnly);
        QCOMPARE(QDir::cleanPath(defaults.folder), QDir::cleanPath(drives));
        QCOMPARE(QDir::cleanPath(window.remoteMounts()->mountFolder()), QDir::cleanPath(drives));
        const KConfigGroup mountOptions = config->group(QStringLiteral("RemoteMounts"));
        {
            RemoteMountDialog options(server, mountOptions, &window);
            QCOMPARE(options.options().backend, RemoteMounts::Backend::Sshfs);
            QVERIFY(options.options().readOnly);
            QVERIFY(options.options().location.isEmpty());
        }
        // Options saved for a connection win.
        RemoteMounts::saveOptions(config->group(QStringLiteral("RemoteMounts")), server.id, {});
        QCOMPARE(RemoteMounts::loadOptions(mountOptions, server, defaults).backend, RemoteMounts::Backend::KioFuse);
        config->group(QStringLiteral("RemoteMounts")).deleteGroup();

        // Mounts can stay after quitting.
        unmountOnExit->setChecked(false);
        QVERIFY2(automaticStatus->text().contains(QStringLiteral("stay after Plasma Opus quits")), qPrintable(automaticStatus->text()));
        apply();
        QVERIFY(!Settings::remoteDriveUnmountOnExit());
        QVERIFY(!window.remoteMounts()->unmountsOnExit());

        // Starting at login adds an autostart entry, and clearing it removes the entry.
        atLogin->setChecked(true);
        QVERIFY2(automaticStatus->text().contains(QStringLiteral("once you apply")), qPrintable(automaticStatus->text()));
        apply();
        QVERIFY(RemoteMounts::mountsAtLogin());
        QFile entry(RemoteMounts::autostartPath());
        QVERIFY(entry.open(QIODevice::ReadOnly));
        QVERIFY(entry.readAll().contains(" --background-mount\n"));
        entry.close();
        QTRY_VERIFY2(automaticStatus->text().contains(QStringLiteral("starts at login")), qPrintable(automaticStatus->text()));
        atLogin->setChecked(false);
        apply();
        QVERIFY(!RemoteMounts::mountsAtLogin());

        // Activating the workspace mounts the connection holding its location.
        choose(sftp, 0);
        onWorkspace->setChecked(true);
        apply();
        QVERIFY(Settings::remoteDriveMountOnWorkspace());
        window.mountRemoteProfilesForWorkspace(QStringLiteral("another"));
        QVERIFY(requested.isEmpty());
        window.mountRemoteProfilesForWorkspace(workspace);
        QCOMPARE(requested, RemoteProfiles::url(server));
        QVERIFY(window.remoteMounts()->isMounted(server.id));
        // A connection already mounted isn't mounted again.
        requested.clear();
        window.mountRemoteProfilesForWorkspace(workspace);
        QVERIFY(requested.isEmpty());
        window.remoteMounts()->unmount(server.id);
        QVERIFY(!window.remoteMounts()->isMounted(server.id));

        onWorkspace->setChecked(false);
        apply();
        window.mountRemoteProfilesForWorkspace(workspace);
        QVERIFY(requested.isEmpty());
    }

    void syncTabManagesPairsAndBackgroundSynchronization()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString local = folder.filePath(QStringLiteral("local"));
        const QString remote = folder.filePath(QStringLiteral("remote"));
        QVERIFY(QDir().mkpath(local));
        QVERIFY(QDir().mkpath(remote));
        const auto config = KSharedConfig::openConfig();
        const QStringList groups{QStringLiteral("CloudSync"), QStringLiteral("CloudSyncPairs")};
        for (const QString &name : groups) {
            config->group(name).deleteGroup();
        }
        QVERIFY(config->sync());
        QVERIFY(CloudSync::setStartAtLogin(false, {}, {}));
        Settings::self()->load();
        const auto restore = qScopeGuard([config, groups] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            CloudSync::setStartAtLogin(false, {}, {});
            config->reparseConfiguration();
            for (const QString &name : groups) {
                config->group(name).deleteGroup();
            }
            config->sync();
        });

        MainWindow window;
        CloudSync::Scheduler *scheduler = window.cloudSync();
        QVERIFY(scheduler);
        QVERIFY(scheduler->pairs().isEmpty());
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("syncIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(tabs->indexOf(page)), QStringLiteral("Sync"));
        QCOMPARE(tabs->tabText(0), QStringLiteral("Connections"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("rclone"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("Remote Drives"));
        QCOMPARE(tabs->indexOf(page), 3);

        auto *atLogin = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_CloudSyncAtLogin"));
        auto *metered = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_CloudSyncPauseOnMetered"));
        auto *interval = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CloudSyncCheckInterval"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("cloudSyncBackgroundStatus"));
        auto *pairs = page->findChild<QTreeWidget *>(QStringLiteral("cloudSyncPairs"));
        QVERIFY(atLogin && metered && interval && status && pairs);
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("openCloudSyncAutostartSettings")));
        QCOMPARE(tabTitleOf(interval), QStringLiteral("Sync"));
        QVERIFY(!atLogin->isChecked());
        QVERIFY(metered->isChecked());
        QCOMPARE(interval->value(), CloudSync::DefaultCheckIntervalMinutes);
        QVERIFY2(status->text().contains(QStringLiteral("only while Plasma Opus is running")), qPrintable(status->text()));
        QVERIFY2(status->text().contains(QStringLiteral("New pairs are checked every 15 minutes")), qPrintable(status->text()));
        const auto apply = [&dialog] {
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };

        // The pair editor of File › Cloud Sync… adds a pair at once, using the default interval.
        page->findChild<QPushButton *>(QStringLiteral("cloudSyncAdd"))->click();
        CloudSyncPairDialog *editor = nullptr;
        QTRY_VERIFY((editor = dialog.findChild<CloudSyncPairDialog *>()) && editor->isVisible());
        editor->setEngineLocations({folder.filePath(QStringLiteral("engine-config")), folder.filePath(QStringLiteral("engine-state"))});
        editor->findChild<KUrlRequester *>(QStringLiteral("cloudSyncLocal"))->setText(local);
        editor->findChild<QLineEdit *>(QStringLiteral("cloudSyncRemote"))->setText(QUrl::fromLocalFile(remote).toString());
        auto *conflictChoice = editor->findChild<QComboBox *>(QStringLiteral("cloudSyncConflictChoice"));
        conflictChoice->setCurrentIndex(conflictChoice->findData(int(CloudSync::Resolution::KeepBoth)));
        editor->accept();
        QCOMPARE(scheduler->pairs().size(), 1);
        QCOMPARE(pairs->topLevelItemCount(), 1);
        const CloudSync::Pair pair = scheduler->pairs().constFirst();
        QVERIFY(pair.useDefaultInterval);
        QVERIFY(pair.conflictChoice == CloudSync::Resolution::KeepBoth);
        QCOMPARE(CloudSync::load(config->group(QStringLiteral("CloudSyncPairs"))).size(), 1);
        QTRY_VERIFY2(status->text().contains(QStringLiteral("One pair is checked every 15 minutes")), qPrintable(status->text()));

        // The default interval, metered connections and login apply to the background runs.
        interval->setValue(5);
        QVERIFY2(status->text().contains(QStringLiteral("One pair is checked every 5 minutes")), qPrintable(status->text()));
        metered->setChecked(false);
        atLogin->setChecked(true);
        QVERIFY2(status->text().contains(QStringLiteral("once you apply")), qPrintable(status->text()));
        apply();
        QCOMPARE(Settings::cloudSyncCheckInterval(), 5);
        QCOMPARE(scheduler->defaultIntervalMinutes(), 5);
        QCOMPARE(scheduler->intervalMinutes(pair), 5);
        QVERIFY(!scheduler->pausesOnMeteredConnections());
        QVERIFY(CloudSync::startsAtLogin());
        QFile entry(CloudSync::autostartPath());
        QVERIFY(entry.open(QIODevice::ReadOnly));
        QVERIFY(entry.readAll().contains(" --background-sync\n"));
        entry.close();
        QTRY_VERIFY2(status->text().contains(QStringLiteral("starts at login")), qPrintable(status->text()));

        // Turning them back on and off reverses them.
        metered->setChecked(true);
        atLogin->setChecked(false);
        apply();
        QVERIFY(scheduler->pausesOnMeteredConnections());
        QVERIFY(!CloudSync::startsAtLogin());

        // Removing the pair takes effect at once, even if Preferences is cancelled.
        pairs->setCurrentItem(pairs->topLevelItem(0));
        page->findChild<QPushButton *>(QStringLiteral("cloudSyncRemove"))->click();
        QVERIFY(scheduler->pairs().isEmpty());
        dialog.reject();
        QVERIFY(CloudSync::load(config->group(QStringLiteral("CloudSyncPairs"))).isEmpty());
        QVERIFY(QDir(local).exists());
    }

    void syncTabConfiguresTheRsyncBackend()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString left = folder.filePath(QStringLiteral("left"));
        const QString right = folder.filePath(QStringLiteral("right"));
        QVERIFY(QDir().mkpath(left));
        QVERIFY(QDir().mkpath(right));
        const auto config = KSharedConfig::openConfig();
        const QString id = QStringLiteral("0f8fad5b-d9cb-469f-a165-70867728950e");
        FolderSync::Options options;
        options.excludePatterns = {QStringLiteral("*.tmp")};
        QString error;
        QVERIFY2(FolderSyncProfiles::save(config->group(QStringLiteral("FolderSyncProfiles")), {{QStringLiteral("Backups"), options, id}}, &error),
                 qPrintable(error));
        const auto restore = qScopeGuard([config] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            config->reparseConfiguration();
            config->group(QStringLiteral("FolderSyncProfiles")).deleteGroup();
            config->sync();
        });

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("rsyncBackendGroup"));
        auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RsyncEnabled"));
        auto *forLocal = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RsyncForLocalFolders"));
        auto *profile = dialog.findChild<FolderSyncProfileCombo *>(QStringLiteral("kcfg_RsyncProfile"));
        auto *policy = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_RsyncCopyPolicy"));
        auto *deletions = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_RsyncAllowDeletions"));
        auto *settings = dialog.findChild<QWidget *>(QStringLiteral("rsyncBackendSettings"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("rsyncBackendStatus"));
        QVERIFY(group && enabled && forLocal && profile && policy && deletions && settings && status);
        QCOMPARE(tabTitleOf(group), QStringLiteral("Sync"));
        QVERIFY(group->isAncestorOf(deletions));
        QVERIFY(!dialog.findChild<QGroupBox *>(QStringLiteral("cloudSyncBackgroundGroup"))->isAncestorOf(enabled));

        // Safe defaults: the built-in backend for local folders, no profile, only replacing older files and no deletions.
        QVERIFY(enabled->isChecked());
        QVERIFY(!forLocal->isChecked());
        QVERIFY(profile->profileId().isEmpty());
        QCOMPARE(profile->count(), 2);
        QCOMPARE(profile->itemText(1), QStringLiteral("Backups"));
        QCOMPARE(policy->currentIndex(), int(Settings::EnumRsyncCopyPolicy::NewerOnly));
        QVERIFY(!deletions->isChecked());
        QVERIFY(settings->isEnabled());
        const bool haveRsync = !RsyncSync::program().isEmpty();
        QVERIFY2(status->text().contains(haveRsync ? QStringLiteral("rsync is installed") : QStringLiteral("not installed")), qPrintable(status->text()));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("rsyncBackendNote"))->text().contains(QStringLiteral("dry run")));
        const auto apply = [&dialog] {
            QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
            dialog.button(QDialogButtonBox::Apply)->click();
        };

        // The chosen defaults start the folder synchronization dialog.
        forLocal->setChecked(true);
        profile->setCurrentIndex(profile->findData(id));
        policy->setCurrentIndex(int(Settings::EnumRsyncCopyPolicy::SkipExisting));
        deletions->setChecked(true);
        QVERIFY2(status->text().contains(QStringLiteral("permanent")), qPrintable(status->text()));
        apply();
        QVERIFY(Settings::rsyncForLocalFolders());
        QCOMPARE(Settings::rsyncProfile(), id);
        QCOMPARE(Settings::rsyncCopyPolicy(), int(Settings::EnumRsyncCopyPolicy::SkipExisting));
        QVERIFY(Settings::rsyncAllowDeletions());
        config->reparseConfiguration();
        QCOMPARE(config->group(QStringLiteral("Rsync")).readEntry("RsyncProfile"), id);
        if (haveRsync) {
            FolderSyncDialog sync(left, right, &window);
            QCOMPARE(sync.findChild<QComboBox *>(QStringLiteral("folderSyncBackend"))->currentData().toString(), QStringLiteral("rsync"));
            QCOMPARE(sync.findChild<QComboBox *>(QStringLiteral("folderSyncCopyPolicy"))->currentIndex(), int(FolderSync::CopyPolicy::SkipExisting));
            QCOMPARE(sync.findChild<QPlainTextEdit *>(QStringLiteral("folderSyncExclude"))->toPlainText(), QStringLiteral("*.tmp"));
        }

        // A deleted profile stays chosen as unavailable.
        config->group(QStringLiteral("FolderSyncProfiles")).deleteGroup();
        config->sync();
        profile->reload();
        QVERIFY(profile->isUnavailable());
        QCOMPARE(profile->profileId(), id);
        deletions->setChecked(false);
        QVERIFY2(status->text().contains(QStringLiteral("no longer exists")), qPrintable(status->text()));

        // Turning rsync off removes it from the dialog, which then can't compare SSH folders.
        enabled->setChecked(false);
        QVERIFY(!settings->isEnabled());
        QVERIFY2(status->text().contains(QStringLiteral("turned off")), qPrintable(status->text()));
        apply();
        QVERIFY(!Settings::rsyncEnabled());
        {
            FolderSyncDialog sync(left, right, &window);
            auto *backend = sync.findChild<QComboBox *>(QStringLiteral("folderSyncBackend"));
            QCOMPARE(backend->currentData().toString(), QStringLiteral("builtin"));
            QVERIFY(!qobject_cast<QStandardItemModel *>(backend->model())->item(1)->isEnabled());
        }
        FolderSyncDialog ssh(QUrl(QStringLiteral("sftp://me@host/srv")), QUrl::fromLocalFile(left), &window);
        QVERIFY(!ssh.findChild<QPushButton *>(QStringLiteral("folderSyncCompare"))->isEnabled());
    }

    void nextcloudTabSetsTheIntegrationChoices()
    {
        const auto config = KSharedConfig::openConfig();
        const auto restore = qScopeGuard([config] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            config->reparseConfiguration();
        });

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("nextcloudIntegrationPage"));
        auto *scroll = dialog.findChild<QWidget *>(QStringLiteral("nextcloudIntegrationScroll"));
        QVERIFY(page && scroll && scroll->isAncestorOf(page));
        auto *tabs = qobject_cast<QTabWidget *>(scroll->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->indexOf(scroll), 4);
        QCOMPARE(tabs->tabText(3), QStringLiteral("Sync"));
        QCOMPARE(tabs->tabText(4), QStringLiteral("Nextcloud"));
        QCOMPARE(tabs->tabText(5), QStringLiteral("Transfers and Cache"));
        for (const char *group :
             {"nextcloudAccountsGroup", "nextcloudBrowsingGroup", "nextcloudMetadataGroup", "nextcloudSharingGroup", "nextcloudDesktopGroup"}) {
            auto *box = dialog.findChild<QGroupBox *>(QLatin1String(group));
            QVERIFY2(box && page->isAncestorOf(box), group);
        }

        auto *serverPreviews = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NextcloudServerPreviews"));
        auto *permissions = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NextcloudCheckPermissions"));
        auto *serverMetadata = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NextcloudServerMetadata"));
        auto *favoritesAndTags = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NextcloudFavoritesAndTags"));
        auto *maxAge = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_NextcloudMetadataMaxAge"));
        auto *resultLimit = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_NextcloudResultLimit"));
        auto *shares = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NextcloudShares"));
        auto *linkExpiry = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_NextcloudLinkExpiryDays"));
        auto *desktopMenu = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NextcloudDesktopClientMenu"));
        auto *desktopStatus = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_NextcloudDesktopStatus"));
        auto *accountsStatus = dialog.findChild<QLabel *>(QStringLiteral("nextcloudAccountsStatus"));
        auto *thumbnailsStatus = dialog.findChild<QLabel *>(QStringLiteral("nextcloudThumbnailsStatus"));
        auto *sharingStatus = dialog.findChild<QLabel *>(QStringLiteral("nextcloudSharingStatus"));
        auto *clientStatus = dialog.findChild<QLabel *>(QStringLiteral("nextcloudDesktopStatus"));
        QVERIFY(serverPreviews && permissions && serverMetadata && favoritesAndTags && maxAge && resultLimit && shares && linkExpiry && desktopMenu
                && desktopStatus && accountsStatus && thumbnailsStatus && sharingStatus && clientStatus);
        for (QWidget *widget : std::initializer_list<QWidget *>{serverPreviews,
                                                                permissions,
                                                                serverMetadata,
                                                                favoritesAndTags,
                                                                maxAge,
                                                                resultLimit,
                                                                shares,
                                                                linkExpiry,
                                                                desktopMenu,
                                                                desktopStatus}) {
            QVERIFY(page->isAncestorOf(widget));
        }

        // The defaults keep what Go › Nextcloud did before these settings.
        for (QCheckBox *box : {serverPreviews, permissions, serverMetadata, favoritesAndTags, shares, desktopMenu, desktopStatus}) {
            QVERIFY2(box->isChecked(), qPrintable(box->objectName()));
        }
        QCOMPARE(maxAge->value(), 60);
        QCOMPARE(resultLimit->value(), 500);
        QCOMPARE(linkExpiry->value(), 0);
        QCOMPARE(linkExpiry->text(), QStringLiteral("Never"));
        QVERIFY2(accountsStatus->text().contains(QStringLiteral("No Nextcloud account is saved")), qPrintable(accountsStatus->text()));
        QVERIFY2(!clientStatus->text().isEmpty(), "the desktop client's state is shown");

        // The status follows the shared settings of other tabs.
        auto *online = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_OnlineAccountsEnabled"));
        auto *remotePreviews = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowRemotePreviews"));
        auto *sendByEmail = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_SendByEmail"));
        QVERIFY(online && remotePreviews && sendByEmail);
        QVERIFY(!page->isAncestorOf(online) && !page->isAncestorOf(remotePreviews) && !page->isAncestorOf(sendByEmail));
        online->setChecked(false);
        QVERIFY2(accountsStatus->text().contains(QStringLiteral("not read")), qPrintable(accountsStatus->text()));
        online->setChecked(true);
        QVERIFY2(accountsStatus->text().contains(QStringLiteral("listed too")), qPrintable(accountsStatus->text()));
        remotePreviews->setChecked(false);
        QVERIFY2(thumbnailsStatus->text().contains(QStringLiteral("no thumbnails")), qPrintable(thumbnailsStatus->text()));
        remotePreviews->setChecked(true);
        QVERIFY2(thumbnailsStatus->text().contains(QStringLiteral("show thumbnails")), qPrintable(thumbnailsStatus->text()));
        sendByEmail->setChecked(false);
        QVERIFY2(sharingStatus->text().contains(QStringLiteral("can't be sent")), qPrintable(sharingStatus->text()));
        sendByEmail->setChecked(true);
        QVERIFY2(sharingStatus->text().contains(QStringLiteral("sent by email")), qPrintable(sharingStatus->text()));

        // Dependent choices are disabled with what they depend on.
        serverMetadata->setChecked(false);
        QVERIFY(!maxAge->isEnabled());
        shares->setChecked(false);
        QVERIFY(!linkExpiry->isEnabled());
        QVERIFY2(sharingStatus->text().contains(QStringLiteral("stay as they are")), qPrintable(sharingStatus->text()));
        desktopMenu->setChecked(false);
        QVERIFY(!desktopStatus->isEnabled());

        serverPreviews->setChecked(false);
        permissions->setChecked(false);
        favoritesAndTags->setChecked(false);
        desktopStatus->setChecked(false);
        maxAge->setValue(300);
        resultLimit->setValue(1000);
        linkExpiry->setValue(7);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::nextcloudServerPreviews());
        QVERIFY(!Settings::nextcloudCheckPermissions());
        QVERIFY(!Settings::nextcloudServerMetadata());
        QVERIFY(!Settings::nextcloudFavoritesAndTags());
        QVERIFY(!Settings::nextcloudShares());
        QVERIFY(!Settings::nextcloudDesktopClientMenu());
        QVERIFY(!Settings::nextcloudDesktopStatus());
        QCOMPARE(Settings::nextcloudMetadataMaxAge(), 300);
        QCOMPARE(Settings::nextcloudResultLimit(), 1000);
        QCOMPARE(Settings::nextcloudLinkExpiryDays(), 7);
        config->reparseConfiguration();
        const KConfigGroup group = config->group(QStringLiteral("Nextcloud"));
        QCOMPARE(group.readEntry("LinkExpiryDays", 0), 7);
        QVERIFY(!group.readEntry("Shares", true));

        // Defaults restore the earlier behaviour.
        dialog.button(QDialogButtonBox::RestoreDefaults)->click();
        QVERIFY(serverMetadata->isChecked() && maxAge->isEnabled());
        QVERIFY(shares->isChecked() && linkExpiry->isEnabled());
        QVERIFY(desktopMenu->isChecked() && desktopStatus->isEnabled());

        // The buttons lead to the other tabs that Nextcloud depends on.
        dialog.findChild<QPushButton *>(QStringLiteral("nextcloudShowOnlineAccounts"))->click();
        auto *onlinePage = dialog.findChild<QWidget *>(QStringLiteral("onlineAccountsIntegrationPage"));
        QCOMPARE(qobject_cast<QTabWidget *>(onlinePage->parentWidget()->parentWidget())->currentWidget(), onlinePage);
        QVERIFY(dialog.currentPage()->widget()->isAncestorOf(onlinePage));
        dialog.findChild<QPushButton *>(QStringLiteral("nextcloudShowPreviewSettings"))->click();
        auto *previewsPage = dialog.findChild<QWidget *>(QStringLiteral("previewsPage"));
        QVERIFY(previewsPage);
        auto *previewTabs = qobject_cast<QTabWidget *>(previewsPage->parentWidget()->parentWidget());
        QVERIFY(previewTabs);
        QCOMPARE(previewTabs->currentWidget(), previewsPage);
        QVERIFY(dialog.currentPage()->widget()->isAncestorOf(previewsPage));

        // Accounts are managed in Go › Nextcloud's dialog, not a copy of it.
        auto *manage = dialog.findChild<QPushButton *>(QStringLiteral("manageNextcloudAccounts"));
        QVERIFY(manage && manage->isEnabled());
        QVERIFY(!window.findChild<NextcloudAccountsDialog *>());
        manage->click();
        auto *accounts = window.findChild<NextcloudAccountsDialog *>();
        QVERIFY(accounts);
        QVERIFY(!dialog.isAncestorOf(accounts));
        accounts->close();
    }

    void transfersTabSetsRcloneLimitsAndTheTrayPanel()
    {
        const auto config = KSharedConfig::openConfig();
        const auto restore = qScopeGuard([config] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            config->reparseConfiguration();
        });

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("transfersIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->indexOf(page), 5);
        QCOMPARE(tabs->tabText(4), QStringLiteral("Nextcloud"));
        QCOMPARE(tabs->tabText(5), QStringLiteral("Transfers and Cache"));

        auto *bandwidth = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CloudBandwidthLimitKiB"));
        auto *parallel = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CloudParallelTransfers"));
        auto *cacheSize = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CloudCacheSizeMiB"));
        auto *cacheAge = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_CloudCacheMaxAgeHours"));
        auto *inTray = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_CloudActivityInTray"));
        auto *transfersStatus = dialog.findChild<QLabel *>(QStringLiteral("cloudTransfersStatus"));
        auto *cacheStatus = dialog.findChild<QLabel *>(QStringLiteral("cloudCacheStatus"));
        auto *panelStatus = dialog.findChild<QLabel *>(QStringLiteral("cloudActivityPanelStatus"));
        auto *note = dialog.findChild<QLabel *>(QStringLiteral("cloudLimitsBackendsNote"));
        QVERIFY(bandwidth && parallel && cacheSize && cacheAge && inTray && transfersStatus && cacheStatus && panelStatus && note);
        for (QWidget *widget : std::initializer_list<QWidget *>{bandwidth, parallel, cacheSize, cacheAge, inTray, note}) {
            QVERIFY(page->isAncestorOf(widget));
        }

        // The defaults match the Cloud Activity window's, and the backends that ignore them are named.
        QCOMPARE(bandwidth->value(), 0);
        QCOMPARE(parallel->value(), 4);
        QCOMPARE(cacheSize->value(), 0);
        QCOMPARE(cacheAge->value(), 1);
        QVERIFY(!cacheAge->isEnabled());
        QVERIFY(inTray->isChecked());
        for (const QString &backend : {QStringLiteral("KIO FUSE"), QStringLiteral("sshfs"), QStringLiteral("davfs2"), QStringLiteral("CIFS")}) {
            QVERIFY2(note->text().contains(backend), qPrintable(backend));
        }
        QVERIFY(note->text().contains(QStringLiteral("no bandwidth, transfer or cache options")));
        QVERIFY2(cacheStatus->text().contains(QDir::toNativeSeparators(CloudActivity::rcloneCacheDir())), qPrintable(cacheStatus->text()));
        QVERIFY2(transfersStatus->text().contains(QStringLiteral("next mounted")), qPrintable(transfersStatus->text()));
        if (Rclone::findExecutable().isEmpty()) {
            QVERIFY2(transfersStatus->text().contains(QStringLiteral("not installed")), qPrintable(transfersStatus->text()));
        }

        // Applying sets the options of later rclone mounts and sync runs.
        bandwidth->setValue(512);
        parallel->setValue(8);
        cacheSize->setValue(2048);
        QVERIFY(cacheAge->isEnabled());
        cacheAge->setValue(72);
        QVERIFY2(cacheStatus->text().contains(QStringLiteral("available offline")), qPrintable(cacheStatus->text()));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::cloudBandwidthLimitKiB(), 512);
        config->reparseConfiguration();
        CloudActivity::Limits expected;
        expected.bandwidthKiB = 512;
        expected.parallelTransfers = 8;
        expected.cacheMiB = 2048;
        expected.cacheMaxAgeHours = 72;
        QCOMPARE(CloudActivity::loadLimits(config->group(CloudActivity::limitsGroupName())), expected);
        auto *scheduler = window.findChild<CloudSync::Scheduler *>();
        QVERIFY(scheduler);
        QCOMPARE(scheduler->rcloneOptions(), CloudActivity::rcloneTransferOptions(expected));
        QVERIFY(scheduler->rcloneOptions().contains(QStringLiteral("512K")));
        RcloneMounts *mounts = window.rcloneMounts();
        QCOMPARE(mounts->mountOptions(), CloudActivity::rcloneMountOptions(expected, CloudActivity::rcloneCacheDir()));
        QVERIFY(mounts->mountOptions().contains(QStringLiteral("72h")));

        // The Cloud Activity window shows the applied limits and its changes reach Preferences.
        window.showCloudActivity();
        auto *activity = window.findChild<CloudActivityDialog *>();
        QVERIFY(activity);
        QCOMPARE(activity->findChild<QSpinBox *>(QStringLiteral("cloudActivityBandwidth"))->value(), 512);
        QCOMPARE(activity->findChild<QSpinBox *>(QStringLiteral("cloudActivityCacheAge"))->value(), 72);
        bandwidth->setValue(1024);
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(activity->findChild<QSpinBox *>(QStringLiteral("cloudActivityBandwidth"))->value(), 1024);
        activity->findChild<QSpinBox *>(QStringLiteral("cloudActivityParallel"))->setValue(3);
        QCOMPARE(Settings::cloudParallelTransfers(), 3);
        QCOMPARE(Settings::cloudBandwidthLimitKiB(), 1024);
        QVERIFY(scheduler->rcloneOptions().contains(QStringLiteral("3")));
        activity->close();

        // The tray menu offers Cloud Activity… only while the setting is checked.
        DesktopTray *tray = window.desktopTray();
        inTray->setChecked(false);
        QVERIFY2(panelStatus->text().contains(QStringLiteral("only in the File menu")), qPrintable(panelStatus->text()));
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::cloudActivityInTray());
        if (QMenu *menu = tray->contextMenu()) {
            QVERIFY(!menu->findChild<QAction *>(QStringLiteral("trayCloudActivity"))->isVisible());
        }
        auto *trayIcon = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTrayIcon"));
        QVERIFY(trayIcon);
        trayIcon->setChecked(false);
        QVERIFY2(panelStatus->text().contains(QStringLiteral("turned off")), qPrintable(panelStatus->text()));
    }

    void lanSharingTabSetsTheDefaultsOfSharing()
    {
        const auto config = KSharedConfig::openConfig();
        const auto restore = qScopeGuard([config] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            config->reparseConfiguration();
        });
        QTemporaryDir uploads;
        QVERIFY(uploads.isValid());

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("lanSharingIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 7);
        QCOMPARE(tabs->indexOf(page), 6);
        const QStringList order{QStringLiteral("Connections"),
                                QStringLiteral("rclone"),
                                QStringLiteral("Remote Drives"),
                                QStringLiteral("Sync"),
                                QStringLiteral("Nextcloud"),
                                QStringLiteral("Transfers and Cache"),
                                QStringLiteral("LAN Sharing")};
        for (int i = 0; i < order.size(); ++i) {
            QCOMPARE(tabs->tabText(i), order.at(i));
        }

        auto *protocol = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_LanShareProtocol"));
        auto *https = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_LanShareHttps"));
        auto *port = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_LanSharePort"));
        auto *timeout = dialog.findChild<QSpinBox *>(QStringLiteral("kcfg_LanShareTimeoutMinutes"));
        auto *interfaces = dialog.findChild<HiddenItemsList *>(QStringLiteral("kcfg_LanShareExcludedInterfaces"));
        auto *allowUploads = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_LanShareAllowUploads"));
        auto *folder = dialog.findChild<KUrlRequester *>(QStringLiteral("kcfg_LanShareUploadFolder"));
        auto *connectionStatus = dialog.findChild<QLabel *>(QStringLiteral("lanShareConnectionStatus"));
        auto *interfacesStatus = dialog.findChild<QLabel *>(QStringLiteral("lanShareInterfacesStatus"));
        auto *uploadsStatus = dialog.findChild<QLabel *>(QStringLiteral("lanShareUploadsStatus"));
        auto *note = dialog.findChild<QLabel *>(QStringLiteral("lanShareDefaultsNote"));
        QVERIFY(protocol && https && port && timeout && interfaces && allowUploads && folder && connectionStatus && interfacesStatus && uploadsStatus && note);
        for (QWidget *widget : std::initializer_list<QWidget *>{protocol, https, port, timeout, interfaces, allowUploads, folder, note}) {
            QVERIFY(page->isAncestorOf(widget));
        }

        // The defaults match what File › Share on Local Network… started with before.
        QCOMPARE(protocol->currentIndex(), int(Settings::EnumLanShareProtocol::BrowsersAndWebDav));
        QVERIFY(https->isChecked());
        QCOMPARE(port->value(), 0);
        QCOMPARE(timeout->value(), 30);
        QVERIFY(!allowUploads->isChecked());
        QVERIFY(!folder->isEnabled());
        QVERIFY(interfaces->hiddenItems().isEmpty());
        QCOMPARE(interfaces->count(), LanShare::networkInterfaces().size());
        QVERIFY2(interfacesStatus->text().contains(QStringLiteral("Public addresses")), qPrintable(interfacesStatus->text()));
        QVERIFY2(uploadsStatus->text().contains(QStringLiteral("read-only")), qPrintable(uploadsStatus->text()));
        QVERIFY(note->text().contains(QStringLiteral("File › Share on Local Network…")));

        // The status follows the choices.
        https->setChecked(false);
        QVERIFY2(connectionStatus->text().contains(QStringLiteral("can see the links")), qPrintable(connectionStatus->text()));
        port->setValue(80);
        QVERIFY2(connectionStatus->text().contains(QStringLiteral("administrator")), qPrintable(connectionStatus->text()));
        port->setValue(8765);
        QVERIFY2(connectionStatus->text().contains(QStringLiteral("8765")), qPrintable(connectionStatus->text()));
        protocol->setCurrentIndex(int(Settings::EnumLanShareProtocol::WebDav));
        QVERIFY2(connectionStatus->text().contains(QStringLiteral("no folder pages")), qPrintable(connectionStatus->text()));
        allowUploads->setChecked(true);
        QVERIFY(folder->isEnabled());
        folder->setUrl(QUrl::fromLocalFile(uploads.path()));
        QVERIFY2(uploadsStatus->text().contains(QDir::toNativeSeparators(uploads.path())), qPrintable(uploadsStatus->text()));
        folder->setUrl(QUrl::fromLocalFile(uploads.filePath(QStringLiteral("missing"))));
        QVERIFY2(uploadsStatus->text().contains(QStringLiteral("can't receive uploads")), qPrintable(uploadsStatus->text()));
        folder->setUrl(QUrl::fromLocalFile(uploads.path()));
        timeout->setValue(45);
        if (interfaces->count() > 0) {
            interfaces->item(0)->setCheckState(Qt::Unchecked);
        }
        const QStringList excluded = interfaces->hiddenItems();

        // Applying saves the defaults, and the next share starts from them.
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::lanShareProtocol(), Settings::EnumLanShareProtocol::WebDav);
        QVERIFY(!Settings::lanShareHttps());
        QCOMPARE(Settings::lanSharePort(), 8765);
        QCOMPARE(Settings::lanShareTimeoutMinutes(), 45);
        QVERIFY(Settings::lanShareAllowUploads());
        QCOMPARE(Settings::lanShareExcludedInterfaces(), excluded);
        config->reparseConfiguration();
        QCOMPARE(config->group(QStringLiteral("LanShare")).readEntry("Port", 0), 8765);

        LanShareDialog share({uploads.path()}, &window);
        const LanShareServer::Options options = share.options();
        QCOMPARE(options.protocol, LanShare::Protocol::WebDav);
        QVERIFY(!options.tls);
        QCOMPARE(options.port, quint16(8765));
        QCOMPARE(options.timeoutSeconds, 45 * 60);
        QCOMPARE(options.uploadFolder, uploads.path());
        share.setAddresses(QList<LanShare::InterfaceAddress>{{QHostAddress(QStringLiteral("192.168.7.2")), QStringLiteral("opustest0")}});
        QCOMPARE(share.checkedAddresses().size(), 1);
    }

    void securityPageHasTheMcpTab()
    {
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("mcpIntegrationPage"));
        QVERIFY(page);
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QVERIFY(tabs);
        QCOMPARE(tabs->indexOf(page), 0);
        QCOMPARE(tabs->tabText(0), QStringLiteral("MCP"));
        auto *pages = dialog.findChild<KPageWidget *>();
        QVERIFY(pages);
        const QAbstractItemModel *model = pages->model();
        QStringList titles;
        for (int row = 0; row < model->rowCount(); ++row) {
            titles.append(model->index(row, 0).data(Qt::DisplayRole).toString());
        }
        const qsizetype security = titles.indexOf(QStringLiteral("Security"));
        QVERIFY2(security > 0, qPrintable(titles.join(u',')));
        QCOMPARE(titles.at(security - 1), QStringLiteral("Remote and Cloud"));
    }

    void mcpTabSetsTheServerAccessFoldersAndAuditLog()
    {
        const auto config = KSharedConfig::openConfig();
        const auto restore = qScopeGuard([config] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            config->reparseConfiguration();
        });
        QTemporaryDir folders;
        QVERIFY(folders.isValid());
        const QString work = QFileInfo(folders.path()).canonicalFilePath() + QStringLiteral("/work");
        const QString secret = QFileInfo(folders.path()).canonicalFilePath() + QStringLiteral("/secret");
        QVERIFY(QDir().mkpath(work));
        QVERIFY(QDir().mkpath(secret));
        const QString log = McpSettings::auditLogPath();
        QFile::remove(log);

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("mcpIntegrationPage"));
        auto *access = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_McpAccess"));
        auto *list = dialog.findChild<FolderListEditor *>(QStringLiteral("kcfg_McpFolders"));
        auto *auditLog = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_McpAuditLog"));
        auto *accessStatus = dialog.findChild<QLabel *>(QStringLiteral("mcpAccessStatus"));
        auto *foldersStatus = dialog.findChild<QLabel *>(QStringLiteral("mcpFoldersStatus"));
        auto *auditStatus = dialog.findChild<QLabel *>(QStringLiteral("mcpAuditStatus"));
        auto *tokenNote = dialog.findChild<QLabel *>(QStringLiteral("mcpTokenNote"));
        auto *configuration = dialog.findChild<QLineEdit *>(QStringLiteral("mcpClientConfiguration"));
        auto *copy = dialog.findChild<QPushButton *>(QStringLiteral("mcpCopyConfiguration"));
        auto *showLog = dialog.findChild<QPushButton *>(QStringLiteral("mcpShowAuditLog"));
        QVERIFY(page && access && list && auditLog && accessStatus && foldersStatus && auditStatus && tokenNote && configuration && copy && showLog);
        for (QWidget *widget : std::initializer_list<QWidget *>{access, list, auditLog, tokenNote, configuration, copy, showLog}) {
            QVERIFY(page->isAncestorOf(widget));
        }

        // The defaults keep what the server did before: reading the home folder, with a log.
        QCOMPARE(access->currentIndex(), int(Settings::EnumMcpAccess::Read));
        QVERIFY(list->folders().isEmpty());
        QVERIFY(auditLog->isChecked());
        QVERIFY2(accessStatus->text().contains(QStringLiteral("not change anything")), qPrintable(accessStatus->text()));
        QVERIFY2(foldersStatus->text().contains(QStringLiteral("home folder")), qPrintable(foldersStatus->text()));
        QVERIFY2(auditStatus->text().contains(QStringLiteral("No tool call has been recorded yet")), qPrintable(auditStatus->text()));
        QVERIFY(!showLog->isEnabled());
        QVERIFY(tokenNote->text().contains(QStringLiteral("No access token")));
        QCOMPARE(configuration->text(), McpSettings::clientConfiguration());
        copy->click();
        QCOMPARE(QGuiApplication::clipboard()->text(), McpSettings::clientConfiguration());

        // The status follows the choices.
        access->setCurrentIndex(int(Settings::EnumMcpAccess::Off));
        QVERIFY2(accessStatus->text().contains(QStringLiteral("no tools")), qPrintable(accessStatus->text()));
        QVERIFY(!list->isEnabled());
        QVERIFY(!copy->isEnabled());
        QVERIFY2(auditStatus->text().contains(QStringLiteral("Nothing is recorded")), qPrintable(auditStatus->text()));
        access->setCurrentIndex(int(Settings::EnumMcpAccess::Write));
        QVERIFY2(accessStatus->text().contains(QStringLiteral("move items to the trash")), qPrintable(accessStatus->text()));
        QVERIFY(list->isEnabled());
        QVERIFY(list->addFolder(work));
        QVERIFY(list->addFolder(folders.filePath(QStringLiteral("missing"))));
        QVERIFY2(foldersStatus->text().contains(QStringLiteral("skips ") + folders.filePath(QStringLiteral("missing"))), qPrintable(foldersStatus->text()));
        QVERIFY(list->addFolder(secret));
        FolderGates::instance()->setIterations(10);
        QVERIFY(FolderGates::instance()->protect(secret, QStringLiteral("pw")).isEmpty());
        const auto unprotect = qScopeGuard([secret] {
            FolderGates::instance()->removeProtection(secret, QStringLiteral("pw"));
        });
        QVERIFY2(foldersStatus->text().contains(QStringLiteral("refused ") + secret), qPrintable(foldersStatus->text()));
        auditLog->setChecked(false);
        QVERIFY2(auditStatus->text().contains(QStringLiteral("aren't recorded")), qPrintable(auditStatus->text()));

        // Applying saves the settings that the server reads for each request.
        QTRY_VERIFY(dialog.button(QDialogButtonBox::Apply)->isEnabled());
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::mcpAccess(), Settings::EnumMcpAccess::Write);
        QCOMPARE(Settings::mcpFolders(), (QStringList{work, folders.filePath(QStringLiteral("missing")), secret}));
        QVERIFY(!Settings::mcpAuditLog());
        config->reparseConfiguration();
        QCOMPARE(config->group(QStringLiteral("Mcp")).readEntry("McpAccess"), QStringLiteral("Write"));

        // Show Audit Log asks the window to select the log once there is one.
        QVERIFY(QDir().mkpath(QFileInfo(log).path()));
        write(log, QStringLiteral("2026-11-27T10:00:00Z list_folder ok\n"));
        const auto removeLog = qScopeGuard([log] {
            QFile::remove(log);
        });
        auditLog->setChecked(true);
        QVERIFY(showLog->isEnabled());
        QVERIFY2(auditStatus->text().contains(QDir::toNativeSeparators(log)), qPrintable(auditStatus->text()));
        QSignalSpy shown(&dialog, &SettingsDialog::showItemsRequested);
        showLog->click();
        QCOMPARE(shown.size(), 1);
        QCOMPARE(shown.at(0).at(0).value<QList<QUrl>>(), QList<QUrl>{QUrl::fromLocalFile(log)});
    }

    void passwordGatedFoldersTabManagesProtectionImmediately()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString secret = QFileInfo(root.path()).canonicalFilePath() + QStringLiteral("/secret");
        QVERIFY(QDir().mkpath(secret));
        FolderGates *gates = FolderGates::instance();
        gates->setIterations(10);
        const auto cleanup = qScopeGuard([gates, secret] {
            for (const QString &password : {QStringLiteral("first password 1"), QStringLiteral("second password 2")}) {
                gates->removeProtection(secret, password);
            }
        });
        QVERIFY(!gates->isGate(secret));

        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        dialog.setFolderGateChooser([secret] {
            return secret;
        });
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("folderGatesIntegrationPage"));
        QVERIFY(page);
        QCOMPARE(tabTitleOf(page->findChild<QWidget *>(QStringLiteral("folderGatesGroup"))), QStringLiteral("Password-Gated Folders"));
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 1);
        auto *warning = page->findChild<KMessageWidget *>(QStringLiteral("folderGatesWarning"));
        auto *list = page->findChild<QTreeWidget *>(QStringLiteral("folderGatesList"));
        auto *status = page->findChild<QLabel *>(QStringLiteral("folderGatesStatus"));
        auto *note = page->findChild<QLabel *>(QStringLiteral("folderGatesNote"));
        const auto button = [page](const char *name) {
            return page->findChild<QPushButton *>(QLatin1String(name));
        };
        QVERIFY(warning && list && status && note);
        for (const char *name :
             {"folderGatesProtect", "folderGatesUnlock", "folderGatesLock", "folderGatesLockAll", "folderGatesChangePassword", "folderGatesRemove"}) {
            QVERIFY2(button(name), name);
        }
        QCOMPARE(warning->messageType(), KMessageWidget::Warning);
        QVERIFY(warning->text().contains(QStringLiteral("only inside Plasma Opus")));
        QVERIFY(warning->text().contains(QStringLiteral("not encryption")));
        QVERIFY(note->text().contains(QStringLiteral("immediately")));
        QCOMPARE(list->topLevelItemCount(), 0);
        QVERIFY2(status->text().contains(QStringLiteral("No folders")), qPrintable(status->text()));
        QVERIFY(button("folderGatesProtect")->isEnabled());
        for (const char *name : {"folderGatesUnlock", "folderGatesLock", "folderGatesLockAll", "folderGatesChangePassword", "folderGatesRemove"}) {
            QVERIFY2(!button(name)->isEnabled(), name);
        }

        const auto findDialog = [&dialog](const QString &name) -> QDialog * {
            const QList<QDialog *> dialogs = dialog.findChildren<QDialog *>(name);
            for (QDialog *candidate : dialogs) {
                if (candidate->isVisible()) {
                    return candidate;
                }
            }
            return nullptr;
        };
        const auto enterNewPassword = [&findDialog](const QString &name, const QString &password) {
            QDialog *newPassword = nullptr;
            QTRY_VERIFY((newPassword = findDialog(name)));
            const QList<QLineEdit *> edits = newPassword->findChildren<QLineEdit *>();
            QCOMPARE(edits.size(), 2);
            for (QLineEdit *edit : edits) {
                edit->setText(password);
            }
            const QPointer<QDialog> guard(newPassword);
            newPassword->accept();
            QTRY_VERIFY(guard.isNull());
        };
        const auto enterPassword = [&findDialog](const QString &name, const QString &password, bool accepted) {
            auto *prompt = qobject_cast<KPasswordDialog *>(findDialog(name));
            QVERIFY(prompt);
            const QPointer<QDialog> guard(prompt);
            prompt->setPassword(password);
            prompt->accept();
            if (accepted) {
                QTRY_VERIFY(guard.isNull());
            } else {
                QTest::qWait(50);
                QVERIFY(guard && guard->isVisible());
            }
        };

        // Protect Folder… asks for a folder and a new password, and the gate applies at once.
        button("folderGatesProtect")->click();
        enterNewPassword(QStringLiteral("folderGateProtectDialog"), QStringLiteral("first password 1"));
        QVERIFY(gates->isGate(secret));
        QVERIFY(gates->isGateLocked(secret));
        QCOMPARE(list->topLevelItemCount(), 1);
        QCOMPARE(list->topLevelItem(0)->data(0, Qt::UserRole).toString(), secret);
        QCOMPARE(list->topLevelItem(0)->text(1), QStringLiteral("Locked"));
        QVERIFY2(status->text().contains(QStringLiteral("1 folder is protected")), qPrintable(status->text()));

        // Choosing a protected folder again selects it instead of asking for a password.
        list->setCurrentItem(nullptr);
        button("folderGatesProtect")->click();
        QVERIFY(!findDialog(QStringLiteral("folderGateProtectDialog")));
        QCOMPARE(list->currentItem(), list->topLevelItem(0));
        QVERIFY2(status->text().contains(QStringLiteral("already protected")), qPrintable(status->text()));

        // Unlocking keeps the prompt open after a wrong password, and unlocks every window.
        list->setCurrentItem(list->topLevelItem(0));
        QVERIFY(button("folderGatesUnlock")->isEnabled());
        QVERIFY(!button("folderGatesLock")->isEnabled());
        button("folderGatesUnlock")->click();
        enterPassword(QStringLiteral("folderGateUnlockDialog"), QStringLiteral("wrong"), false);
        QVERIFY(gates->isGateLocked(secret));
        enterPassword(QStringLiteral("folderGateUnlockDialog"), QStringLiteral("first password 1"), true);
        QVERIFY(!gates->isGateLocked(secret));
        QCOMPARE(list->topLevelItem(0)->text(1), QStringLiteral("Unlocked"));
        QVERIFY(window.actionCollection()->action(QStringLiteral("lock_protected_folders"))->isEnabled());
        QVERIFY(button("folderGatesLock")->isEnabled());
        QVERIFY(button("folderGatesLockAll")->isEnabled());
        button("folderGatesLock")->click();
        QVERIFY(gates->isGateLocked(secret));
        QVERIFY(!button("folderGatesLockAll")->isEnabled());
        QCOMPARE(gates->unlock(secret, QStringLiteral("first password 1")), FolderGates::Check::Unlocked);
        QVERIFY(button("folderGatesLockAll")->isEnabled());
        button("folderGatesLockAll")->click();
        QVERIFY(gates->isGateLocked(secret));

        // Change Password… checks the current password, then asks for a new one.
        list->setCurrentItem(list->topLevelItem(0));
        button("folderGatesChangePassword")->click();
        enterPassword(QStringLiteral("folderGateChangeDialog"), QStringLiteral("first password 1"), true);
        enterNewPassword(QStringLiteral("folderGateNewPasswordDialog"), QStringLiteral("second password 2"));
        QCOMPARE(gates->checkPassword(secret, QStringLiteral("second password 2")), QString());

        // Cancelling Preferences keeps the changes, which only store a verifier.
        dialog.reject();
        const QString stored = KSharedConfig::openConfig()->group(QStringLiteral("FolderGates")).readEntry("Gates", QStringList()).join(u'\n');
        QVERIFY(gates->isGate(secret));
        QVERIFY(!stored.contains(QStringLiteral("second password 2")));

        list->setCurrentItem(list->topLevelItem(0));
        button("folderGatesRemove")->click();
        enterPassword(QStringLiteral("folderGateRemoveDialog"), QStringLiteral("second password 2"), true);
        QVERIFY(!gates->isGate(secret));
        QCOMPARE(list->topLevelItemCount(), 0);
    }

    void encryptionTabChoosesTheVaultsAndTheirState()
    {
        const auto config = KSharedConfig::openConfig();
        const auto restore = qScopeGuard([config] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            config->reparseConfiguration();
        });
        const auto vault = [](const QString &name, PlasmaVaults::Status status) {
            PlasmaVaults::Vault result;
            result.name = name;
            result.device = QStringLiteral("/vaults/") + name + QStringLiteral(".enc");
            result.mountPoint = QStringLiteral("/home/user/Vaults/") + name;
            result.status = status;
            return result;
        };
        MainWindow window;
        auto *client = new FakeVaultClient;
        client->list = {vault(QStringLiteral("Personal"), PlasmaVaults::Status::Opened),
                        vault(QStringLiteral("Work"), PlasmaVaults::Status::Closed),
                        vault(QStringLiteral("Old"), PlasmaVaults::Status::DeviceMissing)};
        window.setVaultClient(client);
        auto *vaultsAction = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("plasma_vaults")));
        QVERIFY(vaultsAction && vaultsAction->isVisible());
        const auto menuTitles = [&window, vaultsAction] {
            window.plasmaVaultMenu()->populate();
            QStringList titles;
            for (QAction *action : vaultsAction->menu()->actions()) {
                if (!action->isSeparator() && action->objectName() != QStringLiteral("createVault")) {
                    titles.append(action->text());
                }
            }
            return titles;
        };
        QCOMPARE(menuTitles(),
                 (QStringList{QStringLiteral("Personal (Open)"), QStringLiteral("Work (Closed)"), QStringLiteral("Old (Encrypted data missing)")}));

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *page = dialog.findChild<QWidget *>(QStringLiteral("encryptionIntegrationPage"));
        auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PlasmaVaultMenu"));
        auto *vaults = dialog.findChild<HiddenItemsList *>(QStringLiteral("kcfg_PlasmaVaultHidden"));
        auto *showState = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_PlasmaVaultShowState"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("plasmaVaultStatus"));
        auto *refresh = dialog.findChild<QPushButton *>(QStringLiteral("refreshPlasmaVaults"));
        QVERIFY(page && enabled && vaults && showState && status && refresh);
        QCOMPARE(tabTitleOf(enabled), QStringLiteral("Encryption"));
        auto *tabs = qobject_cast<QTabWidget *>(page->parentWidget()->parentWidget());
        QCOMPARE(tabs->indexOf(page), 2);
        QVERIFY(enabled->isChecked());
        QVERIFY(showState->isChecked());
        // Plasma Vault is not asked for its vaults before the list is shown.
        QCOMPARE(client->reads, 1);

        refresh->click();
        QCOMPARE(client->reads, 2);
        QCOMPARE(vaults->count(), 3);
        QCOMPARE(vaults->item(0)->text(), QStringLiteral("Personal (Open)"));
        QCOMPARE(vaults->item(1)->text(), QStringLiteral("Work (Closed)"));
        QVERIFY(vaults->item(1)->toolTip().contains(QStringLiteral("/vaults/Work.enc")));
        QVERIFY2(status->text().contains(QStringLiteral("3 vaults")) && status->text().contains(QStringLiteral("One is open")), qPrintable(status->text()));

        // A cleared vault is left out of Go › Plasma Vaults, and its messages out of the status bar.
        vaults->item(1)->setCheckState(Qt::Unchecked);
        dialog.button(QDialogButtonBox::Apply)->click();
        QCOMPARE(Settings::plasmaVaultHidden(), QStringList{QStringLiteral("/vaults/Work.enc")});
        QCOMPARE(menuTitles(), (QStringList{QStringLiteral("Personal (Open)"), QStringLiteral("Old (Encrypted data missing)")}));
        QSignalSpy messages(window.plasmaVaultMenu(), &PlasmaVaultMenu::statusMessage);
        PlasmaVaults::Vault failed = client->list.at(1);
        failed.message = QStringLiteral("Wrong password");
        Q_EMIT client->vaultChanged(failed);
        QCOMPARE(messages.size(), 0);
        failed = client->list.at(0);
        failed.message = QStringLiteral("Busy");
        Q_EMIT client->vaultChanged(failed);
        QCOMPARE(messages.size(), 1);

        // Without the state, open and closed vaults show only their names; problems are still reported.
        showState->setChecked(false);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::plasmaVaultShowState());
        QCOMPARE(menuTitles(), (QStringList{QStringLiteral("Personal"), QStringLiteral("Old (Encrypted data missing)")}));

        // Hiding every vault says so in the menu.
        vaults->item(0)->setCheckState(Qt::Unchecked);
        vaults->item(2)->setCheckState(Qt::Unchecked);
        dialog.button(QDialogButtonBox::Apply)->click();
        const QStringList hiddenAll = menuTitles();
        QCOMPARE(hiddenAll.size(), 1);
        QVERIFY2(hiddenAll.first().contains(QStringLiteral("3 Vaults Hidden")), qPrintable(hiddenAll.first()));

        // Hiding the menu hides Go › Plasma Vaults and stops asking Plasma Vault.
        enabled->setChecked(false);
        QVERIFY(!vaults->isEnabled());
        QVERIFY(!showState->isEnabled());
        QVERIFY(!refresh->isEnabled());
        const int reads = client->reads;
        refresh->click();
        QCOMPARE(client->reads, reads);
        QVERIFY2(status->text().contains(QStringLiteral("hidden")), qPrintable(status->text()));
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::plasmaVaultMenu());
        QVERIFY(!vaultsAction->isVisible());
        Q_EMIT client->vaultChanged(failed);
        QCOMPARE(messages.size(), 1);
        enabled->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(vaultsAction->isVisible());
    }

    void encryptionTabChoosesTheFileActionsAndNames()
    {
        const auto config = KSharedConfig::openConfig();
        const auto restore = qScopeGuard([config] {
            Settings::self()->setDefaults();
            Settings::self()->save();
            config->reparseConfiguration();
        });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString notes = dir.filePath(QStringLiteral("notes.txt"));
        QFile file(notes);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("notes");
        file.close();
        MainWindow window;
        Pane *pane = window.activePane();
        pane->setUrl(QUrl::fromLocalFile(dir.path()));
        QTRY_COMPARE(pane->view()->itemCount(), 1);
        pane->view()->selectUrls({QUrl::fromLocalFile(notes)});
        QTRY_COMPARE(pane->view()->selectedItems().size(), 1);
        KActionCollection *actions = window.actionCollection();
        const auto action = [actions](const char *name) {
            return actions->action(QLatin1String(name));
        };
        QVERIFY(action("encryption")->isVisible());
        QVERIFY(action("encrypt_files")->isVisible());
        QVERIFY(action("aes_encrypt_files")->isVisible());

        SettingsDialog dialog(placesOf(window), &window);
        connect(&dialog, &KConfigDialog::settingsChanged, &window, &MainWindow::applySettings);
        auto *group = dialog.findChild<QGroupBox *>(QStringLiteral("fileEncryptionGroup"));
        auto *gnupg = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_GnuPGActions"));
        auto *aesCrypt = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_AesCryptActions"));
        auto *armor = dialog.findChild<QCheckBox *>(QStringLiteral("kcfg_EncryptionArmor"));
        auto *suffix = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_EncryptedSuffix"));
        auto *decrypted = dialog.findChild<QComboBox *>(QStringLiteral("kcfg_DecryptedNames"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("fileEncryptionStatus"));
        QVERIFY(group && gnupg && aesCrypt && armor && suffix && decrypted && status);
        QCOMPARE(tabTitleOf(gnupg), QStringLiteral("Encryption"));
        QVERIFY(gnupg->isChecked() && aesCrypt->isChecked() && !armor->isChecked());
        QCOMPARE(suffix->currentIndex(), 0);
        QCOMPARE(decrypted->currentIndex(), 0);
        QVERIFY2(status->text().contains(QStringLiteral("GnuPG")) && status->text().contains(QStringLiteral("aescrypt")), qPrintable(status->text()));

        // The text format and names are used by the next encryption window.
        armor->setChecked(true);
        suffix->setCurrentIndex(1);
        decrypted->setCurrentIndex(1);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(Settings::encryptionArmor());
        QCOMPARE(Settings::encryptedSuffix(), int(Settings::EnumEncryptedSuffix::Pgp));
        QCOMPARE(Settings::decryptedNames(), int(Settings::EnumDecryptedNames::Marked));
        {
            // Without GnuPG in PATH, the window only shows its options rather than reading keys.
            const QByteArray path = qgetenv("PATH");
            qputenv("PATH", dir.filePath(QStringLiteral("empty")).toLocal8Bit());
            FileEncryptionDialog encrypt(FileEncryption::Operation::Encrypt, {notes});
            qputenv("PATH", path);
            auto *encryptArmor = encrypt.findChild<QCheckBox *>(QStringLiteral("encryptionArmor"));
            QVERIFY(encryptArmor && encryptArmor->isChecked());
        }

        // Turning off AES Crypt hides only its actions, and its shortcuts do nothing.
        aesCrypt->setChecked(false);
        QVERIFY(suffix->isEnabled() && decrypted->isEnabled());
        QVERIFY(!status->text().contains(QStringLiteral("aescrypt")));
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!Settings::aesCryptActions());
        QVERIFY(!action("aes_encrypt_files")->isVisible());
        QVERIFY(!action("aes_decrypt_files")->isVisible());
        QVERIFY(action("encrypt_files")->isVisible());
        QVERIFY(action("encryption")->isVisible());
        action("aes_encrypt_files")->trigger();
        QVERIFY(!window.findChild<FileEncryptionDialog *>());

        // Turning off both hides File › Encryption.
        gnupg->setChecked(false);
        QVERIFY(!armor->isEnabled() && !suffix->isEnabled() && !decrypted->isEnabled());
        QVERIFY2(status->text().contains(QStringLiteral("hidden")), qPrintable(status->text()));
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(!action("encryption")->isVisible());
        for (const char *name : {"encrypt_files", "decrypt_files", "sign_files", "verify_signatures", "kleopatra_encrypt_sign", "kleopatra_decrypt_verify"}) {
            QVERIFY2(!action(name)->isVisible(), name);
        }
        action("encrypt_files")->trigger();
        QVERIFY(!window.findChild<FileEncryptionDialog *>());

        gnupg->setChecked(true);
        aesCrypt->setChecked(true);
        dialog.button(QDialogButtonBox::Apply)->click();
        QVERIFY(action("encryption")->isVisible());
        QVERIFY(action("encrypt_files")->isVisible());
        QVERIFY(action("aes_encrypt_files")->isVisible());
        action("aes_encrypt_files")->trigger();
        auto *opened = window.findChild<FileEncryptionDialog *>();
        QVERIFY(opened);
        opened->close();
    }
};

QTEST_MAIN(IntegrationSettingsTest)

#include "integrationsettingstest.moc"
