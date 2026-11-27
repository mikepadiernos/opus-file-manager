// SPDX-License-Identifier: GPL-3.0-or-later

#include "cloudactivitydialog.h"

#include "cloudsyncrunner.h"
#include "rclone.h"
#include "remotemounts.h"
#include "remoteprofiles.h"
#include "symbolicicon.h"
#include "transferqueue.h"

#include <KFormat>
#include <KIO/Global>
#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
const QString limitsGroup = QStringLiteral("CloudActivity");
const QString profilesGroup = QStringLiteral("RemoteProfiles");

QString speedText(quint64 bytesPerSecond)
{
    return i18nc("@info:progress", "%1/s", KIO::convertSize(bytesPerSecond));
}

QString remainingText(qint64 seconds)
{
    return seconds < 0 ? QString() : KFormat().formatSpelloutDuration(quint64(seconds) * 1000);
}

QString spaceText(const std::optional<CloudActivity::Space> &space)
{
    if (!space) {
        return i18nc("@info:status the provider reports no quota", "Unknown");
    }
    return i18nc("@info:status %1 and %2 are sizes", "%1 free of %2", KIO::convertSize(space->available), KIO::convertSize(space->total));
}

QString pendingText(const CloudActivity::PendingUploads &pending)
{
    if (pending.isEmpty()) {
        return {};
    }
    return i18ncp("@info:status %2 is a size", "%1 file, %2", "%1 files, %2", pending.files, KIO::convertSize(pending.bytes));
}

QString transferStateText(QueuedTransfer::State state)
{
    switch (state) {
    case QueuedTransfer::State::Waiting:
        return i18nc("@info:status", "Waiting");
    case QueuedTransfer::State::Running:
        return i18nc("@info:status", "Running");
    case QueuedTransfer::State::Paused:
        return i18nc("@info:status", "Paused");
    case QueuedTransfer::State::Completed:
        return i18nc("@info:status", "Completed");
    case QueuedTransfer::State::Failed:
        return i18nc("@info:status", "Failed");
    case QueuedTransfer::State::Canceled:
        return i18nc("@info:status", "Canceled");
    }
    return {};
}
}

CloudSpeedGraph::CloudSpeedGraph(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("cloudActivitySpeedGraph"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(i18nc("@info accessible name", "Transfer speed"));
}

void CloudSpeedGraph::setHistory(const CloudActivity::SpeedHistory &history)
{
    m_samples = history.samples();
    m_peak = history.peak();
    m_capacity = history.capacity();
    update();
}

QList<quint64> CloudSpeedGraph::samples() const
{
    return m_samples;
}

QSize CloudSpeedGraph::sizeHint() const
{
    return {320, fontMetrics().height() * 5};
}

void CloudSpeedGraph::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(palette().color(QPalette::Mid));
    painter.setBrush(palette().color(QPalette::Base));
    painter.drawRect(area);
    if (m_samples.size() < 2 || m_peak == 0) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(area, Qt::AlignCenter, i18nc("@info:placeholder", "No transfers running"));
        return;
    }
    const qreal step = area.width() / qreal(qMax(1, m_capacity - 1));
    const qreal start = area.right() - step * qreal(m_samples.size() - 1);
    QPainterPath line;
    for (int i = 0; i < m_samples.size(); ++i) {
        const QPointF point(start + step * i, area.bottom() - area.height() * qreal(m_samples.at(i)) / qreal(m_peak));
        if (i == 0) {
            line.moveTo(point);
        } else {
            line.lineTo(point);
        }
    }
    QPainterPath fill = line;
    fill.lineTo(area.right(), area.bottom());
    fill.lineTo(start, area.bottom());
    fill.closeSubpath();
    QColor highlight = palette().color(QPalette::Highlight);
    QColor translucent = highlight;
    translucent.setAlphaF(0.25);
    painter.fillPath(fill, translucent);
    painter.setPen(QPen(highlight, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(line);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(area.adjusted(4, 2, -4, -2), Qt::AlignTop | Qt::AlignLeft, i18nc("@info %1 is a speed", "Peak %1", speedText(m_peak)));
}

CloudActivityDialog::CloudActivityDialog(const Sources &sources, QWidget *parent)
    : QDialog(parent)
    , m_monitor(sources.monitor)
    , m_queue(sources.queue)
    , m_scheduler(sources.scheduler)
    , m_rclone(sources.rclone)
    , m_remoteMounts(sources.remoteMounts)
    , m_config(sources.config)
    , m_cacheDir(sources.cacheDir)
    , m_connections(new QTreeWidget(this))
    , m_transfers(new QTreeWidget(this))
    , m_graph(new CloudSpeedGraph(this))
    , m_speed(new QLabel(this))
    , m_errors(new QTreeWidget(this))
    , m_pause(new QPushButton(this))
    , m_bandwidth(new QSpinBox(this))
    , m_parallel(new QSpinBox(this))
    , m_cache(new QSpinBox(this))
    , m_message(new QLabel(this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("cloudActivityDialog"));
    setWindowTitle(i18nc("@title:window", "Cloud Activity"));
    resize(900, 700);
    auto *layout = new QVBoxLayout(this);
    auto *splitter = new QSplitter(Qt::Vertical, this);
    layout->addWidget(splitter);

    auto *connectionsBox = new QGroupBox(i18nc("@title:group", "Connections"), splitter);
    auto *connectionsLayout = new QVBoxLayout(connectionsBox);
    m_connections->setObjectName(QStringLiteral("cloudActivityConnections"));
    m_connections->setRootIsDecorated(false);
    m_connections->setHeaderLabels({i18nc("@title:column", "Connection"),
                                    i18nc("@title:column", "Type"),
                                    i18nc("@title:column", "Status"),
                                    i18nc("@title:column", "Space"),
                                    i18nc("@title:column", "Waiting to Upload")});
    m_connections->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connectionsLayout->addWidget(m_connections);

    auto *transfersBox = new QGroupBox(i18nc("@title:group", "Transfers"), splitter);
    auto *transfersLayout = new QVBoxLayout(transfersBox);
    m_transfers->setObjectName(QStringLiteral("cloudActivityTransfers"));
    m_transfers->setRootIsDecorated(false);
    m_transfers->setHeaderLabels({i18nc("@title:column", "Transfer"),
                                  i18nc("@title:column", "Status"),
                                  i18nc("@title:column", "Progress"),
                                  i18nc("@title:column", "Speed"),
                                  i18nc("@title:column", "Time Remaining")});
    m_transfers->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    transfersLayout->addWidget(m_transfers);
    transfersLayout->addWidget(m_graph);
    m_speed->setObjectName(QStringLiteral("cloudActivitySpeed"));
    transfersLayout->addWidget(m_speed);

    auto *errorsBox = new QGroupBox(i18nc("@title:group", "Recent Errors"), splitter);
    auto *errorsLayout = new QVBoxLayout(errorsBox);
    m_errors->setObjectName(QStringLiteral("cloudActivityErrors"));
    m_errors->setRootIsDecorated(false);
    m_errors->setHeaderLabels({i18nc("@title:column", "Time"), i18nc("@title:column", "Source"), i18nc("@title:column", "Error")});
    m_errors->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    errorsLayout->addWidget(m_errors);
    auto *clear = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-clear-history")), i18nc("@action:button", "Clear Errors"), errorsBox);
    clear->setObjectName(QStringLiteral("cloudActivityClearErrors"));
    connect(clear, &QPushButton::clicked, this, [this] {
        if (m_monitor) {
            m_monitor->clearEvents();
        }
    });
    errorsLayout->addWidget(clear, 0, Qt::AlignRight);

    auto *limitsBox = new QGroupBox(i18nc("@title:group", "rclone Limits"), this);
    auto *limitsLayout = new QFormLayout(limitsBox);
    m_bandwidth->setObjectName(QStringLiteral("cloudActivityBandwidth"));
    m_bandwidth->setRange(0, 1024 * 1024);
    m_bandwidth->setSingleStep(128);
    m_bandwidth->setSpecialValueText(i18nc("@item:inrange", "No limit"));
    m_bandwidth->setSuffix(i18nc("@item:valuesuffix kibibytes per second", " KiB/s"));
    limitsLayout->addRow(i18nc("@label:spinbox", "Bandwidth:"), m_bandwidth);
    m_parallel->setObjectName(QStringLiteral("cloudActivityParallel"));
    m_parallel->setRange(1, 64);
    limitsLayout->addRow(i18nc("@label:spinbox", "Parallel transfers:"), m_parallel);
    m_cache->setObjectName(QStringLiteral("cloudActivityCache"));
    m_cache->setRange(0, 1024 * 1024);
    m_cache->setSingleStep(256);
    m_cache->setSpecialValueText(i18nc("@item:inrange", "Files being written only"));
    m_cache->setSuffix(i18nc("@item:valuesuffix mebibytes", " MiB"));
    m_cache->setToolTip(i18nc("@info:tooltip", "Keeps recently opened files of mounted remotes, so they open again without downloading."));
    limitsLayout->addRow(i18nc("@label:spinbox", "Offline cache:"), m_cache);
    auto *limitsNote = new QLabel(i18nc("@info", "Limits apply to rclone sync pairs at their next run and to remotes when they are next mounted."), limitsBox);
    limitsNote->setWordWrap(true);
    limitsLayout->addRow(limitsNote);
    layout->addWidget(limitsBox);
    const CloudActivity::Limits limits = CloudActivity::loadLimits(m_config->group(limitsGroup));
    m_bandwidth->setValue(limits.bandwidthKiB);
    m_parallel->setValue(limits.parallelTransfers);
    m_cache->setValue(limits.cacheMiB);
    for (QSpinBox *box : {m_bandwidth, m_parallel, m_cache}) {
        connect(box, &QSpinBox::valueChanged, this, &CloudActivityDialog::saveLimits);
    }

    m_message->setObjectName(QStringLiteral("cloudActivityMessage"));
    m_message->setTextFormat(Qt::PlainText);
    m_message->setWordWrap(true);
    m_message->hide();
    layout->addWidget(m_message);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_pause->setObjectName(QStringLiteral("cloudActivityPause"));
    m_pause->setCheckable(true);
    connect(m_pause, &QPushButton::toggled, this, [this](bool paused) {
        if (m_monitor && m_monitor->isPaused() != paused) {
            m_monitor->setPaused(paused);
        }
    });
    auto *exportButton = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-export")), i18nc("@action:button", "Export…"), this);
    exportButton->setObjectName(QStringLiteral("cloudActivityExport"));
    exportButton->setToolTip(i18nc("@info:tooltip", "Save connections and sync pairs, without passwords, to set them up on another computer."));
    auto *importButton = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-import")), i18nc("@action:button", "Import…"), this);
    importButton->setObjectName(QStringLiteral("cloudActivityImport"));
    buttons->addButton(m_pause, QDialogButtonBox::ActionRole);
    buttons->addButton(exportButton, QDialogButtonBox::ActionRole);
    buttons->addButton(importButton, QDialogButtonBox::ActionRole);
    connect(exportButton, &QPushButton::clicked, this, &CloudActivityDialog::chooseExport);
    connect(importButton, &QPushButton::clicked, this, &CloudActivityDialog::chooseImport);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);

    if (m_monitor) {
        connect(m_monitor, &CloudActivity::Monitor::changed, this, &CloudActivityDialog::refresh);
    }
    // Mounts and quotas change without signals, so they are checked while the window is open.
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &CloudActivityDialog::refresh);
    m_timer->start();
    refresh();
}

void CloudActivityDialog::setRcloneMounts(RcloneMounts *rclone)
{
    m_rclone = rclone;
    refresh();
}

void CloudActivityDialog::setRemoteMounts(RemoteMountManager *mounts)
{
    m_remoteMounts = mounts;
    refresh();
}

void CloudActivityDialog::refresh()
{
    refreshConnections();
    refreshTransfers();
    refreshErrors();
    const bool paused = m_monitor && m_monitor->isPaused();
    {
        const QSignalBlocker blocker(m_pause);
        m_pause->setChecked(paused);
    }
    m_pause->setText(paused ? i18nc("@action:button", "Resume All") : i18nc("@action:button", "Pause All"));
    m_pause->setIcon(SymbolicIcon::fromTheme(paused ? QStringLiteral("media-playback-start") : QStringLiteral("media-playback-pause")));
    m_pause->setToolTip(i18nc("@info:tooltip", "Pauses queued transfers and background cloud sync runs. Mounted remotes keep uploading."));
}

void CloudActivityDialog::refreshConnections()
{
    const QString selected = m_connections->currentItem() ? m_connections->currentItem()->data(0, Qt::UserRole).toString() : QString();
    const QSignalBlocker blocker(m_connections);
    m_connections->clear();
    const auto add = [this, &selected](const QString &key, const QStringList &columns) {
        auto *item = new QTreeWidgetItem(m_connections, columns);
        item->setData(0, Qt::UserRole, key);
        if (key == selected) {
            m_connections->setCurrentItem(item);
        }
    };

    const QStringList mounted = m_rclone ? m_rclone->mountedRemotes() : QStringList();
    QStringList remotes = mounted;
    const QStringList pending = CloudActivity::remotesWithPendingUploads(m_cacheDir);
    for (const QString &remote : pending) {
        if (!remotes.contains(remote)) {
            remotes.append(remote);
        }
    }
    for (const QString &remote : std::as_const(remotes)) {
        const bool isMounted = mounted.contains(remote);
        add(QStringLiteral("rclone:") + remote,
            {remote,
             i18nc("@item connection type", "rclone mount"),
             isMounted ? i18nc("@info:status", "Mounted") : i18nc("@info:status", "Not mounted"),
             isMounted ? spaceText(CloudActivity::mountSpace(Rclone::mountPoint(remote))) : QString(),
             pendingText(CloudActivity::pendingUploads(m_cacheDir, remote))});
    }

    if (m_remoteMounts) {
        QHash<QString, QString> names;
        const QList<RemoteProfiles::Profile> profiles = RemoteProfiles::load(m_config->group(profilesGroup));
        for (const RemoteProfiles::Profile &profile : profiles) {
            names.insert(profile.id, profile.name);
        }
        const QStringList ids = m_remoteMounts->mountedIds();
        for (const QString &id : ids) {
            const QString path = m_remoteMounts->path(id);
            add(QStringLiteral("mount:") + id,
                {names.value(id, id),
                 i18nc("@item connection type, %1 is a backend such as sshfs", "%1 mount", RemoteMounts::name(m_remoteMounts->backend(id))),
                 m_remoteMounts->isStarting(id) ? i18nc("@info:status", "Mounting") : i18nc("@info:status", "Mounted"),
                 path.isEmpty() ? QString() : spaceText(CloudActivity::mountSpace(path)),
                 QString()});
        }
    }

    if (m_scheduler) {
        const QList<CloudSync::Pair> pairs = m_scheduler->pairs();
        for (const CloudSync::Pair &pair : pairs) {
            QString state;
            switch (m_scheduler->state(pair.id)) {
            case CloudSync::Scheduler::State::Running:
                state = i18nc("@info:status", "Synchronizing");
                break;
            case CloudSync::Scheduler::State::Queued:
                state = i18nc("@info:status", "Queued");
                break;
            case CloudSync::Scheduler::State::Idle:
                if (m_scheduler->isPaused(pair)) {
                    state = i18nc("@info:status", "Paused");
                } else if (const CloudSync::Report report = m_scheduler->lastReport(pair.id); report.finishedAt.isValid()) {
                    state = CloudSync::summary(report);
                } else {
                    state = i18nc("@info:status", "Not run yet");
                }
                break;
            }
            add(QStringLiteral("sync:") + pair.id,
                {i18nc("@item %1 is a local folder, %2 a remote", "%1 ⇄ %2", pair.localPath, CloudSync::remoteDisplayName(pair)),
                 i18nc("@item connection type", "Sync pair"),
                 state,
                 QString(),
                 QString()});
        }
    }
}

void CloudActivityDialog::refreshTransfers()
{
    const QSignalBlocker blocker(m_transfers);
    m_transfers->clear();
    if (m_queue) {
        const auto entries = m_queue->entries();
        for (const QueuedTransfer *entry : entries) {
            const auto state = entry->state();
            if (state != QueuedTransfer::State::Waiting && state != QueuedTransfer::State::Running && state != QueuedTransfer::State::Paused) {
                continue;
            }
            const qint64 total = qint64(entry->totalAmount(KJob::Bytes));
            const qint64 processed = qint64(entry->processedAmount(KJob::Bytes));
            const quint64 speed = state == QueuedTransfer::State::Running ? entry->bytesPerSecond() : 0;
            QString progress = i18nc("@info:progress", "%1%", entry->percent());
            if (total > 0) {
                progress = i18nc("@info:progress %1 and %2 are sizes, %3 a percentage",
                                 "%1 of %2 (%3%)",
                                 KIO::convertSize(processed),
                                 KIO::convertSize(total),
                                 entry->percent());
            }
            new QTreeWidgetItem(m_transfers,
                                {entry->description(),
                                 transferStateText(state),
                                 progress,
                                 speed ? speedText(speed) : QString(),
                                 remainingText(CloudActivity::remainingSeconds(total, processed, speed))});
        }
    }
    if (m_scheduler) {
        const QList<CloudSync::Pair> pairs = m_scheduler->pairs();
        for (const CloudSync::Pair &pair : pairs) {
            if (m_scheduler->state(pair.id) != CloudSync::Scheduler::State::Running) {
                continue;
            }
            new QTreeWidgetItem(m_transfers,
                                {i18nc("@item %1 is a remote", "Cloud Sync of %1", CloudSync::remoteDisplayName(pair)),
                                 i18nc("@info:status", "Running"),
                                 m_scheduler->progress(pair.id),
                                 QString(),
                                 QString()});
        }
    }
    if (m_monitor) {
        m_graph->setHistory(m_monitor->speedHistory());
        const quint64 speed = m_monitor->currentSpeed();
        m_speed->setText(speed ? i18nc("@info:status %1 is a speed", "Current speed: %1", speedText(speed))
                               : i18nc("@info:status", "No queued transfers are running."));
    }
}

void CloudActivityDialog::refreshErrors()
{
    if (!m_monitor) {
        return;
    }
    const QList<CloudActivity::Event> events = m_monitor->events();
    if (m_errors->topLevelItemCount() == events.size()
        && (events.isEmpty() || m_errors->topLevelItem(0)->data(0, Qt::UserRole).toDateTime() == events.last().time)) {
        return;
    }
    m_errors->clear();
    // Newest first.
    for (auto it = events.crbegin(); it != events.crend(); ++it) {
        auto *item = new QTreeWidgetItem(m_errors, {QLocale().toString(it->time.time(), QLocale::ShortFormat), it->source, it->message});
        item->setData(0, Qt::UserRole, it->time);
        item->setToolTip(0, QLocale().toString(it->time, QLocale::LongFormat));
        item->setToolTip(2, it->message);
    }
}

void CloudActivityDialog::saveLimits()
{
    CloudActivity::Limits limits;
    limits.bandwidthKiB = m_bandwidth->value();
    limits.parallelTransfers = m_parallel->value();
    limits.cacheMiB = m_cache->value();
    KConfigGroup group = m_config->group(limitsGroup);
    CloudActivity::saveLimits(group, limits);
    group.sync();
    Q_EMIT limitsChanged(limits);
}

bool CloudActivityDialog::exportTo(const QString &path)
{
    CloudActivity::Definitions definitions;
    definitions.profiles = RemoteProfiles::load(m_config->group(profilesGroup));
    if (m_scheduler) {
        definitions.pairs = m_scheduler->pairs();
    }
    QString error;
    if (!CloudActivity::exportDefinitions(path, definitions, &error)) {
        showMessage(i18nc("@info %1 is the error", "Could not export the definitions: %1", error));
        return false;
    }
    showMessage(i18nc("@info %1 and %2 are counts, %3 a file",
                      "Exported %1 connections and %2 sync pairs to %3. Passwords are not included.",
                      definitions.profiles.size(),
                      definitions.pairs.size(),
                      path));
    return true;
}

bool CloudActivityDialog::importFrom(const QString &path)
{
    QString error;
    const auto imported = CloudActivity::importDefinitions(path, &error);
    if (!imported) {
        showMessage(i18nc("@info %1 is the error", "Could not import the definitions: %1", error));
        return false;
    }
    CloudActivity::Definitions existing;
    existing.profiles = RemoteProfiles::load(m_config->group(profilesGroup));
    if (m_scheduler) {
        existing.pairs = m_scheduler->pairs();
    }
    CloudActivity::MergeResult result = CloudActivity::merge(existing, *imported);
    if (result.addedProfiles > 0 && !RemoteProfiles::save(m_config->group(profilesGroup), result.profiles, &error)) {
        showMessage(i18nc("@info %1 is the error", "Could not save the imported connections: %1", error));
        return false;
    }
    if (result.addedPairs > 0 && (!m_scheduler || !m_scheduler->setPairs(result.pairs, &error))) {
        showMessage(i18nc("@info %1 is the error", "Could not save the imported sync pairs: %1", error));
        return false;
    }
    QStringList lines;
    lines.append(i18nc("@info %1 and %2 are counts", "Imported %1 connections and %2 sync pairs.", result.addedProfiles, result.addedPairs));
    if (result.addedProfiles > 0 || result.addedPairs > 0) {
        lines.append(i18nc("@info",
                           "Passwords are not imported, so sign in again when you first open each connection. "
                           "Imported sync pairs are paused; resume them in Cloud Sync once their accounts work."));
    }
    lines += result.skipped;
    showMessage(lines.join(QLatin1Char('\n')));
    refresh();
    return true;
}

void CloudActivityDialog::chooseExport()
{
    const QString start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/plasma-opus-connections.opusdefs");
    const QString path = QFileDialog::getSaveFileName(this,
                                                      i18nc("@title:window", "Export Connections and Sync Pairs"),
                                                      start,
                                                      i18nc("@item:inlistbox file filter", "Plasma Opus definitions (*.opusdefs)"));
    if (!path.isEmpty()) {
        exportTo(path);
    }
}

void CloudActivityDialog::chooseImport()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      i18nc("@title:window", "Import Connections and Sync Pairs"),
                                                      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                      i18nc("@item:inlistbox file filter", "Plasma Opus definitions (*.opusdefs)"));
    if (!path.isEmpty()) {
        importFrom(path);
    }
}

void CloudActivityDialog::showMessage(const QString &message)
{
    m_message->setText(message);
    m_message->setVisible(!message.isEmpty());
}

CloudUploadWaitDialog::CloudUploadWaitDialog(const QString &text, StatusProvider status, QWidget *parent)
    : QDialog(parent)
    , m_status(std::move(status))
    , m_label(new QLabel(this))
    , m_progress(new QProgressBar(this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("cloudUploadWait"));
    setWindowTitle(i18nc("@title:window", "Waiting for Uploads"));
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(text, this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    m_label->setObjectName(QStringLiteral("cloudUploadStatus"));
    m_label->setWordWrap(true);
    layout->addWidget(m_label);
    m_progress->setObjectName(QStringLiteral("cloudUploadProgress"));
    layout->addWidget(m_progress);
    auto *buttons = new QDialogButtonBox(this);
    auto *skip = buttons->addButton(i18nc("@action:button", "Continue Without Waiting"), QDialogButtonBox::DestructiveRole);
    skip->setObjectName(QStringLiteral("cloudUploadContinue"));
    skip->setToolTip(i18nc("@info:tooltip", "Files not uploaded yet stay in the cache and upload when the remote is next mounted."));
    auto *cancel = buttons->addButton(QDialogButtonBox::Cancel);
    cancel->setObjectName(QStringLiteral("cloudUploadCancel"));
    connect(skip, &QPushButton::clicked, this, [this] {
        done(Skipped);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    m_initialBytes = m_status ? m_status().uploads.bytes : 0;
    m_timer->setInterval(500);
    connect(m_timer, &QTimer::timeout, this, &CloudUploadWaitDialog::poll);
    m_timer->start();
    poll();
}

void CloudUploadWaitDialog::setPollInterval(int milliseconds)
{
    m_timer->setInterval(milliseconds);
}

void CloudUploadWaitDialog::poll()
{
    const CloudActivity::UploadStatus status = m_status ? m_status() : CloudActivity::UploadStatus();
    if (status.isIdle()) {
        m_timer->stop();
        // Accepting from the constructor's poll would be ignored, so wait for the event loop.
        QTimer::singleShot(0, this, &QDialog::accept);
        return;
    }
    QStringList lines;
    if (!status.uploads.isEmpty()) {
        lines.append(i18ncp("@info:status %2 is a size",
                            "%1 file (%2) is waiting to upload.",
                            "%1 files (%2) are waiting to upload.",
                            status.uploads.files,
                            KIO::convertSize(status.uploads.bytes)));
    }
    if (status.syncRuns > 0) {
        lines.append(i18ncp("@info:status", "%1 cloud sync run is in progress.", "%1 cloud sync runs are in progress.", status.syncRuns));
    }
    m_label->setText(lines.join(QLatin1Char('\n')));
    m_initialBytes = qMax(m_initialBytes, status.uploads.bytes);
    if (m_initialBytes > 0) {
        m_progress->setRange(0, 1000);
        m_progress->setValue(int(1000 * (m_initialBytes - status.uploads.bytes) / m_initialBytes));
    } else {
        m_progress->setRange(0, 0);
    }
}

bool CloudUploadWaitDialog::wait(QWidget *parent, const QString &text, const StatusProvider &status)
{
    if (!status || status().isIdle()) {
        return true;
    }
    CloudUploadWaitDialog dialog(text, status, parent);
    return dialog.exec() != QDialog::Rejected;
}
