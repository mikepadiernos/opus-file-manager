// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "activityassociationseditor.h"
#include "activitycontroller.h"
#include "appidentity.h"
#include "defaultfilemanager.h"
#include "desktoptray.h"
#include "favoritesmodel.h"
#include "filechooser.h"
#include "filemanagerservice.h"
#include "kwinintegration.h"
#include "kwinscript.h"
#include "onlineaccounts.h"
#include "portalsetup.h"
#include "runnermatches.h"
#include "settingswidgets.h"
#include "traypolicy.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KIO/CommandLauncherJob>
#include <KKeySequenceWidget>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KPluginMetaData>
#include <KProtocolInfo>
#include <KService>
#include <KSharedConfig>
#include <KToolBar>
#include <KXmlGuiWindow>

#include <QApplication>
#include <QCheckBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>
#include <functional>
#include <memory>

namespace
{
/// A group of integration controls that reads their status again whenever it is shown.
class StatusGroup : public QGroupBox
{
public:
    using QGroupBox::QGroupBox;
    std::function<void()> refresh;

protected:
    void showEvent(QShowEvent *event) override
    {
        QGroupBox::showEvent(event);
        if (refresh) {
            refresh();
        }
    }
};

QLabel *statusLabel(const QString &name)
{
    auto *label = new QLabel;
    label->setObjectName(name);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QPushButton *actionButton(const QString &name, const QString &text)
{
    auto *button = new QPushButton(text);
    button->setObjectName(name);
    return button;
}

QHBoxLayout *buttonRow(const QList<QPushButton *> &buttons)
{
    auto *row = new QHBoxLayout;
    for (QPushButton *button : buttons) {
        row->addWidget(button);
    }
    row->addStretch();
    return row;
}

KMessageWidget *messageWidget(const QString &name)
{
    auto *message = new KMessageWidget;
    message->setObjectName(name);
    message->setWordWrap(true);
    message->setCloseButtonVisible(true);
    message->hide();
    return message;
}

void report(KMessageWidget *message, KMessageWidget::MessageType type, const QString &text)
{
    message->setMessageType(type);
    message->setText(text);
    message->animatedShow();
}

/// Opens the System Settings module @p module, reporting on @p message when it cannot.
/// @p args are passed to the module, as with kcmshell6 --args.
void openSettingsModule(const QString &module, const QString &missing, KMessageWidget *message, const QString &args = {})
{
    // QWidgets modules, such as KDE Connect's, are installed apart from the QML ones.
    if (!KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), module).isValid()
        && !KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings_qwidgets"), module).isValid()) {
        report(message, KMessageWidget::Error, missing);
        return;
    }
    QStringList arguments{module};
    if (!args.isEmpty()) {
        arguments << QStringLiteral("--args") << args;
    }
    const QString kcmshell = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
    auto *job =
        kcmshell.isEmpty() ? new KIO::CommandLauncherJob(QStringLiteral("systemsettings"), arguments) : new KIO::CommandLauncherJob(kcmshell, arguments);
    QObject::connect(job, &KJob::result, message, [message](KJob *job) {
        if (job->error()) {
            report(message, KMessageWidget::Error, i18nc("@info %1 is an error message", "Could not open System Settings: %1", job->errorString()));
        }
    });
    job->start();
}

QString plasmaOpusExecutable()
{
    return QCoreApplication::applicationDirPath() + QLatin1Char('/') + AppIdentity::componentName();
}

QString fileManagerActivationFile()
{
    return DefaultFileManager::activationFile(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
}

QString activationStatus(DefaultFileManager::Activation state, const QString &target)
{
    using DefaultFileManager::Activation;
    switch (state) {
    case Activation::Missing:
        return i18nc("@info",
                     "No activation file is installed, so while no file manager is running, requests such as Show in Folder start "
                     "the file manager that the system provides for them, if any.");
    case Activation::Installed:
        return i18nc("@info", "The activation file is installed, so requests such as Show in Folder start Plasma Opus while no file manager is running.");
    case Activation::Elsewhere:
        return i18nc("@info", "The activation file starts another Plasma Opus installation, %1. Update it to start this one.", target);
    case Activation::Broken:
        return i18nc("@info",
                     "The activation file starts %1, which no longer exists, so requests such as Show in Folder fail while no file manager "
                     "is running. Update or remove it.",
                     target);
    case Activation::Foreign:
        return i18nc("@info", "Another application installed %1, so Plasma Opus leaves it unchanged.", fileManagerActivationFile());
    }
    return {};
}

PortalSetup::Paths portalPaths()
{
    return PortalSetup::Paths::fromEnvironment(QCoreApplication::applicationDirPath() + QStringLiteral("/opus-file-manager-portal"));
}

/// The state configuration, read again because the portal setup commands and the file chooser service also write it.
KSharedConfigPtr sharedState()
{
    const KSharedConfigPtr state = KSharedConfig::openStateConfig();
    state->reparseConfiguration();
    return state;
}

/// The backup of the previous portal configuration.
KConfigGroup portalBackup()
{
    return sharedState()->group(PortalSetup::backupGroupName());
}
}

using SettingsWidgets::bound;
using SettingsWidgets::checkBox;
using SettingsWidgets::comboBox;

QWidget *SettingsDialog::createFileManagerIntegrationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("fileManagerIntegrationPage"));
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(createDefaultFileManagerGroup());
    layout->addWidget(createFileChooserGroup());
    layout->addStretch();
    return page;
}

QGroupBox *SettingsDialog::createDefaultFileManagerGroup()
{
    auto *group = new StatusGroup(i18nc("@title:group", "Default File Manager"));
    group->setObjectName(QStringLiteral("defaultFileManagerGroup"));
    auto *layout = new QVBoxLayout(group);
    auto *status = statusLabel(QStringLiteral("defaultFileManagerStatus"));
    auto *setDefault = actionButton(QStringLiteral("setDefaultFileManager"), i18nc("@action:button", "Set Plasma Opus as Default"));
    setDefault->setToolTip(i18nc("@info:tooltip", "Open folders from other applications in Plasma Opus, for your user account"));
    auto *openSettings = actionButton(QStringLiteral("openDefaultApplications"), i18nc("@action:button", "Open Default Applications Settings"));
    openSettings->setToolTip(i18nc("@info:tooltip", "Choose the default file manager and other default applications in System Settings"));
    auto *activation = statusLabel(QStringLiteral("fileManagerActivationStatus"));
    auto *install = actionButton(QStringLiteral("installFileManagerActivation"), i18nc("@action:button", "Install Activation File"));
    auto *remove = actionButton(QStringLiteral("removeFileManagerActivation"), i18nc("@action:button", "Remove Activation File"));
    auto *message = messageWidget(QStringLiteral("fileManagerMessage"));
    layout->addWidget(status);
    layout->addLayout(buttonRow({setDefault, openSettings}));
    layout->addWidget(activation);
    layout->addLayout(buttonRow({install, remove}));
    layout->addWidget(message);

    // KService notices a new default only after its cache check interval,
    // so the default just set is shown until then.
    auto justSet = std::make_shared<bool>(false);
    group->refresh = [=] {
        const DefaultFileManager::Handler handler = DefaultFileManager::folderHandler();
        const bool installed = KService::serviceByDesktopName(AppIdentity::desktopFileName());
        const bool isDefault = *justSet || DefaultFileManager::isPlasmaOpus(handler.desktopName);
        if (isDefault) {
            status->setText(i18nc("@info", "Plasma Opus is the default file manager, so applications open folders in it."));
        } else if (handler.desktopName.isEmpty()) {
            status->setText(i18nc("@info", "No default file manager is set for folders."));
        } else {
            status->setText(i18nc("@info %1 is an application name", "%1 is the default file manager.", handler.name));
        }
        if (!installed && !isDefault) {
            status->setText(
                status->text() + QLatin1Char(' ')
                + i18nc("@info", "Plasma Opus is not installed as an application, so it cannot become the default. Install it with install.sh first."));
        }
        setDefault->setEnabled(installed && !isDefault);

        QString target;
        const auto state = DefaultFileManager::activationState(fileManagerActivationFile(), plasmaOpusExecutable(), &target);
        using DefaultFileManager::Activation;
        const bool replaceable = state == Activation::Missing || state == Activation::Elsewhere || state == Activation::Broken;
        QString text = activationStatus(state, target);
        if (replaceable && !isDefault) {
            text += QLatin1Char(' ')
                + i18nc("@info", "Set Plasma Opus as the default file manager before installing it, so it never takes these requests from your file manager.");
        }
        activation->setText(text);
        install->setText(state == Activation::Missing ? i18nc("@action:button", "Install Activation File") : i18nc("@action:button", "Update Activation File"));
        install->setEnabled(replaceable && isDefault);
        remove->setEnabled(state == Activation::Installed || state == Activation::Elsewhere || state == Activation::Broken);
    };

    connect(setDefault, &QPushButton::clicked, group, [=] {
        const QString error = DefaultFileManager::setPlasmaOpusAsDefault();
        if (!error.isEmpty()) {
            report(message, KMessageWidget::Error, error);
            group->refresh();
            return;
        }
        *justSet = true;
        group->refresh();
        QTimer::singleShot(std::chrono::seconds(2), group, [group, justSet] {
            *justSet = false;
            group->refresh();
        });
        QString text = i18nc("@info", "Plasma Opus is now the default file manager.");
        auto type = KMessageWidget::Positive;
        switch (FileManagerService::claimName()) {
        case FileManagerService::Claim::Claimed:
            text += QLatin1Char(' ') + i18nc("@info", "It now handles requests such as Show in Folder.");
            break;
        case FileManagerService::Claim::OwnedElsewhere:
            text += QLatin1Char(' ')
                + i18nc("@info",
                        "Another application, such as another file manager, still handles requests such as Show in Folder. Plasma Opus "
                        "takes them over when it starts after that application quits.");
            type = KMessageWidget::Warning;
            break;
        case FileManagerService::Claim::Unavailable:
            break;
        }
        if (DefaultFileManager::activationState(fileManagerActivationFile(), plasmaOpusExecutable()) == DefaultFileManager::Activation::Missing) {
            text += QLatin1Char(' ') + i18nc("@info", "Install the activation file so that these requests also start Plasma Opus when it is not running.");
        }
        report(message, type, text);
    });
    connect(openSettings, &QPushButton::clicked, group, [message] {
        openSettingsModule(QStringLiteral("kcm_componentchooser"), i18nc("@info", "Plasma's Default Applications settings are not installed."), message);
    });
    connect(install, &QPushButton::clicked, group, [=] {
        const QString error = DefaultFileManager::installActivationFile(fileManagerActivationFile(), plasmaOpusExecutable());
        if (error.isEmpty()) {
            DefaultFileManager::reloadBusConfiguration();
            report(message,
                   KMessageWidget::Positive,
                   i18nc("@info",
                         "Installed %1. Requests such as Show in Folder start Plasma Opus while no file manager is running.",
                         fileManagerActivationFile()));
        } else {
            report(message, KMessageWidget::Error, error);
        }
        group->refresh();
    });
    connect(remove, &QPushButton::clicked, group, [=] {
        const QString error = DefaultFileManager::removeActivationFile(fileManagerActivationFile());
        if (error.isEmpty()) {
            DefaultFileManager::reloadBusConfiguration();
            report(message, KMessageWidget::Positive, i18nc("@info", "Removed %1.", fileManagerActivationFile()));
        } else {
            report(message, KMessageWidget::Error, error);
        }
        group->refresh();
    });
    group->refresh();
    return group;
}

QGroupBox *SettingsDialog::createFileChooserGroup()
{
    if (!m_portalAvailable) {
        m_portalAvailable = PortalSetup::portalServiceAvailable;
        m_restartPortal = PortalSetup::restartPortal;
        m_confirmPortalRestart = [this](const QString &question) {
            return KMessageBox::warningContinueCancel(this,
                                                      question,
                                                      i18nc("@title:window", "Restart the File Chooser Service"),
                                                      KGuiItem(i18nc("@action:button", "Restart"), QStringLiteral("view-refresh")))
                == KMessageBox::Continue;
        };
    }
    auto *group = new StatusGroup(i18nc("@title:group", "System File Chooser"));
    group->setObjectName(QStringLiteral("fileChooserGroup"));
    auto *layout = new QVBoxLayout(group);
    auto *status = statusLabel(QStringLiteral("fileChooserPortalStatus"));
    auto *install = actionButton(QStringLiteral("installFileChooserPortal"), i18nc("@action:button", "Set Up"));
    install->setToolTip(i18nc("@info:tooltip", "Show the Plasma Opus file chooser when applications that use the XDG portal open or save files"));
    auto *restore = actionButton(QStringLiteral("restoreFileChooserPortal"), i18nc("@action:button", "Restore Previous Chooser"));
    restore->setToolTip(i18nc("@info:tooltip", "Show the file chooser that applications used before the Plasma Opus file chooser was set up"));
    auto *startForm = new QFormLayout;
    startForm->addRow(i18nc("@label:listbox", "Start folder:"),
                      comboBox(QStringLiteral("FileChooserStartFolder"),
                               {i18nc("@item:inlistbox file chooser start folder", "Home folder"),
                                i18nc("@item:inlistbox file chooser start folder", "Folder of the last choice")}));
    auto *startNote = new QLabel(i18nc("@info",
                                       "The Plasma Opus file chooser starts here when the application suggests no folder. The folder of the last "
                                       "choice is remembered for each application while this option is selected; applications that the portal "
                                       "does not identify share one folder."));
    startNote->setWordWrap(true);
    auto *forget = actionButton(QStringLiteral("forgetFileChooserFolders"), i18nc("@action:button", "Forget Remembered Folders"));
    forget->setToolTip(i18nc("@info:tooltip", "Forget the folder of each application's last choice"));
    auto *message = messageWidget(QStringLiteral("fileChooserPortalMessage"));
    layout->addWidget(status);
    layout->addLayout(buttonRow({install, restore}));
    layout->addLayout(startForm);
    layout->addWidget(startNote);
    layout->addLayout(buttonRow({forget}));
    layout->addWidget(message);

    group->refresh = [=, this] {
        const PortalSetup::Paths paths = portalPaths();
        const PortalSetup::State state = PortalSetup::state(paths);
        QStringList text;
        switch (state) {
        case PortalSetup::State::Installed:
            text.append(i18nc("@info", "Plasma Opus serves the system file chooser, so applications that use the XDG portal show it to open and save files."));
            break;
        case PortalSetup::State::NotInstalled:
            text.append(i18nc("@info",
                              "Applications that use the XDG portal show the system's own file chooser. Set up the Plasma Opus file chooser to "
                              "show it instead."));
            break;
        case PortalSetup::State::Partial:
            text.append(i18nc("@info",
                              "The Plasma Opus file chooser is only partly set up, for example because the portal configuration was changed after "
                              "setup. Complete the setup, or restore the previous chooser."));
            break;
        }
        const QFileInfo helper(paths.helperExecutable);
        const bool helperInstalled = helper.isFile() && helper.isExecutable();
        if (!helperInstalled) {
            text.append(i18nc("@info", "The file chooser service %1 is not installed. Install Plasma Opus with install.sh first.", paths.helperExecutable));
        }
        const bool available = m_portalAvailable();
        if (!available) {
            text.append(i18nc("@info", "xdg-desktop-portal is not available on the session bus, so applications cannot use the Plasma Opus file chooser."));
        }
        if (!PortalSetup::readsKdeConfig(QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")))) {
            text.append(i18nc("@info",
                              "This is not a Plasma session, and xdg-desktop-portal follows this setting only in Plasma sessions, so "
                              "applications here keep their usual chooser."));
        }
        status->setText(text.join(QLatin1Char(' ')));
        install->setText(state == PortalSetup::State::Partial ? i18nc("@action:button", "Complete Setup") : i18nc("@action:button", "Set Up"));
        install->setEnabled(state != PortalSetup::State::Installed && helperInstalled && available);
        restore->setEnabled(state != PortalSetup::State::NotInstalled || portalBackup().exists());
        forget->setEnabled(sharedState()->hasGroup(FileChooser::stateGroupName()));
    };

    const auto apply = [=, this](PortalSetup::Change change) {
        const bool installing = change == PortalSetup::Change::Install;
        const QString question = installing ? i18nc("@info",
                                                    "Setting up the file chooser restarts xdg-desktop-portal, which closes file dialogs that "
                                                    "are open in other applications. Continue?")
                                            : i18nc("@info",
                                                    "Restoring the previous file chooser restarts xdg-desktop-portal, which closes file dialogs "
                                                    "that are open in other applications. Continue?");
        if (!m_confirmPortalRestart(question)) {
            return;
        }
        KConfigGroup backup = portalBackup();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const PortalSetup::Outcome outcome = PortalSetup::change(change, portalPaths(), backup, !installing || m_portalAvailable(), m_restartPortal);
        QApplication::restoreOverrideCursor();
        const QString done = installing ? i18nc("@info", "Plasma Opus now serves the system file chooser for applications that use the XDG portal.")
                                        : i18nc("@info", "The previous system file chooser was restored.");
        if (!outcome.error.isEmpty()) {
            report(message, KMessageWidget::Error, outcome.error);
        } else if (!outcome.changed) {
            report(message, KMessageWidget::Information, i18nc("@info", "The Plasma Opus file chooser is not set up. Nothing was changed."));
        } else if (!outcome.restartError.isEmpty()) {
            report(message,
                   KMessageWidget::Warning,
                   done + QLatin1Char(' ') + outcome.restartError + QLatin1Char(' ') + i18nc("@info", "Log out and back in to apply the change."));
        } else {
            report(message, KMessageWidget::Positive, done);
        }
        group->refresh();
    };
    connect(install, &QPushButton::clicked, group, [apply] {
        apply(PortalSetup::Change::Install);
    });
    connect(restore, &QPushButton::clicked, group, [apply] {
        apply(PortalSetup::Change::Uninstall);
    });
    connect(forget, &QPushButton::clicked, group, [=] {
        const KSharedConfigPtr state = sharedState();
        state->deleteGroup(FileChooser::stateGroupName());
        if (state->sync()) {
            report(message, KMessageWidget::Positive, i18nc("@info", "The file chooser no longer remembers the folders of earlier choices."));
        } else {
            report(message, KMessageWidget::Error, i18nc("@info", "Could not forget the folders of earlier choices."));
        }
        group->refresh();
    });
    group->refresh();
    return group;
}

void SettingsDialog::setPortalSetupHooks(std::function<bool()> available, std::function<QString()> restart, std::function<bool(const QString &)> confirm)
{
    m_portalAvailable = std::move(available);
    m_restartPortal = std::move(restart);
    m_confirmPortalRestart = std::move(confirm);
    if (auto *group = findChild<QGroupBox *>(QStringLiteral("fileChooserGroup"))) {
        static_cast<StatusGroup *>(group)->refresh();
    }
}

QWidget *SettingsDialog::createNotificationsIntegrationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("notificationsIntegrationPage"));
    auto *layout = new QVBoxLayout(page);

    auto *operations = new QGroupBox(i18nc("@title:group", "Operations"));
    operations->setObjectName(QStringLiteral("operationNotificationsGroup"));
    auto *operationsLayout = new QVBoxLayout(operations);
    auto *intro = new QLabel(i18nc("@info", "Report the progress and result of these operations to Plasma, which shows them in its notifications:"));
    intro->setWordWrap(true);
    operationsLayout->addWidget(intro);
    operationsLayout->addWidget(checkBox(QStringLiteral("NotifyChecksums"), i18nc("@option:check", "Creating and verifying checksum files")));
    operationsLayout->addWidget(checkBox(QStringLiteral("NotifyFolderSync"), i18nc("@option:check", "Synchronizing folders")));
    operationsLayout->addWidget(checkBox(QStringLiteral("NotifyDuplicateFinder"), i18nc("@option:check", "Finding duplicate files")));
    operationsLayout->addWidget(checkBox(QStringLiteral("NotifyFileSplit"), i18nc("@option:check", "Splitting and combining files")));
    operationsLayout->addWidget(checkBox(QStringLiteral("NotifyBatchRename"), i18nc("@option:check", "Renaming items in a batch")));
    operationsLayout->addWidget(checkBox(QStringLiteral("NotifyImageConversion"), i18nc("@option:check", "Converting images in a batch")));
    operationsLayout->addWidget(checkBox(QStringLiteral("NotifyLibraryRefresh"), i18nc("@option:check", "Refreshing tag libraries from the Tagging settings")));
    auto *operationsNote = new QLabel(i18nc("@info",
                                            "A change applies to operations started afterwards. Each operation's window still shows its progress "
                                            "and errors. Copying, moving and deleting files report to Plasma as in other KDE applications, so only "
                                            "Plasma's settings change them."));
    operationsNote->setWordWrap(true);
    operationsLayout->addWidget(operationsNote);
    layout->addWidget(operations);

    auto *plasma = new QGroupBox(i18nc("@title:group", "Plasma Notifications"));
    plasma->setObjectName(QStringLiteral("plasmaNotificationsGroup"));
    auto *plasmaLayout = new QVBoxLayout(plasma);
    auto *plasmaNote = new QLabel(i18nc("@info",
                                        "Plasma decides how reports appear, such as whether a finished operation shows a notification, how "
                                        "long notifications stay and when Do Not Disturb hides them. Change these in System Settings."));
    plasmaNote->setWordWrap(true);
    auto *configure = actionButton(QStringLiteral("openNotificationSettings"), i18nc("@action:button", "Configure Plasma Notifications…"));
    configure->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-notification-bell")));
    configure->setToolTip(i18nc("@info:tooltip", "Open Plasma's notification settings for Plasma Opus in System Settings"));
    auto *message = messageWidget(QStringLiteral("notificationsMessage"));
    plasmaLayout->addWidget(plasmaNote);
    plasmaLayout->addLayout(buttonRow({configure}));
    plasmaLayout->addWidget(message);
    layout->addWidget(plasma);
    layout->addStretch();

    connect(configure, &QPushButton::clicked, page, [message] {
        openSettingsModule(QStringLiteral("kcm_notifications"),
                           i18nc("@info", "Plasma's Notifications settings are not installed."),
                           message,
                           QStringLiteral("--desktop-entry=") + AppIdentity::desktopFileName());
    });
    return page;
}

DesktopTray *SettingsDialog::desktopTray() const
{
    return parentWidget() ? parentWidget()->findChild<DesktopTray *>() : nullptr;
}

QWidget *SettingsDialog::createShortcutsIntegrationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("shortcutsIntegrationPage"));
    auto *layout = new QVBoxLayout(page);

    auto *group = new StatusGroup(i18nc("@title:group", "Global Shortcut"));
    group->setObjectName(QStringLiteral("globalShortcutGroup"));
    auto *groupLayout = new QVBoxLayout(group);
    auto *enabled = checkBox(QStringLiteral("GlobalShortcut"), i18nc("@option:check", "Show or hide Plasma Opus with a global shortcut"));
    enabled->setToolTip(i18nc("@info:tooltip", "The shortcut brings the window to the front, or hides it when it is already in front"));
    groupLayout->addWidget(enabled);
    auto *form = new QFormLayout;
    m_globalShortcutKey = new KKeySequenceWidget;
    m_globalShortcutKey->setObjectName(QStringLiteral("globalShortcutKey"));
    m_globalShortcutKey->setCheckForConflictsAgainst(KKeySequenceWidget::GlobalShortcuts | KKeySequenceWidget::StandardShortcuts);
    form->addRow(i18nc("@label", "Key combination:"), m_globalShortcutKey);
    groupLayout->addLayout(form);
    auto *status = statusLabel(QStringLiteral("globalShortcutStatus"));
    groupLayout->addWidget(status);
    auto *note = new QLabel(i18nc("@info",
                                  "The shortcut works anywhere in Plasma, also while the window is hidden in the system tray. While it is "
                                  "on, Plasma's Shortcuts settings list it under Plasma Opus and can change it too; turning it off "
                                  "removes it there until it is turned on again."));
    note->setWordWrap(true);
    groupLayout->addWidget(note);
    auto *configure = actionButton(QStringLiteral("openShortcutSettings"), i18nc("@action:button", "Configure Plasma Shortcuts…"));
    configure->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-keyboard-shortcut")));
    configure->setToolTip(i18nc("@info:tooltip", "Open Plasma's Shortcuts settings in System Settings"));
    auto *message = messageWidget(QStringLiteral("shortcutsMessage"));
    groupLayout->addLayout(buttonRow({configure}));
    groupLayout->addWidget(message);
    layout->addWidget(group);
    layout->addStretch();

    const auto updateKey = [this, enabled] {
        m_globalShortcutKey->setEnabled(enabled->isChecked() && desktopTray());
    };
    connect(enabled, &QCheckBox::toggled, page, updateKey);
    updateKey();
    connect(m_globalShortcutKey, &KKeySequenceWidget::keySequenceChanged, this, &SettingsDialog::updateButtons);

    m_refreshGlobalShortcut = [this, status] {
        const DesktopTray *tray = desktopTray();
        if (!tray) {
            status->setText(i18nc("@info", "Open Preferences from a Plasma Opus window to change the global shortcut."));
            return;
        }
        const QKeySequence key = tray->globalShortcut();
        switch (TrayPolicy::shortcutStatus(tray->globalShortcutRegistered(), DesktopTray::globalShortcutServiceAvailable(), !key.isEmpty())) {
        case TrayPolicy::ShortcutStatus::Off:
            status->setText(i18nc("@info", "The global shortcut is off."));
            break;
        case TrayPolicy::ShortcutStatus::Unavailable:
            status->setText(i18nc("@info", "KDE's global shortcut service is not running, so the shortcut does not work in this session."));
            break;
        case TrayPolicy::ShortcutStatus::Unassigned:
            status->setText(i18nc("@info",
                                  "No key combination is assigned, so the shortcut does nothing. KDE leaves a key unassigned when "
                                  "another application already uses it; choose another one."));
            break;
        case TrayPolicy::ShortcutStatus::Assigned:
            status->setText(i18nc("@info %1 is a key combination",
                                  "Press %1 to show Plasma Opus, or to hide it when it is in front.",
                                  key.toString(QKeySequence::NativeText)));
            break;
        }
    };
    group->refresh = m_refreshGlobalShortcut;
    m_refreshGlobalShortcut();
    // Plasma's Shortcuts settings can change the key while the dialog is open.
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, page, [this](QAction *action) {
        if (const DesktopTray *tray = desktopTray(); tray && action == tray->toggleAction()) {
            m_refreshGlobalShortcut();
        }
    });

    connect(configure, &QPushButton::clicked, page, [message] {
        openSettingsModule(QStringLiteral("kcm_keys"), i18nc("@info", "Plasma's Shortcuts settings are not installed."), message);
    });
    return page;
}

ActivityController *SettingsDialog::activityController() const
{
    return parentWidget() ? parentWidget()->findChild<ActivityController *>() : nullptr;
}

QWidget *SettingsDialog::createActivitiesIntegrationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("workspaceActivitiesPage"));
    auto *layout = new QVBoxLayout(page);

    auto *associations = new QGroupBox(i18nc("@title:group", "Workspaces and Folders"));
    associations->setObjectName(QStringLiteral("activityAssociationsGroup"));
    auto *associationsLayout = new QVBoxLayout(associations);
    auto *enabled = checkBox(QStringLiteral("ActivityAssociations"), i18nc("@option:check", "Associate workspaces and default folders with Plasma Activities"));
    auto *offer = checkBox(QStringLiteral("OfferActivityAssociation"),
                           i18nc("@option:check", "Offer to apply an Activity's association when the current Activity changes"));
    associationsLayout->addWidget(enabled);
    associationsLayout->addWidget(offer);
    m_activityAssociations = new ActivityAssociationsEditor;
    if (const ActivityController *controller = activityController()) {
        m_activityAssociations->setSource(controller->source());
    }
    associationsLayout->addWidget(m_activityAssociations);
    auto *note = new QLabel(i18nc("@info",
                                  "Each Activity can name a workspace, the workspace behaviors to use when switching to it and a default "
                                  "folder for each pane. Associations are only offered, never applied without asking; View › Apply "
                                  "Activity Association applies the current one at any time. Plasma Opus never creates, removes or switches "
                                  "Activities, nor changes their settings."));
    note->setWordWrap(true);
    associationsLayout->addWidget(note);
    auto *configure = actionButton(QStringLiteral("openActivitySettings"), i18nc("@action:button", "Configure Plasma Activities…"));
    configure->setIcon(QIcon::fromTheme(QStringLiteral("activities")));
    configure->setToolTip(i18nc("@info:tooltip", "Open Plasma's Activities settings in System Settings to create, rename or remove Activities"));
    auto *message = messageWidget(QStringLiteral("activitiesMessage"));
    associationsLayout->addLayout(buttonRow({configure}));
    associationsLayout->addWidget(message);
    layout->addWidget(associations, 1);

    auto *usage = new QGroupBox(i18nc("@title:group", "Recent Files"));
    usage->setObjectName(QStringLiteral("activityUsageGroup"));
    auto *usageLayout = new QVBoxLayout(usage);
    usageLayout->addWidget(
        checkBox(QStringLiteral("ReportActivityUsage"), i18nc("@option:check", "Report visited folders and opened files to Plasma Activities")));
    auto *usageNote = new QLabel(i18nc("@info",
                                       "Plasma lists them in the current Activity's recent files and locations, where other applications, "
                                       "such as Plasma Impresario, can offer them too. Temporary files, encrypted vaults, searches and "
                                       "administrator locations are never reported. Choose what Plasma remembers, or clear it, in System "
                                       "Settings › Recent Files."));
    usageNote->setWordWrap(true);
    usageLayout->addWidget(usageNote);
    layout->addWidget(usage);

    const auto update = [this, enabled, offer] {
        offer->setEnabled(enabled->isChecked());
        m_activityAssociations->setEditable(enabled->isChecked());
    };
    connect(enabled, &QCheckBox::toggled, page, update);
    update();
    connect(m_activityAssociations, &ActivityAssociationsEditor::changed, this, &SettingsDialog::updateButtons);
    connect(configure, &QPushButton::clicked, page, [message] {
        openSettingsModule(QStringLiteral("kcm_activities"), i18nc("@info", "Plasma's Activities settings are not installed."), message);
    });
    return page;
}

QWidget *SettingsDialog::createKdeConnectIntegrationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("kdeConnectIntegrationPage"));
    auto *layout = new QVBoxLayout(page);

    auto *sharing = new QGroupBox(i18nc("@title:group", "Sharing"));
    sharing->setObjectName(QStringLiteral("kdeConnectSharingGroup"));
    auto *sharingLayout = new QVBoxLayout(sharing);
    auto *enabled = checkBox(QStringLiteral("KdeConnectSharing"), i18nc("@option:check", "Offer File › Send to Device… to send files with KDE Connect"));
    auto *remember = checkBox(QStringLiteral("KdeConnectRememberDevice"), i18nc("@option:check", "Choose the device the files were last sent to again"));
    sharingLayout->addWidget(enabled);
    sharingLayout->addWidget(remember);
    auto *sharingNote = new QLabel(i18nc("@info",
                                         "Send to Device sends the selected local files to a paired device through KDE Connect, only when Send "
                                         "is pressed. When the device is not remembered, the first device that can receive files is chosen, "
                                         "and the device used last is forgotten once the change is applied."));
    sharingNote->setWordWrap(true);
    sharingLayout->addWidget(sharingNote);
    layout->addWidget(sharing);

    auto *devices = new QGroupBox(i18nc("@title:group", "Devices"));
    devices->setObjectName(QStringLiteral("kdeConnectDevicesGroup"));
    auto *devicesLayout = new QVBoxLayout(devices);
    auto *devicesNote = new QLabel(i18nc("@info",
                                         "Plasma Opus never pairs devices or changes KDE Connect's settings. Pair devices, and turn file "
                                         "sharing on or off for each of them, in KDE Connect's settings."));
    devicesNote->setWordWrap(true);
    auto *configure = actionButton(QStringLiteral("openKdeConnectSettings"), i18nc("@action:button", "Configure KDE Connect…"));
    configure->setIcon(QIcon::fromTheme(QStringLiteral("kdeconnect")));
    configure->setToolTip(i18nc("@info:tooltip", "Open KDE Connect's settings in System Settings to pair devices and choose what they can do"));
    auto *message = messageWidget(QStringLiteral("kdeConnectSettingsMessage"));
    devicesLayout->addWidget(devicesNote);
    devicesLayout->addLayout(buttonRow({configure}));
    devicesLayout->addWidget(message);
    layout->addWidget(devices);
    layout->addStretch();

    connect(enabled, &QCheckBox::toggled, remember, &QWidget::setEnabled);
    remember->setEnabled(enabled->isChecked());
    connect(configure, &QPushButton::clicked, page, [message] {
        openSettingsModule(QStringLiteral("kcm_kdeconnect"), i18nc("@info", "KDE Connect's settings are not installed."), message);
    });
    return page;
}

QWidget *SettingsDialog::createKRunnerIntegrationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("plasmaSearchPage"));
    auto *layout = new QVBoxLayout(page);

    auto *discovery = new QGroupBox(i18nc("@title:group", "Discovery"));
    discovery->setObjectName(QStringLiteral("runnerDiscoveryGroup"));
    auto *discoveryLayout = new QVBoxLayout(discovery);
    auto *enabled = checkBox(QStringLiteral("RunnerEnabled"), i18nc("@option:check", "Offer saved workspaces and Favorites in KRunner and Plasma Search"));
    discoveryLayout->addWidget(enabled);
    auto *workspaces = checkBox(QStringLiteral("RunnerWorkspaces"), i18nc("@option:check", "Offer these saved workspaces:"));
    auto *favorites = checkBox(QStringLiteral("RunnerFavorites"), i18nc("@option:check", "Offer these Favorites:"));
    m_runnerWorkspaces = bound(new HiddenItemsList, QStringLiteral("RunnerHiddenWorkspaces"), "hiddenItems");
    m_runnerFavorites = bound(new HiddenItemsList, QStringLiteral("RunnerHiddenFavorites"), "hiddenItems");
    auto *lists = new QGridLayout;
    lists->addWidget(workspaces, 0, 0);
    lists->addWidget(favorites, 0, 1);
    lists->addWidget(m_runnerWorkspaces, 1, 0);
    lists->addWidget(m_runnerFavorites, 1, 1);
    discoveryLayout->addLayout(lists, 1);
    auto *note = new QLabel(i18nc("@info",
                                  "Only the checked workspaces and Favorites are offered, never history, searches or recent folders. "
                                  "Workspaces and Favorites added later are offered until they are cleared here. Choosing a workspace asks "
                                  "before switching, as any workspace switch does. KRunner uses a change from its next search on."));
    note->setWordWrap(true);
    discoveryLayout->addWidget(note);
    layout->addWidget(discovery, 1);

    auto *search = new QGroupBox(i18nc("@title:group", "Plasma Search"));
    search->setObjectName(QStringLiteral("runnerPlasmaSearchGroup"));
    auto *searchLayout = new QVBoxLayout(search);
    auto *searchNote = new QLabel(i18nc("@info",
                                        "KRunner keeps the Plasma Opus search plugin loaded while it offers nothing. Turn the plugin off "
                                        "entirely in Plasma's search settings; Plasma Opus never changes them."));
    searchNote->setWordWrap(true);
    auto *configure = actionButton(QStringLiteral("openPlasmaSearchSettings"), i18nc("@action:button", "Configure Plasma Search…"));
    configure->setIcon(QIcon::fromTheme(QStringLiteral("krunner")));
    configure->setToolTip(i18nc("@info:tooltip", "Open Plasma's search settings in System Settings to turn search plugins on or off"));
    auto *message = messageWidget(QStringLiteral("plasmaSearchMessage"));
    searchLayout->addWidget(searchNote);
    searchLayout->addLayout(buttonRow({configure}));
    searchLayout->addWidget(message);
    layout->addWidget(search);

    const auto update = [this, enabled, workspaces, favorites] {
        workspaces->setEnabled(enabled->isChecked());
        favorites->setEnabled(enabled->isChecked());
        m_runnerWorkspaces->setEnabled(enabled->isChecked() && workspaces->isChecked());
        m_runnerFavorites->setEnabled(enabled->isChecked() && favorites->isChecked());
    };
    for (QCheckBox *box : {enabled, workspaces, favorites}) {
        connect(box, &QCheckBox::toggled, page, update);
    }
    update();
    connect(configure, &QPushButton::clicked, page, [message] {
        openSettingsModule(QStringLiteral("kcm_plasmasearch"), i18nc("@info", "Plasma's search settings are not installed."), message);
    });

    // Follow the main window's Favorites while the dialog is open.
    if (const auto *model = parentWidget() ? parentWidget()->findChild<FavoritesModel *>() : nullptr) {
        connect(model, &QAbstractItemModel::rowsInserted, this, &SettingsDialog::reloadRunnerFavorites);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &SettingsDialog::reloadRunnerFavorites);
        connect(model, &QAbstractItemModel::rowsMoved, this, &SettingsDialog::reloadRunnerFavorites);
        connect(model, &QAbstractItemModel::dataChanged, this, &SettingsDialog::reloadRunnerFavorites);
        connect(model, &QAbstractItemModel::modelReset, this, &SettingsDialog::reloadRunnerFavorites);
    }
    reloadRunnerFavorites();
    return page;
}

void SettingsDialog::reloadRunnerFavorites()
{
    if (!m_runnerFavorites) {
        return;
    }
    const auto list = [this](const FavoritesModel &model) {
        QList<std::pair<QString, QString>> items;
        QStringList locations;
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row);
            const QUrl url = index.data(FavoritesModel::UrlRole).toUrl();
            items.append({RunnerMatches::locationId(url), index.data(Qt::DisplayRole).toString()});
            locations.append(url.toDisplayString(QUrl::PreferLocalFile | QUrl::RemoveUserInfo));
        }
        m_runnerFavorites->setItems(items);
        for (int row = 0; row < m_runnerFavorites->count(); ++row) {
            m_runnerFavorites->item(row)->setToolTip(locations.at(row));
        }
    };
    if (const auto *model = parentWidget() ? parentWidget()->findChild<FavoritesModel *>() : nullptr) {
        list(*model);
        return;
    }
    const auto shared = KSharedConfig::openConfig();
    KConfig config(shared->name(), shared->openFlags(), shared->locationType());
    list(FavoritesModel(config.group(QStringLiteral("Favorites"))));
}

QWidget *SettingsDialog::createOnlineAccountsIntegrationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("onlineAccountsIntegrationPage"));
    auto *layout = new QVBoxLayout(page);

    auto *access = new StatusGroup(i18nc("@title:group", "Access"));
    access->setObjectName(QStringLiteral("onlineAccountsAccessGroup"));
    auto *accessLayout = new QVBoxLayout(access);
    auto *enabled = checkBox(QStringLiteral("OnlineAccountsEnabled"), i18nc("@option:check", "Show the folders of KDE Online Accounts in Plasma Opus"));
    accessLayout->addWidget(enabled);
    auto *listLabel = new QLabel(i18nc("@label", "Show these accounts:"));
    accessLayout->addWidget(listLabel);
    auto *accounts = bound(new HiddenItemsList, QStringLiteral("OnlineAccountsHidden"), "hiddenItems");
    accessLayout->addWidget(accounts, 1);
    auto *status = statusLabel(QStringLiteral("onlineAccountsStatus"));
    accessLayout->addWidget(status);
    auto *refresh = actionButton(QStringLiteral("refreshOnlineAccounts"), i18nc("@action:button", "Refresh List"));
    refresh->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    refresh->setToolTip(i18nc("@info:tooltip", "Read the accounts from KDE Online Accounts again"));
    accessLayout->addLayout(buttonRow({refresh}));
    auto *note = new QLabel(i18nc("@info",
                                  "Plasma Opus reads the account list only while this is checked, and never reads, logs or stores "
                                  "passwords or access tokens. Go › Online Accounts and the cloud sync accounts menu offer the checked "
                                  "accounts with file storage. Clearing an account only hides it in Plasma Opus; it stays in KDE Online "
                                  "Accounts for other applications. Accounts added later are shown until they are cleared here."));
    note->setWordWrap(true);
    accessLayout->addWidget(note);
    layout->addWidget(access, 1);

    auto *manage = new QGroupBox(i18nc("@title:group", "KDE Online Accounts"));
    manage->setObjectName(QStringLiteral("onlineAccountsManageGroup"));
    auto *manageLayout = new QVBoxLayout(manage);
    auto *manageNote = new QLabel(i18nc("@info",
                                        "Plasma Opus never adds, removes or signs in to accounts. Do that, and choose the services each "
                                        "account offers, in KDE's Online Accounts settings."));
    manageNote->setWordWrap(true);
    auto *configure = actionButton(QStringLiteral("openOnlineAccountsSettings"), i18nc("@action:button", "Manage Online Accounts…"));
    configure->setIcon(QIcon::fromTheme(QStringLiteral("internet-services")));
    configure->setToolTip(i18nc("@info:tooltip", "Open KDE's Online Accounts settings in System Settings to add, remove or sign in to accounts"));
    auto *message = messageWidget(QStringLiteral("onlineAccountsMessage"));
    manageLayout->addWidget(manageNote);
    manageLayout->addLayout(buttonRow({configure}));
    manageLayout->addWidget(message);
    layout->addWidget(manage);

    // The accounts are read only after opting in, as in the main window.
    const auto reload = [enabled, accounts, status] {
        if (!enabled->isChecked()) {
            status->setText(i18nc("@info", "Plasma Opus does not read KDE Online Accounts."));
            return;
        }
        QString error;
        const QList<OnlineAccounts::Account> all = OnlineAccounts::readAccounts(&error);
        if (!error.isEmpty()) {
            status->setText(error);
            return;
        }
        const QList<OnlineAccounts::Account> storage = OnlineAccounts::shownAccounts(all, {});
        QList<std::pair<QString, QString>> items;
        for (const OnlineAccounts::Account &account : storage) {
            items.append({OnlineAccounts::settingsId(account),
                          account.providerDisplayName.isEmpty()
                              ? account.displayName
                              : i18nc("@item %1 is an account name, %2 its provider", "%1 (%2)", account.displayName, account.providerDisplayName)});
        }
        accounts->setItems(items);
        const OnlineAccounts::ProtocolCheck known = [](const QString &protocol) {
            return KProtocolInfo::isKnownProtocol(protocol);
        };
        for (int row = 0; row < accounts->count(); ++row) {
            const OnlineAccounts::Account &account = storage.at(row);
            accounts->item(row)->setIcon(QIcon::fromTheme(account.iconName, QIcon::fromTheme(QStringLiteral("internet-services"))));
            QStringList folders;
            for (const OnlineAccounts::Location &location : OnlineAccounts::locations(account, known)) {
                folders.append(location.isAvailable() ? location.title
                                                      : i18nc("@info:tooltip %1 is a folder, %2 why it cannot be opened",
                                                              "%1 (unavailable: %2)",
                                                              location.title,
                                                              location.unavailableReason));
            }
            accounts->item(row)->setToolTip(folders.join(QLatin1Char('\n')));
        }
        QStringList others;
        for (const OnlineAccounts::Account &account : all) {
            if (!OnlineAccounts::hasFileStorage(account)) {
                others.append(account.displayName);
            }
        }
        QString text = storage.isEmpty() ? i18nc("@info", "No account offers file storage. Add one in KDE's Online Accounts settings.")
                                         : i18ncp("@info", "One account offers file storage.", "%1 accounts offer file storage.", storage.size());
        if (!others.isEmpty()) {
            text += QLatin1Char(' ')
                + i18nc("@info %1 is a list of account names", "Accounts without file storage are not listed: %1.", QLocale().createSeparatedList(others));
        }
        status->setText(text);
    };
    const auto update = [enabled, accounts, listLabel, refresh] {
        listLabel->setEnabled(enabled->isChecked());
        accounts->setEnabled(enabled->isChecked());
        refresh->setEnabled(enabled->isChecked());
    };
    access->refresh = reload;
    connect(enabled, &QCheckBox::toggled, page, [update, reload] {
        update();
        reload();
    });
    update();
    connect(refresh, &QPushButton::clicked, page, reload);
    connect(configure, &QPushButton::clicked, page, [message] {
        openSettingsModule(QStringLiteral("kcm_kaccounts"), i18nc("@info", "KDE's Online Accounts settings are not installed."), message);
    });
    return page;
}

QWidget *SettingsDialog::createMountManagerIntegrationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("mountManagerIntegrationPage"));
    auto *layout = new QVBoxLayout(page);

    auto *display = new QGroupBox(i18nc("@title:group", "Display"));
    display->setObjectName(QStringLiteral("mountManagerDisplayGroup"));
    auto *displayLayout = new QVBoxLayout(display);
    auto *removable = checkBox(QStringLiteral("MountRemovableOnly"), i18nc("@option:check", "Show only removable devices"));
    removable->setToolTip(i18nc("@info:tooltip",
                                "Lists only removable devices in Go › Mount Points, and checks Show only removable devices "
                                "each time the Mount Manager opens"));
    auto *system = checkBox(QStringLiteral("MountHideSystem"), i18nc("@option:check", "Hide pseudo, snap and other system file systems"));
    system->setToolTip(i18nc("@info:tooltip",
                             "Leaves out kernel and memory file systems, automounts, snaps, containers and the document portal. "
                             "When shown, they are listed but never mounted or unmounted from Plasma Opus."));
    displayLayout->addWidget(removable);
    displayLayout->addWidget(system);

    auto *mainWindow = qobject_cast<KXmlGuiWindow *>(parentWidget());
    const QPointer<KXmlGuiWindow> window(mainWindow);
    const QPointer<QAction> menuAction = mainWindow ? mainWindow->actionCollection()->action(QStringLiteral("mount_points")) : nullptr;
    QCheckBox *toolbar = addSharedToggle(
        QStringLiteral("mountPointsToolbarButton"),
        i18nc("@option:check", "Show the Mount Points menu on the main toolbar"),
        [window, menuAction]() -> bool {
            return window && menuAction && window->toolBar()->actions().contains(menuAction.data());
        },
        [window, menuAction](bool shown) {
            if (!window || !menuAction) {
                return;
            }
            QToolBar *bar = window->toolBar();
            const QList<QAction *> entries = bar->actions();
            if (entries.contains(menuAction.data()) == shown) {
                return;
            }
            if (!shown) {
                bar->removeAction(menuAction);
                return;
            }
            // Place the menu where it is by default, after the Preview button, or before Search or the hamburger menu.
            QAction *before = nullptr;
            bool found = false;
            if (QAction *preview = window->actionCollection()->action(QStringLiteral("show_preview_panel")); preview && entries.contains(preview)) {
                const qsizetype next = entries.indexOf(preview) + 1;
                before = next < entries.size() ? entries.at(next) : nullptr;
                found = true;
            }
            for (const QString &name : {QStringLiteral("search_folder"), QStringLiteral("hamburger_menu")}) {
                QAction *candidate = window->actionCollection()->action(name);
                if (!found && candidate && entries.contains(candidate)) {
                    before = candidate;
                    found = true;
                }
            }
            bar->insertAction(before, menuAction);
        },
        true);
    toolbar->setEnabled(!menuAction.isNull());
    toolbar->setToolTip(i18nc("@info:tooltip", "Adds or removes the Mount Points entry, like the main toolbar editor on the Toolbars page"));
    displayLayout->addWidget(toolbar);
    auto *note = new QLabel(i18nc("@info",
                                  "File › Mount Manager… can still show every device while it is open. "
                                  "The root folder and system file systems are never unmounted from Plasma Opus."));
    note->setWordWrap(true);
    displayLayout->addWidget(note);
    layout->addWidget(display);
    layout->addStretch();
    return page;
}

KWinIntegration *SettingsDialog::kwinIntegration() const
{
    return parentWidget() ? parentWidget()->findChild<KWinIntegration *>() : nullptr;
}

void SettingsDialog::setKWinRemovalConfirmation(std::function<bool()> confirm)
{
    m_confirmKWinRemoval = std::move(confirm);
}

QWidget *SettingsDialog::createKWinIntegrationPage()
{
    if (!m_confirmKWinRemoval) {
        m_confirmKWinRemoval = [this] {
            return KMessageBox::questionTwoActions(this,
                                                   i18nc("@info", "Remove the Plasma Opus KWin script? Other KWin scripts and settings are not changed."),
                                                   i18nc("@title:window", "Remove KWin Script"),
                                                   KStandardGuiItem::remove(),
                                                   KStandardGuiItem::cancel())
                == KMessageBox::PrimaryAction;
        };
    }
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("kwinIntegrationPage"));
    auto *layout = new QVBoxLayout(page);

    auto *script = new StatusGroup(i18nc("@title:group", "KWin Script"));
    script->setObjectName(QStringLiteral("kwinScriptGroup"));
    auto *scriptLayout = new QVBoxLayout(script);
    auto *intro = new QLabel(i18nc("@info",
                                   "An optional KWin script lets Plasma Opus restore, tile and place its windows, which Wayland applications cannot do "
                                   "themselves. Nothing is installed until you choose to, and the script acts only on Plasma Opus windows."));
    intro->setWordWrap(true);
    scriptLayout->addWidget(intro);
    auto *status = statusLabel(QStringLiteral("kwinStatus"));
    scriptLayout->addWidget(status);
    auto *install = actionButton(QStringLiteral("kwinInstall"), i18nc("@action:button", "Install and Enable"));
    auto *enable = actionButton(QStringLiteral("kwinEnable"), i18nc("@action:button", "Enable"));
    enable->setIcon(QIcon::fromTheme(QStringLiteral("checkmark")));
    enable->setToolTip(i18nc("@info:tooltip", "Turn the installed script on in KWin and load it"));
    auto *disable = actionButton(QStringLiteral("kwinDisable"), i18nc("@action:button", "Disable"));
    disable->setIcon(QIcon::fromTheme(QStringLiteral("dialog-cancel")));
    disable->setToolTip(i18nc("@info:tooltip", "Turn the script off in KWin and unload it, keeping it installed"));
    auto *remove = actionButton(QStringLiteral("kwinRemove"), i18nc("@action:button", "Remove…"));
    remove->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    remove->setToolTip(i18nc("@info:tooltip", "Unload and uninstall the script; other KWin scripts and settings are not changed"));
    auto *refresh = actionButton(QStringLiteral("kwinRefresh"), i18nc("@action:button", "Refresh"));
    refresh->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    refresh->setToolTip(i18nc("@info:tooltip", "Read the script's state from KWin again"));
    scriptLayout->addLayout(buttonRow({install, enable, disable, remove, refresh}));
    auto *scripts = actionButton(QStringLiteral("openKWinScriptsSettings"), i18nc("@action:button", "Configure KWin Scripts…"));
    scripts->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-behavior")));
    scripts->setToolTip(i18nc("@info:tooltip", "Open KWin's script settings in System Settings, which list every installed KWin script"));
    scriptLayout->addLayout(buttonRow({scripts}));
    auto *message = messageWidget(QStringLiteral("kwinMessage"));
    scriptLayout->addWidget(message);
    layout->addWidget(script);

    auto *windows = new QGroupBox(i18nc("@title:group", "Window Management"));
    windows->setObjectName(QStringLiteral("kwinWindowManagementGroup"));
    auto *windowsLayout = new QVBoxLayout(windows);
    windowsLayout->addWidget(
        checkBox(QStringLiteral("KWinRestoreWindows"),
                 i18nc("@option:check", "Restore the window's screen, virtual desktop, size, tile and keep-above state with each workspace")));
    windowsLayout->addWidget(
        checkBox(QStringLiteral("KWinTileCompanions"), i18nc("@option:check", "Place detached previews, editors and panes beside the main window")));
    auto *follow =
        checkBox(QStringLiteral("KWinFollowDesktops"), i18nc("@option:check", "Apply the workspace assigned to the virtual desktop that becomes current:"));
    windowsLayout->addWidget(follow);
    m_desktopWorkspaces = new DesktopWorkspacesEditor;
    m_desktopWorkspaces->setObjectName(QStringLiteral("kwinDesktopWorkspaces"));
    windowsLayout->addWidget(m_desktopWorkspaces);
    auto *windowsNote = new QLabel(i18nc("@info",
                                         "These take effect while the script is running. A workspace restores the window as it was last "
                                         "placed while that workspace was in use. Plasma Opus never changes KWin's settings; window rules for "
                                         "Plasma Opus and other applications are set in KWin's settings."));
    windowsNote->setWordWrap(true);
    windowsLayout->addWidget(windowsNote);
    auto *rules = actionButton(QStringLiteral("openKWinRulesSettings"), i18nc("@action:button", "Configure Window Rules…"));
    rules->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows-actions")));
    rules->setToolTip(i18nc("@info:tooltip", "Open KWin's window rules in System Settings, which apply to every application"));
    auto *rulesMessage = messageWidget(QStringLiteral("kwinRulesMessage"));
    windowsLayout->addLayout(buttonRow({rules}));
    windowsLayout->addWidget(rulesMessage);
    layout->addWidget(windows);
    layout->addStretch();

    const QPointer<KWinIntegration> kwin(kwinIntegration());
    const auto update = [kwin, status, install, enable, disable, remove, refresh] {
        if (!kwin) {
            status->setText(i18nc("@info", "Open Preferences from a Plasma Opus window to manage the KWin script."));
            for (QPushButton *button : {install, enable, disable, remove, refresh}) {
                button->setEnabled(false);
            }
            enable->hide();
            disable->hide();
            return;
        }
        const KWinScript::State state = kwin->state();
        const KWinScript::Status current = KWinScript::status(state);
        status->setText(kwin->statusText());
        const bool available = current != KWinScript::Status::KWinUnavailable && current != KWinScript::Status::ScriptingUnavailable;
        const bool installed = state.installedVersion >= 0;
        install->setVisible(!installed || state.installedVersion < state.bundledVersion);
        install->setText(installed ? i18nc("@action:button", "Update") : i18nc("@action:button", "Install and Enable"));
        install->setIcon(QIcon::fromTheme(installed ? QStringLiteral("update-none") : QStringLiteral("download")));
        install->setToolTip(installed ? i18nc("@info:tooltip", "Install the version included with this Plasma Opus, enable it and reload it")
                                      : i18nc("@info:tooltip", "Install the script for your user account, enable it and load it"));
        install->setEnabled(available);
        enable->setVisible(installed && !state.enabled);
        enable->setEnabled(available);
        disable->setVisible(installed && state.enabled);
        disable->setEnabled(true);
        remove->setEnabled(installed);
        refresh->setEnabled(true);
    };
    script->refresh = update;
    update();

    const auto reloadDesktops = [this, kwin] {
        QList<std::pair<QString, QString>> desktops;
        if (kwin) {
            for (const KWinScript::Desktop &desktop : kwin->desktops()) {
                desktops.append({desktop.id, desktop.name});
            }
        }
        m_desktopWorkspaces->setDesktops(desktops);
    };
    reloadDesktops();
    connect(follow, &QCheckBox::toggled, m_desktopWorkspaces, &QWidget::setEnabled);
    m_desktopWorkspaces->setEnabled(follow->isChecked());
    connect(m_desktopWorkspaces, &DesktopWorkspacesEditor::changed, this, &SettingsDialog::updateButtons);

    // The message to show once an install, update or removal succeeds.
    auto succeeded = std::make_shared<QString>();
    if (kwin) {
        connect(kwin, &KWinIntegration::stateChanged, page, update);
        connect(kwin, &KWinIntegration::desktopsChanged, page, reloadDesktops);
        connect(kwin, &KWinIntegration::operationFinished, page, [message, succeeded](const QString &error) {
            if (!error.isEmpty()) {
                report(message, KMessageWidget::Error, error);
            } else if (!succeeded->isEmpty()) {
                report(message, KMessageWidget::Positive, *succeeded);
            }
            succeeded->clear();
        });
    }
    connect(install, &QPushButton::clicked, page, [kwin, install, remove, message, succeeded] {
        if (!kwin) {
            return;
        }
        message->animatedHide();
        *succeeded = kwin->state().installedVersion >= 0 ? i18nc("@info", "The KWin script is updated, enabled and reloaded.")
                                                         : i18nc("@info", "The KWin script is installed and enabled.");
        install->setEnabled(false);
        remove->setEnabled(false);
        kwin->install();
    });
    connect(enable, &QPushButton::clicked, page, [kwin, message] {
        if (kwin) {
            message->animatedHide();
            kwin->setEnabled(true);
        }
    });
    connect(disable, &QPushButton::clicked, page, [kwin, message] {
        if (kwin) {
            message->animatedHide();
            kwin->setEnabled(false);
        }
    });
    connect(remove, &QPushButton::clicked, page, [this, kwin, install, remove, message, succeeded] {
        if (!kwin || !m_confirmKWinRemoval()) {
            return;
        }
        message->animatedHide();
        *succeeded = i18nc("@info", "The KWin script is removed. Other KWin scripts and settings are not changed.");
        install->setEnabled(false);
        remove->setEnabled(false);
        kwin->remove();
    });
    connect(refresh, &QPushButton::clicked, page, [kwin] {
        if (kwin) {
            kwin->refresh();
        }
    });
    connect(scripts, &QPushButton::clicked, page, [message] {
        openSettingsModule(QStringLiteral("kcm_kwin_scripts"), i18nc("@info", "KWin's script settings are not installed."), message);
    });
    connect(rules, &QPushButton::clicked, page, [rulesMessage] {
        openSettingsModule(QStringLiteral("kcm_kwinrules"), i18nc("@info", "KWin's window rules settings are not installed."), rulesMessage);
    });
    return page;
}
