// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libraryscan.h"

#include <QDialog>
#include <QFutureWatcher>
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
 * document; a canceled scan records what it found but marks nothing missing.
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

    void reject() override;

Q_SIGNALS:
    /// The library document was updated.
    void libraryChanged(const QString &root);
    void refreshFinished();
    /// The user asked to review the missing entries.
    void reviewRequested(const QString &root);

private:
    void finish();

    QString m_root;
    QLabel *m_status;
    QProgressBar *m_progress;
    QDialogButtonBox *m_buttons;
    QPushButton *m_review;
    QFutureWatcher<LibraryScan::Result> m_watcher;
    QTimer m_progressTimer;
    std::shared_ptr<std::atomic_bool> m_canceled;
    std::shared_ptr<std::atomic_int> m_scanned;
    TagLibrary::ScanChanges m_changes;
    QString m_message;
    int m_missingEntries = 0;
};
