// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryautorefresh.h"

#include "libraryregistry.h"
#include "settings.h"
#include "taggingsettings.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QtConcurrentRun>

#include <utility>

namespace
{
KConfigGroup refreshTimes()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("LibraryRefresh"));
}

/// The key of @p root in refreshTimes(); encoded because KConfig gives [ and = in keys a meaning.
QString timeKey(const QString &root)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(QDir::cleanPath(root), "/"));
}

QString libraryName(const QString &root)
{
    const QString name = QFileInfo(root).fileName();
    return name.isEmpty() ? root : name;
}
}

LibraryAutoRefresh::LibraryAutoRefresh(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<LibraryScan::Result>::finished, this, &LibraryAutoRefresh::finish);
    connect(&m_importWatcher, &QFutureWatcher<QList<EmbeddedMetadata::Reading>>::finished, this, &LibraryAutoRefresh::finishImport);
    if (auto *application = QCoreApplication::instance()) {
        connect(application, &QCoreApplication::aboutToQuit, this, [this] {
            m_quitting = true;
            cancel();
        });
    }
}

LibraryAutoRefresh *LibraryAutoRefresh::instance()
{
    static auto *refresher = new LibraryAutoRefresh(qApp);
    return refresher;
}

LibraryAutoRefresh::~LibraryAutoRefresh()
{
    m_quitting = true;
    cancel();
}

void LibraryAutoRefresh::refreshAtStartup()
{
    if (std::exchange(m_startupDone, true) || Settings::automaticLibraryRefresh() != Settings::EnumAutomaticLibraryRefresh::AtStartup) {
        return;
    }
    for (const QString &root : LibraryRegistry::roots()) {
        enqueue(root);
    }
}

void LibraryAutoRefresh::folderOpened(const QUrl &folder)
{
    if (Settings::automaticLibraryRefresh() != Settings::EnumAutomaticLibraryRefresh::WhenOpened || !folder.isLocalFile()) {
        return;
    }
    if (const QString root = LibraryRegistry::rootFor(folder.toLocalFile()); !root.isEmpty()) {
        enqueue(root);
    }
}

void LibraryAutoRefresh::enqueue(const QString &root, bool force)
{
    const QString clean = QDir::cleanPath(root);
    if (m_quitting || clean.isEmpty() || isPending(clean) || m_manual.contains(clean) || !LibraryRegistry::contains(clean)) {
        return;
    }
    if (!force) {
        const QDateTime last = lastRefresh(clean);
        const qint64 elapsed = last.isValid() ? last.secsTo(QDateTime::currentDateTimeUtc()) : -1;
        if (elapsed >= 0 && elapsed < qint64(Settings::libraryRefreshInterval()) * 60) {
            return;
        }
    }
    m_queue.append(clean);
    startNext();
}

bool LibraryAutoRefresh::isRefreshing(const QString &root) const
{
    return !m_current.isEmpty() && m_current == QDir::cleanPath(root);
}

bool LibraryAutoRefresh::isPending(const QString &root) const
{
    return isRefreshing(root) || m_queue.contains(QDir::cleanPath(root));
}

void LibraryAutoRefresh::beginManualRefresh(const QString &root)
{
    const QString clean = QDir::cleanPath(root);
    ++m_manual[clean];
    m_queue.removeAll(clean);
    if (isRefreshing(clean) && m_canceled) {
        m_canceled->store(true);
    }
}

void LibraryAutoRefresh::endManualRefresh(const QString &root, bool completed)
{
    const QString clean = QDir::cleanPath(root);
    const auto it = m_manual.find(clean);
    if (it != m_manual.end() && --it.value() <= 0) {
        m_manual.erase(it);
    }
    if (completed) {
        recordRefresh(clean);
    }
}

void LibraryAutoRefresh::cancel()
{
    m_queue.clear();
    if (m_canceled) {
        m_canceled->store(true);
    }
    m_watcher.waitForFinished();
    m_importWatcher.waitForFinished();
    // The finished signals may still arrive; with no current library their results are dropped.
    m_current.clear();
    m_importRoot.clear();
}

QDateTime LibraryAutoRefresh::lastRefresh(const QString &root)
{
    return refreshTimes().readEntry(timeKey(root), QDateTime());
}

void LibraryAutoRefresh::recordRefresh(const QString &root, const QDateTime &time)
{
    KConfigGroup group = refreshTimes();
    group.writeEntry(timeKey(root), time.toUTC());
    group.sync();
}

void LibraryAutoRefresh::startNext()
{
    if (m_quitting || m_watcher.isRunning() || m_importWatcher.isRunning() || !m_current.isEmpty() || m_queue.isEmpty()) {
        return;
    }
    m_current = m_queue.takeFirst();
    m_options = TaggingSettings::scanOptions();
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const QString root = m_current;
    const LibraryScan::Options options = m_options;
    const auto canceled = m_canceled;
    m_watcher.setFuture(QtConcurrent::run([root, options, canceled] {
        return LibraryScan::scan(root, options, [canceled] {
            return canceled->load();
        });
    }));
}

void LibraryAutoRefresh::finish()
{
    const QString root = std::exchange(m_current, QString());
    const LibraryScan::Result result = m_watcher.result();
    if (!root.isEmpty() && !result.canceled && !m_manual.contains(root) && LibraryRegistry::contains(root)) {
        const LibraryScan::Applied applied = LibraryScan::apply(root, result, m_options);
        recordRefresh(root);
        const TagLibrary::ScanChanges &changes = applied.changes;
        const bool changed = applied.saved && (changes.added || changes.updated || changes.missing || changes.found);
        const QString name = libraryName(root);
        QString message;
        if (!applied.saved) {
            message = i18nc("@info:status %1 is a library name, %2 the reason", "Could not refresh library %1: %2", name, applied.error);
        } else if (!result.errors.isEmpty()) {
            message = i18nc("@info:status %1 is a library name, %2 the reason", "Could not refresh all of library %1: %2", name, result.errors.constFirst());
        } else if (changes.added || changes.missing || result.limited) {
            QStringList parts;
            if (changes.added) {
                parts.append(i18ncp("@info:status entries added to a library", "%1 added", "%1 added", changes.added));
            }
            if (changes.missing) {
                parts.append(i18ncp("@info:status library entries no longer found", "%1 missing", "%1 missing", changes.missing));
            }
            const QString list = parts.join(i18nc("@item list separator", ", "));
            if (result.limited) {
                message = parts.isEmpty()
                    ? i18nc("@info:status %1 is a library name", "Refreshed library %1 up to the scan limit", name)
                    : i18nc("@info:status %1 is a library name, %2 the changes", "Refreshed library %1 up to the scan limit: %2", name, list);
            } else {
                message = i18nc("@info:status %1 is a library name, %2 the changes", "Refreshed library %1: %2", name, list);
            }
        }
        if (applied.saved && !changes.addedFiles.isEmpty() && TaggingSettings::importsEmbeddedMetadataOnRefresh()) {
            // The library stays current until its new files' metadata is imported.
            m_current = root;
            m_importRoot = root;
            m_importMessage = message;
            m_importChanged = changed;
            m_mappings = TaggingSettings::embeddedMetadataMappings();
            m_keys = TaggingSettings::embeddedMetadataKeys();
            m_policy = TaggingSettings::embeddedMetadataPolicy();
            m_tagColor = TaggingSettings::defaultTagColor();
            const QStringList files = changes.addedFiles;
            const QStringList keys = MetadataImport::mappedKeys(m_mappings, m_keys);
            const auto canceled = m_canceled;
            m_importWatcher.setFuture(QtConcurrent::run([root, files, keys, canceled] {
                return MetadataImport::readFiles(root, files, keys, [canceled] {
                    return canceled->load();
                });
            }));
            return;
        }
        Q_EMIT refreshed(root, message, changed);
    }
    startNext();
}

void LibraryAutoRefresh::finishImport()
{
    const QString root = std::exchange(m_importRoot, QString());
    if (root.isEmpty() || root != m_current) {
        startNext();
        return;
    }
    m_current.clear();
    QString message = m_importMessage;
    bool changed = m_importChanged;
    if (!m_canceled->load() && LibraryRegistry::contains(root)) {
        const QString name = libraryName(root);
        MetadataImport::Result result;
        QString error;
        if (!MetadataImport::importReadings(root, m_importWatcher.result(), m_mappings, m_keys, m_policy, m_tagColor, &result, &error)) {
            message = message.isEmpty()
                ? i18nc("@info:status %1 is a library name, %2 the reason", "Could not import embedded metadata into library %1: %2", name, error)
                : i18nc("@info:status %1 is a refresh message, %2 the reason", "%1; could not import embedded metadata: %2", message, error);
        } else if (result.items > 0) {
            changed = true;
            message = message.isEmpty() ? i18ncp("@info:status %2 is a library name",
                                                 "Imported embedded metadata for one item in library %2",
                                                 "Imported embedded metadata for %1 items in library %2",
                                                 result.items,
                                                 name)
                                        : i18ncp("@info:status %2 is a refresh message",
                                                 "%2; imported embedded metadata for one item",
                                                 "%2; imported embedded metadata for %1 items",
                                                 result.items,
                                                 message);
        }
    }
    Q_EMIT refreshed(root, message, changed);
    startNext();
}
