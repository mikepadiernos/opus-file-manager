// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libraryscan.h"
#include "metadataimport.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QStringList>

#include <atomic>
#include <memory>

class QUrl;

/**
 * Refreshes tag libraries quietly in the background, as chosen in
 * Preferences › Tagging › Libraries: when Plasma Opus starts or when a folder
 * in a library is opened. One instance serves every window, and libraries
 * are refreshed one at a time, at most once per refresh interval. Manual
 * refreshes take precedence over automatic ones of the same library. When
 * Preferences › Tagging › Embedded Metadata imports on refresh, the embedded
 * metadata of the files a refresh added is read and imported before the
 * refresh ends.
 */
class LibraryAutoRefresh : public QObject
{
    Q_OBJECT

public:
    static LibraryAutoRefresh *instance();
    ~LibraryAutoRefresh() override;

    /// Queues every registered library once per process when automatic refresh is set to run at startup.
    void refreshAtStartup();
    /// Queues the library containing @p folder when automatic refresh is set to run when a folder in a library is opened.
    void folderOpened(const QUrl &folder);
    /**
     * Queues the registered library @p root unless it is queued, being
     * refreshed manually or, unless @p force is set, was refreshed within the
     * refresh interval.
     */
    void enqueue(const QString &root, bool force = false);

    /// Whether @p root is being refreshed in the background.
    bool isRefreshing(const QString &root) const;
    /// Whether @p root is being refreshed or waits to be.
    bool isPending(const QString &root) const;

    /// Stops refreshing @p root in the background until endManualRefresh(), because it's refreshed manually.
    void beginManualRefresh(const QString &root);
    /// Ends beginManualRefresh(); a @p completed refresh counts as the library's latest refresh.
    void endManualRefresh(const QString &root, bool completed);

    /// Stops the refresh in progress, waits for it and forgets queued libraries.
    void cancel();

    /// When @p root was last refreshed, from the state configuration, or an invalid time.
    static QDateTime lastRefresh(const QString &root);
    static void recordRefresh(const QString &root, const QDateTime &time = QDateTime::currentDateTimeUtc());

Q_SIGNALS:
    /**
     * A background refresh of @p root ended.
     * @param message a short status message when entries were added or marked missing, or the refresh failed; otherwise empty
     * @param changed whether the library document changed
     */
    void refreshed(const QString &root, const QString &message, bool changed);

private:
    explicit LibraryAutoRefresh(QObject *parent);
    void startNext();
    void finish();
    void finishImport();

    QStringList m_queue;
    QString m_current;
    LibraryScan::Options m_options;
    QHash<QString, int> m_manual;
    QFutureWatcher<LibraryScan::Result> m_watcher;
    QFutureWatcher<QList<EmbeddedMetadata::Reading>> m_importWatcher;
    /// The library whose embedded metadata is read, with the refresh's message and change, and the import settings.
    QString m_importRoot;
    QString m_importMessage;
    bool m_importChanged = false;
    MetadataImport::NamedMappings m_mappings;
    QStringList m_keys;
    MetadataImport::Policy m_policy = MetadataImport::Policy::Keep;
    QString m_tagColor;
    std::shared_ptr<std::atomic_bool> m_canceled;
    bool m_startupDone = false;
    bool m_quitting = false;
};
