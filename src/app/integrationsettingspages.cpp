// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "appidentity.h"
#include "defaultfilemanager.h"
#include "filechooser.h"
#include "filemanagerservice.h"
#include "portalsetup.h"
#include "settingswidgets.h"

#include <KConfigGroup>
#include <KIO/CommandLauncherJob>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <KPluginMetaData>
#include <KService>
#include <KSharedConfig>

#include <QApplication>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
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
void openSettingsModule(const QString &module, const QString &missing, KMessageWidget *message)
{
    if (!KPluginMetaData::findPluginById(QStringLiteral("plasma/kcms/systemsettings"), module).isValid()) {
        report(message, KMessageWidget::Error, missing);
        return;
    }
    const QString kcmshell = QStandardPaths::findExecutable(QStringLiteral("kcmshell6"));
    auto *job = kcmshell.isEmpty() ? new KIO::CommandLauncherJob(QStringLiteral("systemsettings"), {module}) : new KIO::CommandLauncherJob(kcmshell, {module});
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
