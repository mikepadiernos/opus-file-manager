// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libraryscan.h"
#include "metadataimport.h"
#include "operationjob.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QPointer>
#include <QTimer>

#include <atomic>
#include <memory>

class QDialogButtonBox;
class QLabel;
class QProgressBar;
class QPushButton;

/**
 * Indexes or refreshes a tag library with progress and cancellation. The
 * scan runs in the background and its result is merged into the library
 * document; a canceled scan, or one stopped at the scan limit, records what
 * it found but marks nothing missing. It uses the scan options in
 * Preferences › Tagging › Libraries. When Preferences › Tagging › Embedded
 * Metadata imports on refresh, the embedded metadata of the files the refresh
 * added is then read in the background and imported with the saved mappings.
 */
class LibraryRefreshDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LibraryRefreshDialog(const QString &root, QWidget *parent = nullptr);
    ~LibraryRefreshDialog() override;

    void start();
    void cancel();
    bool isRunning() const;
    QString root() const;
    TagLibrary::ScanChanges changes() const;
    /// The summary or error shown when the refresh finished.
    QString message() const;
    /// The number of missing entries in the library after the refresh.
    int missingEntries() const;
    /// The number of items whose tags or fields the embedded metadata import changed.
    int importedItems() const;

    void reject() override;

Q_SIGNALS:
    /// The library document was updated.
    void libraryChanged(const QString &root);
    void refreshFinished();
    /// The user asked to review the missing entries.
    void reviewRequested(const QString &root);

private:
    void finish();
    void startImport(const QStringList &files);
    void finishImport();
    void showFinished();

    QString m_root;
    QLabel *m_status;
    QProgressBar *m_progress;
    QDialogButtonBox *m_buttons;
    QPushButton *m_review;
    QFutureWatcher<LibraryScan::Result> m_watcher;
    QFutureWatcher<QList<EmbeddedMetadata::Reading>> m_importWatcher;
    QPointer<OperationJob> m_job;
    QTimer m_progressTimer;
    std::shared_ptr<std::atomic_bool> m_canceled;
    std::shared_ptr<std::atomic_int> m_scanned;
    LibraryScan::Options m_options;
    MetadataImport::NamedMappings m_mappings;
    QStringList m_keys;
    MetadataImport::Policy m_policy = MetadataImport::Policy::Keep;
    QString m_tagColor;
    int m_importFiles = 0;
    int m_importedItems = 0;
    /// Whether the background refresher was asked to leave this library alone.
    bool m_manual = false;
    TagLibrary::ScanChanges m_changes;
    QString m_message;
    int m_missingEntries = 0;
};
