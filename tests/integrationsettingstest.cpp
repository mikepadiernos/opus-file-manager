// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityassociations.h"
#include "activityassociationseditor.h"
#include "activitycontroller.h"
#include "appidentity.h"
#include "defaultfilemanager.h"
#include "desktoptray.h"
#include "filechooser.h"
#include "mainwindow.h"
#include "operationjob.h"
#include "portalsetup.h"
#include "settings.h"
#include "settingsdialog.h"

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

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTest>

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

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        ksycoca_ms_between_checks = 0;
        m_applications = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/applications");
        m_configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        // The desktop-specific list of the developer's session must not apply.
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
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
};

QTEST_MAIN(IntegrationSettingsTest)

#include "integrationsettingstest.moc"
