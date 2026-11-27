// SPDX-License-Identifier: GPL-3.0-or-later

#include "appidentity.h"
#include "defaultfilemanager.h"
#include "mainwindow.h"
#include "portalsetup.h"
#include "settingsdialog.h"

#include <KConfig>
#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KMessageWidget>
#include <KService>
#include <KSharedConfig>
#include <KSycoca>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTest>

extern KSERVICE_EXPORT int ksycoca_ms_between_checks;

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
        KSharedConfig::openStateConfig()->sync();
        qputenv("XDG_CURRENT_DESKTOP", "KDE");
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

    void notesASessionOutsidePlasma()
    {
        qputenv("XDG_CURRENT_DESKTOP", "GNOME");
        MainWindow window;
        SettingsDialog dialog(placesOf(window), &window);
        setPortalHooks(dialog, true);
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("fileChooserPortalStatus"))->text().contains(QStringLiteral("not a Plasma session")));
    }
};

QTEST_MAIN(IntegrationSettingsTest)

#include "integrationsettingstest.moc"
