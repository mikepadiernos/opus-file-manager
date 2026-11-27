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
#include <QMimeData>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include <functional>

namespace
{
/// Keeps prepared copies until Plasma Opus quits, because a mail client may
/// read attached or dropped files after the dialog is closed.
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

/// Lists the selection and drags the attachable files as file URLs.
class AttachmentList : public QListWidget
{
public:
    using UrlsFor = std::function<QList<QUrl>(const QList<QListWidgetItem *> &)>;

    AttachmentList(UrlsFor urlsFor, QWidget *parent)
        : QListWidget(parent)
        , m_urlsFor(std::move(urlsFor))
    {
    }

protected:
    QStringList mimeTypes() const override
    {
        return {QStringLiteral("text/uri-list")};
    }

    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        const QList<QUrl> urls = m_urlsFor(items);
        if (urls.isEmpty()) {
            return nullptr;
        }
        auto *data = new QMimeData;
        data->setUrls(urls);
        return data;
    }

private:
    UrlsFor m_urlsFor;
};

QString itemFileName(const QUrl &url)
{
    const QString name = url.fileName();
    return name.isEmpty() ? QStringLiteral("attachment") : name;
}

QString remoteItemText(const QUrl &url)
{
    return i18nc("@item:inlistbox remote file", "%1 (needs a local copy)", MailHandoff::displayName(url));
}

void setDragUrl(QListWidgetItem *item, const QUrl &url)
{
    item->setData(Qt::UserRole, url);
    item->setFlags(url.isValid() ? item->flags() | Qt::ItemIsDragEnabled : item->flags() & ~Qt::ItemIsDragEnabled);
}
}

SendByEmailDialog::SendByEmailDialog(const KFileItemList &items, QWidget *parent)
    : QDialog(parent)
    , m_selection(MailHandoff::classify(items, MailHandoff::remoteFiles()))
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

    m_items = new AttachmentList(
        [this](const QList<QListWidgetItem *> &dragged) {
            return dragUrls(dragged);
        },
        this);
    m_items->setObjectName(QStringLiteral("mailItems"));
    m_items->setAccessibleName(i18nc("@info:accessibility", "Selected items"));
    m_items->setToolTip(i18nc("@info:tooltip", "Drag files from this list into a message to attach them"));
    m_items->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_items->setDragDropMode(QAbstractItemView::DragOnly);
    m_items->setDragEnabled(true);
    for (const QUrl &file : std::as_const(m_selection.files)) {
        auto *item = new QListWidgetItem(SymbolicIcon::fromTheme(QStringLiteral("mail-attachment")), MailHandoff::displayName(file), m_items);
        setDragUrl(item, file);
    }
    for (const QUrl &remote : std::as_const(m_selection.remote)) {
        auto *item = new QListWidgetItem(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")), remoteItemText(remote), m_items);
        setDragUrl(item, QUrl());
        m_remoteItems.append(item);
    }
    for (const MailHandoff::Rejected &rejected : std::as_const(m_selection.rejected)) {
        auto *item = new QListWidgetItem(SymbolicIcon::fromTheme(QStringLiteral("dialog-cancel")),
                                         i18nc("@item:inlistbox item, why it cannot be attached",
                                               "%1 (%2)",
                                               MailHandoff::displayName(rejected.url),
                                               MailHandoff::reasonText(rejected.reason)),
                                         m_items);
        item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable));
    }
    layout->addWidget(m_items, 1);

    m_prepareCopies = new QCheckBox(i18ncp("@option:check",
                                           "Download a private local copy of the remote file",
                                           "Download private local copies of the %1 remote files",
                                           m_selection.remote.size()),
                                    this);
    m_prepareCopies->setObjectName(QStringLiteral("mailPrepareCopies"));
    m_prepareCopies->setToolTip(i18nc("@info:tooltip",
                                      "Plasma Opus downloads the files into a private temporary folder that only you can read, so they can be attached "
                                      "or dragged into a message. The copies are removed when Plasma Opus quits; the original files are not changed."));
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
    connect(m_prepareCopies, &QCheckBox::toggled, this, &SendByEmailDialog::consentChanged);

    if (!KSandbox::isInside() && !m_client) {
        showMessage(KMessageWidget::Error,
                    i18nc("@info",
                          "No mail client is set up to write messages. Choose one in System Settings › Default Applications, then open Send by "
                          "Email again. You can still drag files from this list into a message."));
    } else if (m_client && !m_client->acceptsAttachments) {
        showMessage(KMessageWidget::Warning,
                    i18nc("@info %1 is an application name",
                          "%1 cannot receive attached files from other applications yet. It will open a new message without the files; "
                          "drag them from this list into its composer.",
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

QList<QUrl> SendByEmailDialog::dragUrls(const QList<QListWidgetItem *> &items)
{
    QList<QUrl> urls;
    bool copies = false;
    for (const QListWidgetItem *item : items) {
        const QUrl url = item->data(Qt::UserRole).toUrl();
        if (url.isValid()) {
            urls.append(url);
            copies = copies || m_remoteItems.contains(item);
        }
    }
    if (copies) {
        // The drop target may read the copies at any time.
        keepCopies();
    }
    return urls;
}

bool SendByEmailDialog::copiesRemoteFiles() const
{
    return m_prepareCopies->isChecked() && !m_selection.remote.isEmpty();
}

bool SendByEmailDialog::copiesReady() const
{
    return copiesRemoteFiles() && !m_copyJob && m_prepared.size() == m_selection.remote.size();
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
        m_summary->setText(m_selection.remote.isEmpty()
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
    const bool downloading = m_copyJob;
    m_prepareCopies->setEnabled(!m_composed && !m_launching);
    m_cancel->setVisible(downloading);
    const bool copiesSettled = !copyRemote || copiesReady();
    m_compose->setEnabled(!m_launching && !m_composed && !downloading && copiesSettled && attachable > 0 && (m_client || KSandbox::isInside()));
}

void SendByEmailDialog::consentChanged(bool checked)
{
    if (checked) {
        prepareCopies();
    } else if (m_copyJob) {
        cancelCopies();
    } else {
        discardCopies();
        m_message->hide();
        updateState();
    }
}

void SendByEmailDialog::prepareCopies()
{
    if (m_copies || m_selection.remote.isEmpty()) {
        updateState();
        return;
    }
    m_copies = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/opus-file-manager-mail-XXXXXX"));
    if (!m_copies->isValid()) {
        const QString error = m_copies->errorString();
        discardCopies();
        showMessage(KMessageWidget::Error, i18nc("@info", "Could not create a private folder for the local copies: %1", error));
        updateState();
        return;
    }
    m_copiesPath = m_copies->path();
    m_prepared.clear();
    prepareNextCopy();
}

void SendByEmailDialog::prepareNextCopy()
{
    const int index = m_prepared.size();
    if (index == m_selection.remote.size()) {
        m_copyJob = nullptr;
        showMessage(KMessageWidget::Positive,
                    i18ncp("@info",
                           "The local copy is ready. Press Compose, or drag it from the list into a message.",
                           "The local copies are ready. Press Compose, or drag them from the list into a message.",
                           m_prepared.size()));
        updateState();
        Q_EMIT copiesPrepared();
        return;
    }
    const QUrl source = m_selection.remote.at(index);
    const QString folder = m_copiesPath + QLatin1Char('/') + QString::number(index + 1);
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
        {
            const QSignalBlocker blocker(m_prepareCopies);
            m_prepareCopies->setChecked(false);
        }
        showMessage(KMessageWidget::Error,
                    i18nc("@info %1 is a file location, %2 an error",
                          "Could not download a local copy of %1: %2. The other copies were removed.",
                          MailHandoff::displayName(source),
                          job->errorString()));
        updateState();
        return;
    }
    const int index = m_prepared.size();
    m_prepared.append(copy->destUrl());
    QListWidgetItem *item = m_remoteItems.at(index);
    item->setText(i18nc("@item:inlistbox remote file", "%1 (local copy ready)", MailHandoff::displayName(m_selection.remote.at(index))));
    item->setIcon(SymbolicIcon::fromTheme(QStringLiteral("mail-attachment")));
    setDragUrl(item, copy->destUrl());
    prepareNextCopy();
}

void SendByEmailDialog::cancelCopies()
{
    if (!m_copyJob) {
        return;
    }
    m_copyJob->kill();
    discardCopies();
    {
        const QSignalBlocker blocker(m_prepareCopies);
        m_prepareCopies->setChecked(false);
    }
    showMessage(KMessageWidget::Information,
                i18nc("@info", "Downloading the local copies was stopped and the partial copies were removed. The original files were not changed."));
    updateState();
}

void SendByEmailDialog::discardCopies()
{
    m_copyJob = nullptr;
    m_copies.reset();
    m_copiesPath.clear();
    m_prepared.clear();
    for (int i = 0; i < m_remoteItems.size(); ++i) {
        QListWidgetItem *item = m_remoteItems.at(i);
        item->setText(remoteItemText(m_selection.remote.at(i)));
        item->setIcon(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")));
        setDragUrl(item, QUrl());
    }
}

void SendByEmailDialog::keepCopies()
{
    if (m_copies) {
        new PreparedCopies(std::move(m_copies), qApp);
    }
}

void SendByEmailDialog::compose()
{
    if (!m_compose->isEnabled()) {
        return;
    }
    m_launching = true;
    m_message->hide();
    updateState();
    launch(m_selection.files + (copiesReady() ? m_prepared : QList<QUrl>()));
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
    m_launching = false;
    if (!error.isEmpty()) {
        showMessage(KMessageWidget::Error, i18nc("@info %1 is an error", "Could not open the mail client: %1", error));
        updateState();
        Q_EMIT composeFinished(error);
        return;
    }
    m_composed = true;
    if (copiesReady()) {
        // The composer may read the copies at any time until the message is sent.
        keepCopies();
    }
    const QString name = m_client ? m_client->name : i18nc("@info the user's mail client", "Your mail client");
    if (m_client && !m_client->acceptsAttachments) {
        showMessage(KMessageWidget::Warning,
                    i18nc("@info %1 is an application name",
                          "%1 opened a new message, but it cannot receive files from other applications yet, so nothing was attached. "
                          "Drag the files from this list into its composer.",
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
