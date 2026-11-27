// SPDX-License-Identifier: GPL-3.0-or-later

#include "sendbyemaildialog.h"
#include "symbolicicon.h"

#include <KEMailClientLauncherJob>
#include <KIO/FileCopyJob>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KSandbox>

#include <QApplication>
#include <QCheckBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVBoxLayout>

namespace
{
/// Keeps prepared copies until Plasma Opus quits, because the mail client may
/// read the attachments after the dialog is closed.
class PreparedCopies : public QObject
{
public:
    PreparedCopies(std::unique_ptr<QTemporaryDir> dir, QObject *parent)
        : QObject(parent)
        , m_dir(std::move(dir))
    {
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

QString itemFileName(const QUrl &url)
{
    const QString name = url.fileName();
    return name.isEmpty() ? QStringLiteral("attachment") : name;
}
}

SendByEmailDialog::SendByEmailDialog(const KFileItemList &items, QWidget *parent)
    : QDialog(parent)
    , m_selection(MailHandoff::classify(items))
    , m_client(KSandbox::isInside() ? std::nullopt : MailHandoff::preferredClient())
{
    setObjectName(QStringLiteral("sendByEmailDialog"));
    setWindowTitle(i18nc("@title:window", "Send by Email"));
    auto *layout = new QVBoxLayout(this);

    m_message = new KMessageWidget(this);
    m_message->setObjectName(QStringLiteral("mailMessage"));
    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(false);
    m_message->hide();
    layout->addWidget(m_message);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("mailSummary"));
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    m_items = new QListWidget(this);
    m_items->setObjectName(QStringLiteral("mailItems"));
    m_items->setAccessibleName(i18nc("@info:accessibility", "Selected items"));
    m_items->setSelectionMode(QAbstractItemView::NoSelection);
    for (const QUrl &file : std::as_const(m_selection.files)) {
        new QListWidgetItem(SymbolicIcon::fromTheme(QStringLiteral("mail-attachment")), MailHandoff::displayName(file), m_items);
    }
    for (const QUrl &remote : std::as_const(m_selection.remote)) {
        new QListWidgetItem(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")),
                            i18nc("@item:inlistbox remote file", "%1 (needs a local copy)", MailHandoff::displayName(remote)),
                            m_items);
    }
    for (const MailHandoff::Rejected &rejected : std::as_const(m_selection.rejected)) {
        auto *item = new QListWidgetItem(SymbolicIcon::fromTheme(QStringLiteral("dialog-cancel")),
                                         i18nc("@item:inlistbox item, why it cannot be attached",
                                               "%1 (%2)",
                                               MailHandoff::displayName(rejected.url),
                                               MailHandoff::reasonText(rejected.reason)),
                                         m_items);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    layout->addWidget(m_items, 1);

    m_prepareCopies = new QCheckBox(i18ncp("@option:check",
                                           "Download a private local copy of the remote file to attach it",
                                           "Download private local copies of the %1 remote files to attach them",
                                           m_selection.remote.size()),
                                    this);
    m_prepareCopies->setObjectName(QStringLiteral("mailPrepareCopies"));
    m_prepareCopies->setToolTip(i18nc("@info:tooltip",
                                      "Plasma Opus downloads the files into a private temporary folder that only you can read, and removes it when "
                                      "Plasma Opus quits. The original files are not changed."));
    m_prepareCopies->setVisible(!m_selection.remote.isEmpty());
    layout->addWidget(m_prepareCopies);

    m_clientLabel = new QLabel(this);
    m_clientLabel->setObjectName(QStringLiteral("mailClient"));
    m_clientLabel->setWordWrap(true);
    if (KSandbox::isInside()) {
        m_clientLabel->setText(i18nc("@info", "The desktop portal opens a new message in your preferred mail client."));
    } else if (m_client) {
        m_clientLabel->setText(i18nc("@info %1 is an application name", "Mail client: %1", m_client->name));
    }
    m_clientLabel->setVisible(!m_clientLabel->text().isEmpty());
    layout->addWidget(m_clientLabel);

    auto *note = new QLabel(i18nc("@info", "The message opens for you to review; nothing is sent until you send it from the mail client."), this);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_cancel = buttons->addButton(i18nc("@action:button", "Stop Downloading"), QDialogButtonBox::ActionRole);
    m_cancel->setObjectName(QStringLiteral("mailCancel"));
    m_cancel->setIcon(SymbolicIcon::fromTheme(QStringLiteral("process-stop")));
    m_cancel->hide();
    m_compose = buttons->addButton(i18nc("@action:button", "Compose"), QDialogButtonBox::ActionRole);
    m_compose->setObjectName(QStringLiteral("mailCompose"));
    m_compose->setIcon(SymbolicIcon::fromTheme(QStringLiteral("mail-message-new")));
    m_compose->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_compose, &QPushButton::clicked, this, &SendByEmailDialog::compose);
    connect(m_cancel, &QPushButton::clicked, this, &SendByEmailDialog::cancelCopies);
    connect(m_prepareCopies, &QCheckBox::toggled, this, &SendByEmailDialog::updateState);

    if (!KSandbox::isInside() && !m_client) {
        showMessage(KMessageWidget::Error,
                    i18nc("@info",
                          "No mail client is set up to write messages. Choose one in System Settings › Default Applications, then open Send by "
                          "Email again."));
    } else if (m_client && !m_client->acceptsAttachments) {
        m_prepareCopies->setEnabled(false);
        showMessage(KMessageWidget::Warning,
                    i18nc("@info %1 is an application name",
                          "%1 cannot receive attached files from other applications yet. It will open a new message without the files; "
                          "attach them from its composer or drag them into it.",
                          m_client->name));
    }

    resize(560, 420);
    updateState();
}

SendByEmailDialog::~SendByEmailDialog()
{
    if (m_copyJob) {
        m_copyJob->kill();
    }
}

QString SendByEmailDialog::copiesPath() const
{
    return m_copiesPath;
}

bool SendByEmailDialog::copiesRemoteFiles() const
{
    return m_prepareCopies->isChecked() && !m_selection.remote.isEmpty() && (!m_client || m_client->acceptsAttachments);
}

void SendByEmailDialog::discardCopies()
{
    m_copyJob = nullptr;
    m_copies.reset();
    m_copiesPath.clear();
    m_prepared.clear();
}

void SendByEmailDialog::showMessage(int type, const QString &text)
{
    m_message->setMessageType(static_cast<KMessageWidget::MessageType>(type));
    m_message->setText(text);
    m_message->show();
}

void SendByEmailDialog::updateState()
{
    const bool copyRemote = copiesRemoteFiles();
    const int attachable = m_selection.files.size() + (copyRemote ? m_selection.remote.size() : 0);
    const int skipped = m_selection.rejected.size() + (copyRemote ? 0 : m_selection.remote.size());
    if (attachable == 0) {
        m_summary->setText(m_selection.remote.isEmpty() || !m_prepareCopies->isEnabled()
                               ? i18nc("@info", "None of the selected items can be attached.")
                               : i18nc("@info", "None of the selected items can be attached unless you allow local copies of the remote files."));
    } else if (skipped == 0) {
        m_summary->setText(i18ncp("@info", "One file will be attached.", "%1 files will be attached.", attachable));
    } else {
        m_summary->setText(i18ncp("@info %2 is a count of files",
                                  "%2 will be attached; one item will be skipped.",
                                  "%2 will be attached; %1 items will be skipped.",
                                  skipped,
                                  i18ncp("@info", "One file", "%1 files", attachable)));
    }
    m_prepareCopies->setEnabled(!m_busy && !m_composed && (!m_client || m_client->acceptsAttachments));
    m_cancel->setVisible(m_busy && m_copyJob);
    m_compose->setEnabled(!m_busy && !m_composed && attachable > 0 && (m_client || KSandbox::isInside()));
}

void SendByEmailDialog::compose()
{
    if (!m_compose->isEnabled()) {
        return;
    }
    m_busy = true;
    m_message->hide();
    if (copiesRemoteFiles()) {
        m_copies = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/opus-file-manager-mail-XXXXXX"));
        if (!m_copies->isValid()) {
            const QString error = m_copies->errorString();
            m_copies.reset();
            m_busy = false;
            showMessage(KMessageWidget::Error, i18nc("@info", "Could not create a private folder for the local copies: %1", error));
            updateState();
            return;
        }
        m_copiesPath = m_copies->path();
        m_prepared.clear();
        prepareNextCopy();
        return;
    }
    updateState();
    launch(m_selection.files);
}

void SendByEmailDialog::prepareNextCopy()
{
    const int index = m_prepared.size();
    if (index == m_selection.remote.size()) {
        m_copyJob = nullptr;
        updateState();
        launch(m_selection.files + m_prepared);
        return;
    }
    const QUrl source = m_selection.remote.at(index);
    const QString folder = m_copies->path() + QLatin1Char('/') + QString::number(index + 1);
    QDir().mkpath(folder);
    const QUrl destination = QUrl::fromLocalFile(folder + QLatin1Char('/') + itemFileName(source));
    showMessage(KMessageWidget::Information,
                i18nc("@info %1 is a file location",
                      "Downloading a local copy of %1 (%2 of %3)…",
                      MailHandoff::displayName(source),
                      index + 1,
                      m_selection.remote.size()));
    KIO::FileCopyJob *job = KIO::file_copy(source, destination, 0600, KIO::HideProgressInfo);
    m_copyJob = job;
    connect(job, &KJob::result, this, &SendByEmailDialog::copyFinished);
    updateState();
}

void SendByEmailDialog::copyFinished(KJob *job)
{
    if (job != m_copyJob) {
        return;
    }
    auto *copy = static_cast<KIO::FileCopyJob *>(job);
    if (job->error()) {
        const QUrl source = copy->srcUrl();
        discardCopies();
        m_busy = false;
        showMessage(KMessageWidget::Error,
                    i18nc("@info %1 is a file location, %2 an error",
                          "Could not download a local copy of %1: %2. The mail client was not opened.",
                          MailHandoff::displayName(source),
                          job->errorString()));
        updateState();
        return;
    }
    m_prepared.append(copy->destUrl());
    prepareNextCopy();
}

void SendByEmailDialog::cancelCopies()
{
    if (!m_copyJob) {
        return;
    }
    m_copyJob->kill();
    discardCopies();
    m_busy = false;
    showMessage(KMessageWidget::Information,
                i18nc("@info", "Downloading the local copies was stopped and the partial copies were removed. The original files were not changed."));
    updateState();
}

void SendByEmailDialog::launch(const QList<QUrl> &attachments)
{
    if (KSandbox::isInside()) {
        launchThroughPortal(attachments);
        return;
    }
    auto *job = new KEMailClientLauncherJob(this);
    job->setAttachments(attachments);
    const int count = attachments.size();
    connect(job, &KJob::result, this, [this, job, count] {
        finish(job->error() ? job->errorString() : QString(), count);
    });
    job->start();
}

void SendByEmailDialog::launchThroughPortal(const QList<QUrl> &attachments)
{
    qDBusRegisterMetaType<QList<QDBusUnixFileDescriptor>>();
    QList<QDBusUnixFileDescriptor> descriptors;
    for (const QUrl &url : attachments) {
        QFile file(url.toLocalFile());
        if (!file.open(QIODevice::ReadOnly)) {
            finish(i18nc("@info %1 is a file path, %2 an error", "Could not open %1: %2", url.toLocalFile(), file.errorString()), 0);
            return;
        }
        // QDBusUnixFileDescriptor duplicates the descriptor, so the file can be closed.
        descriptors.append(QDBusUnixFileDescriptor(file.handle()));
    }
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                          QStringLiteral("/org/freedesktop/portal/desktop"),
                                                          QStringLiteral("org.freedesktop.portal.Email"),
                                                          QStringLiteral("ComposeEmail"));
    message << QString() << MailHandoff::portalOptions(descriptors);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    const int count = attachments.size();
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, count](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *call;
        finish(reply.isError() ? reply.error().message() : QString(), count);
    });
}

void SendByEmailDialog::finish(const QString &error, int attached)
{
    m_busy = false;
    if (!error.isEmpty()) {
        discardCopies();
        showMessage(KMessageWidget::Error, i18nc("@info %1 is an error", "Could not open the mail client: %1", error));
        updateState();
        Q_EMIT composeFinished(error);
        return;
    }
    m_composed = true;
    if (m_copies) {
        // The composer may read the copies at any time until the message is sent.
        new PreparedCopies(std::move(m_copies), qApp);
    }
    const QString name = m_client ? m_client->name : i18nc("@info the user's mail client", "Your mail client");
    if (m_client && !m_client->acceptsAttachments) {
        showMessage(KMessageWidget::Warning,
                    i18nc("@info %1 is an application name",
                          "%1 opened a new message, but it cannot receive files from other applications yet, so nothing was attached. "
                          "Attach the files from its composer, or drag them into it.",
                          name));
    } else {
        showMessage(KMessageWidget::Positive,
                    i18ncp("@info %2 is an application name",
                           "%2 opened a new message with one attached file. Review the message and send it from the mail client.",
                           "%2 opened a new message with %1 attached files. Review the message and send it from the mail client.",
                           attached,
                           name));
    }
    updateState();
    Q_EMIT composeFinished(QString());
}
