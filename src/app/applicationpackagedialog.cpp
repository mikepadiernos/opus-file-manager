// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationpackagedialog.h"
#include "packagetools.h"

#include <KGuiItem>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVBoxLayout>

using AppPackages::Action;
using AppPackages::Kind;

namespace
{
QString iconName(Action action)
{
    switch (action) {
    case Action::Run:
        return QStringLiteral("system-run");
    case Action::Install:
        return QStringLiteral("download");
    case Action::Update:
        return QStringLiteral("update-none");
    case Action::Uninstall:
        return QStringLiteral("edit-delete");
    case Action::Integrate:
        return QStringLiteral("list-add");
    case Action::RemoveIntegration:
        return QStringLiteral("list-remove");
    case Action::OpenInDiscover:
        return QStringLiteral("plasmadiscover");
    }
    return {};
}

QString buttonName(Action action)
{
    switch (action) {
    case Action::Run:
        return QStringLiteral("packageRun");
    case Action::Install:
        return QStringLiteral("packageInstall");
    case Action::Update:
        return QStringLiteral("packageUpdate");
    case Action::Uninstall:
        return QStringLiteral("packageUninstall");
    case Action::Integrate:
        return QStringLiteral("packageIntegrate");
    case Action::RemoveIntegration:
        return QStringLiteral("packageRemoveIntegration");
    case Action::OpenInDiscover:
        return QStringLiteral("packageOpenInDiscover");
    }
    return {};
}

bool isFlatpak(Kind kind)
{
    return kind == Kind::FlatpakBundle || kind == Kind::FlatpakRef || kind == Kind::FlatpakRepo || kind == Kind::FlatpakApp;
}

/// The last lines of a tool's output, for an error message.
QString lastLines(const QString &output)
{
    QStringList lines = output.trimmed().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() > 3) {
        lines = lines.mid(lines.size() - 3);
    }
    return lines.join(QLatin1Char('\n'));
}
}

ApplicationPackageDialog::ApplicationPackageDialog(const QString &path,
                                                   const QString &mimeType,
                                                   PackageTools *tools,
                                                   const AppPackages::Locations &locations,
                                                   QWidget *parent)
    : QDialog(parent)
    , m_path(path)
    , m_mimeType(mimeType)
    , m_tools(tools)
    , m_locations(locations)
    , m_message(new KMessageWidget(this))
    , m_icon(new QLabel(this))
    , m_title(new QLabel(this))
    , m_form(new QFormLayout)
    , m_systemWide(new QCheckBox(i18nc("@option:check", "Install for all users"), this))
    , m_details(new QPlainTextEdit(this))
    , m_progress(new QProgressBar(this))
    , m_actions(new QWidget(this))
{
    setWindowTitle(i18nc("@title:window", "Application Package"));
    resize(560, 480);

    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(true);
    m_message->hide();

    auto *header = new QHBoxLayout;
    m_icon->setPixmap(QIcon::fromTheme(QStringLiteral("package-x-generic")).pixmap(48, 48));
    header->addWidget(m_icon);
    m_title->setTextFormat(Qt::RichText);
    m_title->setWordWrap(true);
    header->addWidget(m_title, 1);

    m_systemWide->setObjectName(QStringLiteral("packageSystemWide"));
    m_systemWide->setToolTip(i18nc("@info:tooltip", "Install into the system installation, which asks for authorization, instead of your own"));

    m_details->setObjectName(QStringLiteral("packageDetails"));
    m_details->setReadOnly(true);
    m_details->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_details->setPlaceholderText(i18nc("@info:placeholder", "Package tool output appears here."));

    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->hide();

    auto *actionLayout = new QHBoxLayout(m_actions);
    actionLayout->setContentsMargins({});

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(m_message);
    layout->addLayout(m_form);
    layout->addWidget(m_systemWide);
    layout->addWidget(m_details, 1);
    layout->addWidget(m_progress);
    layout->addWidget(m_actions);
    layout->addWidget(buttons);

    reload();
    loadDetails(0);
}

ApplicationPackageDialog::~ApplicationPackageDialog() = default;

const AppPackages::Package &ApplicationPackageDialog::package() const
{
    return m_package;
}

bool ApplicationPackageDialog::isBusy() const
{
    return m_busy;
}

void ApplicationPackageDialog::setConfirmation(Confirmation confirmation)
{
    m_confirmation = std::move(confirmation);
}

void ApplicationPackageDialog::reload()
{
    m_package = AppPackages::identify(m_path, m_mimeType, m_locations);
    const QString kind = AppPackages::kindName(m_package.kind);
    m_title->setText(QStringLiteral("<b>%1</b><br/>%2").arg(m_package.name.toHtmlEscaped(), kind.toHtmlEscaped()));

    while (m_form->rowCount() > 0) {
        m_form->removeRow(0);
    }
    auto addRow = [this](const QString &label, const QString &value) {
        if (value.isEmpty()) {
            return;
        }
        auto *field = new QLabel(value, this);
        field->setTextInteractionFlags(Qt::TextSelectableByMouse);
        field->setWordWrap(true);
        m_form->addRow(label, field);
    };
    addRow(i18nc("@label", "Location:"), m_path);
    addRow(i18nc("@label", "ID:"), m_package.id != m_package.name ? m_package.id : QString());
    addRow(i18nc("@label", "Branch:"), m_package.branch);
    addRow(i18nc("@label", "Source:"), m_package.source);
    addRow(i18nc("@label", "Description:"), m_package.comment);

    switch (m_package.kind) {
    case Kind::AppImage:
        addRow(i18nc("@label", "Format:"),
               m_package.appImageType > 0 ? i18nc("@info AppImage format", "Type %1", m_package.appImageType)
                                          : i18nc("@info AppImage format", "Unknown, without an AppImage header"));
        addRow(i18nc("@label", "Executable:"),
               QFileInfo(m_path).isExecutable() ? i18nc("@info", "Yes") : i18nc("@info", "No; Plasma Opus asks before marking it executable"));
        addRow(i18nc("@label", "Application menu:"), m_package.integrated ? i18nc("@info", "Added") : i18nc("@info", "Not added"));
        break;
    case Kind::FlatpakRef:
    case Kind::FlatpakApp:
    case Kind::SnapFile:
    case Kind::SnapApp:
        if (m_package.installed) {
            addRow(i18nc("@label", "Status:"),
                   m_package.kind == Kind::SnapFile || m_package.kind == Kind::SnapApp
                       ? i18nc("@info", "Installed")
                       : (m_package.systemWide ? i18nc("@info", "Installed for all users") : i18nc("@info", "Installed for you")));
        } else {
            addRow(i18nc("@label", "Status:"), i18nc("@info", "Not installed"));
        }
        break;
    default:
        break;
    }

    const QList<Action> actions = AppPackages::actions(m_package);
    m_systemWide->setVisible(isFlatpak(m_package.kind) && actions.contains(Action::Install));
    rebuildActions();
}

void ApplicationPackageDialog::rebuildActions()
{
    // A button can be rebuilt from its own clicked() handler, so delete later.
    while (QLayoutItem *item = m_actions->layout()->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    m_buttons.clear();
    for (const Action action : AppPackages::actions(m_package)) {
        auto *button = new QPushButton(QIcon::fromTheme(iconName(action)), actionText(action), m_actions);
        button->setObjectName(buttonName(action));
        const QString tool = AppPackages::tool(m_package, action);
        if (!tool.isEmpty() && (!m_tools || !m_tools->hasProgram(tool))) {
            button->setEnabled(false);
            button->setToolTip(missingToolText(tool));
        }
        connect(button, &QPushButton::clicked, this, [this, action] {
            trigger(action);
        });
        m_actions->layout()->addWidget(button);
        m_buttons.insert(action, button);
    }
    static_cast<QHBoxLayout *>(m_actions->layout())->addStretch();
    setBusy(m_busy);
}

QString ApplicationPackageDialog::actionText(Action action) const
{
    switch (action) {
    case Action::Run:
        return i18nc("@action:button", "Run");
    case Action::Install:
        return m_package.kind == Kind::FlatpakRepo ? i18nc("@action:button", "Add Repository") : i18nc("@action:button", "Install");
    case Action::Update:
        return i18nc("@action:button", "Update");
    case Action::Uninstall:
        return i18nc("@action:button", "Uninstall");
    case Action::Integrate:
        return i18nc("@action:button", "Add to Application Menu");
    case Action::RemoveIntegration:
        return i18nc("@action:button", "Remove from Application Menu");
    case Action::OpenInDiscover:
        return i18nc("@action:button", "Open in Discover");
    }
    return {};
}

QString ApplicationPackageDialog::missingToolText(const QString &tool) const
{
    if (tool == QLatin1String("plasma-discover")) {
        return i18nc("@info", "Discover is not installed. Install the plasma-discover package from your distribution.");
    }
    if (tool == QLatin1String("appimageupdatetool")) {
        return i18nc("@info", "Updating AppImages needs AppImageUpdate's appimageupdatetool, which is not installed.");
    }
    return i18nc("@info %1 is a program name such as flatpak or snap", "This needs %1, which is not installed. Install it from your distribution.", tool);
}

void ApplicationPackageDialog::loadDetails(int index)
{
    const QList<AppPackages::Command> commands = AppPackages::detailsCommands(m_package);
    if (index >= commands.size() || !m_tools || !m_tools->hasProgram(commands.at(index).program)) {
        return;
    }
    const AppPackages::Command command = commands.at(index);
    m_tools->run(command, {}, this, [this, command, index](const PackageTools::Result &result) {
        appendOutput(command, result.output);
        loadDetails(index + 1);
    });
}

void ApplicationPackageDialog::appendOutput(const AppPackages::Command &command, const QString &output)
{
    QStringList line{command.program};
    line += command.arguments;
    m_details->appendPlainText(QStringLiteral("$ ") + line.join(QLatin1Char(' ')));
    if (!output.trimmed().isEmpty()) {
        m_details->appendPlainText(output.trimmed());
    }
    m_details->appendPlainText(QString());
}

void ApplicationPackageDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_progress->setVisible(busy);
    m_systemWide->setEnabled(!busy);
    for (QPushButton *button : std::as_const(m_buttons)) {
        const QString tool = AppPackages::tool(m_package, m_buttons.key(button));
        const bool available = tool.isEmpty() || (m_tools && m_tools->hasProgram(tool));
        button->setEnabled(!busy && available);
    }
}

bool ApplicationPackageDialog::confirm(Action action)
{
    const QString name = m_package.name;
    QString title;
    QString text;
    switch (action) {
    case Action::Install:
        if (m_package.kind == Kind::FlatpakRepo) {
            title = i18nc("@title:window", "Add Flatpak Repository");
            text = i18nc("@info", "Add the Flatpak repository %1 from %2? Applications from it can then be installed and updated.", name, m_package.source);
        } else if (isFlatpak(m_package.kind)) {
            title = i18nc("@title:window", "Install Flatpak");
            text = m_systemWide->isChecked() ? i18nc("@info", "Install %1 with Flatpak for all users? This asks for authorization.", name)
                                             : i18nc("@info", "Install %1 with Flatpak for your account?", name);
            text += QLatin1Char(' ')
                + i18nc("@info", "The application receives the sandbox permissions it requests; review them under Details after installing, or in Discover.");
        } else {
            title = i18nc("@title:window", "Install Snap");
            text = i18nc("@info",
                         "Install %1 from this file? It is not signed by the Snap Store, so snapd installs it without a signature check and does not update it "
                         "automatically. This may ask for authorization.",
                         name);
        }
        break;
    case Action::Update:
        title = i18nc("@title:window", "Update Application");
        text = m_package.kind == Kind::AppImage ? i18nc("@info", "Update %1 with AppImageUpdate, from the update information the AppImage contains?", name)
                                                : i18nc("@info", "Update %1? This may ask for authorization.", name);
        break;
    case Action::Uninstall:
        title = i18nc("@title:window", "Uninstall Application");
        text = isFlatpak(m_package.kind)
            ? i18nc("@info", "Uninstall %1? Its data in your home folder is kept.", name)
            : i18nc("@info", "Uninstall %1? snapd keeps a snapshot of its data for a limited time. This may ask for authorization.", name);
        break;
    case Action::Integrate:
        title = i18nc("@title:window", "Add to Application Menu");
        text = m_package.appImageType == 2
            ? i18nc("@info", "Add %1 to the application menu? Plasma Opus runs the AppImage to read its name and icon.", name)
            : i18nc("@info",
                    "Add %1 to the application menu? Only type 2 AppImages provide a name and icon, so the file name and a generic icon are used.",
                    name);
        break;
    case Action::RemoveIntegration:
        title = i18nc("@title:window", "Remove from Application Menu");
        text = i18nc("@info", "Remove %1 from the application menu? The AppImage itself is kept.", name);
        break;
    case Action::Run:
    case Action::OpenInDiscover:
        return true;
    }
    if (m_confirmation) {
        return m_confirmation(title, text, actionText(action));
    }
    return KMessageBox::warningContinueCancel(this, text, title, KGuiItem(actionText(action), iconName(action))) == KMessageBox::Continue;
}

bool ApplicationPackageDialog::ensureExecutable()
{
    QFileInfo info(m_path);
    if (info.isExecutable()) {
        return true;
    }
    const QString title = i18nc("@title:window", "Mark as Executable");
    const QString text =
        i18nc("@info", "%1 is not marked as executable. Mark it executable and continue? Do this only for AppImages you trust.", info.fileName());
    const QString action = i18nc("@action:button", "Mark Executable");
    const bool accepted = m_confirmation
        ? m_confirmation(title, text, action)
        : KMessageBox::warningContinueCancel(this, text, title, KGuiItem(action, QStringLiteral("system-run"))) == KMessageBox::Continue;
    if (!accepted) {
        return false;
    }
    QFile file(m_path);
    if (!file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser)) {
        m_message->setMessageType(KMessageWidget::Error);
        m_message->setText(i18nc("@info", "Could not mark %1 as executable: %2", info.fileName(), file.errorString()));
        m_message->animatedShow();
        return false;
    }
    reload();
    return true;
}

void ApplicationPackageDialog::trigger(Action action)
{
    if (m_busy || !AppPackages::actions(m_package).contains(action)) {
        return;
    }
    const QString tool = AppPackages::tool(m_package, action);
    if (!tool.isEmpty() && (!m_tools || !m_tools->hasProgram(tool))) {
        m_message->setMessageType(KMessageWidget::Warning);
        m_message->setText(missingToolText(tool));
        m_message->animatedShow();
        return;
    }
    if (m_package.kind == Kind::AppImage && (action == Action::Run || (action == Action::Integrate && m_package.appImageType == 2)) && !ensureExecutable()) {
        return;
    }
    if (!confirm(action)) {
        return;
    }
    m_message->animatedHide();

    if (action == Action::Run || action == Action::OpenInDiscover) {
        const AppPackages::Command command = AppPackages::command(m_package, action);
        m_tools->launch(command, this, [this, action](const QString &error) {
            if (error.isEmpty()) {
                return;
            }
            m_message->setMessageType(KMessageWidget::Error);
            m_message->setText(action == Action::Run ? i18nc("@info", "Could not run %1: %2", m_package.name, error)
                                                     : i18nc("@info", "Could not open Discover: %1", error));
            m_message->animatedShow();
        });
        return;
    }
    if (action == Action::Integrate) {
        setBusy(true);
        integrate(0);
        return;
    }
    if (action == Action::RemoveIntegration) {
        const QString error = AppPackages::removeIntegration(m_path, m_locations);
        finishChange(action, error.isEmpty(), error);
        return;
    }
    runChange(action);
}

void ApplicationPackageDialog::runChange(Action action)
{
    const AppPackages::Command command = AppPackages::command(m_package, action, m_systemWide->isChecked());
    setBusy(true);
    m_tools->run(command, {}, this, [this, action, command](const PackageTools::Result &result) {
        appendOutput(command, result.output);
        finishChange(action, result.succeeded(), result.started ? lastLines(result.output) : result.output);
    });
}

void ApplicationPackageDialog::integrate(int step)
{
    if (step == 0) {
        m_extractFolder = std::make_unique<QTemporaryDir>();
        if (m_package.appImageType != 2 || !m_extractFolder->isValid()) {
            finishIntegration();
            return;
        }
    }
    const QString root = m_extractFolder->filePath(QStringLiteral("squashfs-root"));
    QString pattern;
    if (step == 0) {
        pattern = QStringLiteral("*.desktop");
    } else if (step == 1) {
        pattern = QStringLiteral(".DirIcon");
    } else if (step == 2) {
        // .DirIcon is usually a link to the icon, which is extracted separately.
        const QFileInfo dirIcon(root + QStringLiteral("/.DirIcon"));
        if (dirIcon.isSymLink() && !dirIcon.exists()) {
            pattern = QDir(root).relativeFilePath(dirIcon.symLinkTarget());
        }
    }
    if (pattern.isEmpty() || pattern.startsWith(QLatin1String(".."))) {
        finishIntegration();
        return;
    }
    m_tools->run(AppPackages::extractCommand(m_path, pattern), m_extractFolder->path(), this, [this, step](const PackageTools::Result &) {
        // Missing files leave the defaults in place, so failures are not errors here.
        integrate(step + 1);
    });
}

void ApplicationPackageDialog::finishIntegration()
{
    QString desktopFile;
    QString icon;
    if (m_extractFolder && m_extractFolder->isValid()) {
        const QString root = m_extractFolder->filePath(QStringLiteral("squashfs-root"));
        const QStringList topLevel = QDir(root).entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name);
        if (!topLevel.isEmpty()) {
            desktopFile = QDir(root).filePath(topLevel.first());
        } else {
            QDirIterator it(root, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
            if (it.hasNext()) {
                desktopFile = it.next();
            }
        }
        const QFileInfo dirIcon(root + QStringLiteral("/.DirIcon"));
        const QString resolved = dirIcon.canonicalFilePath();
        // Only icons inside the extracted image are copied.
        if (!resolved.isEmpty() && resolved.startsWith(QFileInfo(root).canonicalFilePath() + QLatin1Char('/'))
            && QMimeDatabase().mimeTypeForFile(resolved).name().startsWith(QLatin1String("image/"))) {
            icon = resolved;
        }
    }
    const QString error = AppPackages::writeIntegration(m_path, desktopFile, icon, m_locations);
    m_extractFolder.reset();
    finishChange(Action::Integrate, error.isEmpty(), error);
    if (error.isEmpty() && m_package.appImageType == 2 && desktopFile.isEmpty()) {
        m_message->setMessageType(KMessageWidget::Information);
        m_message->setText(i18nc("@info", "The AppImage's name and icon could not be read, so the file name and a generic icon are used."));
        m_message->animatedShow();
    }
}

void ApplicationPackageDialog::finishChange(Action action, bool succeeded, const QString &error)
{
    const QString name = m_package.name;
    setBusy(false);
    if (!succeeded) {
        QString text;
        switch (action) {
        case Action::Install:
            text = i18nc("@info", "Could not install %1.", name);
            break;
        case Action::Update:
            text = i18nc("@info", "Could not update %1.", name);
            break;
        case Action::Uninstall:
            text = i18nc("@info", "Could not uninstall %1.", name);
            break;
        case Action::Integrate:
            text = i18nc("@info", "Could not add %1 to the application menu.", name);
            break;
        case Action::RemoveIntegration:
            text = i18nc("@info", "Could not remove %1 from the application menu.", name);
            break;
        default:
            break;
        }
        if (!error.isEmpty()) {
            text += QLatin1Char('\n') + error;
        }
        m_message->setMessageType(KMessageWidget::Error);
        m_message->setText(text);
        m_message->animatedShow();
        return;
    }

    QString text;
    switch (action) {
    case Action::Install:
        text = m_package.kind == Kind::FlatpakRepo ? i18nc("@info", "Added the repository %1.", name) : i18nc("@info", "Installed %1.", name);
        break;
    case Action::Update:
        text = i18nc("@info", "Updated %1.", name);
        break;
    case Action::Uninstall:
        text = i18nc("@info", "Uninstalled %1.", name);
        break;
    case Action::Integrate:
        text = i18nc("@info", "Added %1 to the application menu.", name);
        break;
    case Action::RemoveIntegration:
        text = i18nc("@info", "Removed %1 from the application menu.", name);
        break;
    default:
        break;
    }
    reload();
    if (action == Action::Install || action == Action::Update) {
        loadDetails(0);
    }
    m_message->setMessageType(KMessageWidget::Positive);
    m_message->setText(text);
    m_message->animatedShow();
    Q_EMIT packageChanged();
}
