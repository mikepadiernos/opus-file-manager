// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cloudactivity.h"

#include <KSharedConfig>

#include <QDialog>
#include <QPointer>
#include <QWidget>

#include <functional>

class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTimer;
class QTreeWidget;
class RcloneMounts;
class RemoteMountManager;
class TransferQueue;

namespace CloudSync
{
class Scheduler;
}

/// Draws recent transfer speeds as a line, scaled to the peak.
class CloudSpeedGraph : public QWidget
{
    Q_OBJECT

public:
    explicit CloudSpeedGraph(QWidget *parent = nullptr);

    void setHistory(const CloudActivity::SpeedHistory &history);
    QList<quint64> samples() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<quint64> m_samples;
    quint64 m_peak = 0;
    int m_capacity = 60;
};

/**
 * Shows cloud connections, transfers, their speed and recent errors in one
 * window, and sets the limits rclone transfers with.
 */
class CloudActivityDialog : public QDialog
{
    Q_OBJECT

public:
    struct Sources {
        CloudActivity::Monitor *monitor = nullptr;
        TransferQueue *queue = nullptr;
        CloudSync::Scheduler *scheduler = nullptr;
        /// Null until rclone mounts are first used.
        RcloneMounts *rclone = nullptr;
        /// Null until remote connections are first mounted.
        RemoteMountManager *remoteMounts = nullptr;
        KSharedConfig::Ptr config;
        QString cacheDir;
    };

    explicit CloudActivityDialog(const Sources &sources, QWidget *parent = nullptr);

    /// Sets the mount managers created after the window opened.
    void setRcloneMounts(RcloneMounts *rclone);
    void setRemoteMounts(RemoteMountManager *mounts);

    /// Writes the connection and sync-pair definitions to @p path.
    bool exportTo(const QString &path);
    /// Adds the definitions in @p path to the existing ones.
    bool importFrom(const QString &path);

    void refresh();

Q_SIGNALS:
    void limitsChanged(const CloudActivity::Limits &limits);

private:
    void refreshConnections();
    void refreshTransfers();
    void refreshErrors();
    void saveLimits();
    void chooseExport();
    void chooseImport();
    void showMessage(const QString &message);

    QPointer<CloudActivity::Monitor> m_monitor;
    QPointer<TransferQueue> m_queue;
    QPointer<CloudSync::Scheduler> m_scheduler;
    QPointer<RcloneMounts> m_rclone;
    QPointer<RemoteMountManager> m_remoteMounts;
    KSharedConfig::Ptr m_config;
    QString m_cacheDir;
    QTreeWidget *m_connections;
    QTreeWidget *m_transfers;
    CloudSpeedGraph *m_graph;
    QLabel *m_speed;
    QTreeWidget *m_errors;
    QPushButton *m_pause;
    QSpinBox *m_bandwidth;
    QSpinBox *m_parallel;
    QSpinBox *m_cache;
    QLabel *m_message;
    QTimer *m_timer;
};

/**
 * Waits for files written to mounted remotes to finish uploading, and for
 * cloud sync runs to finish, before unmounting or quitting.
 */
class CloudUploadWaitDialog : public QDialog
{
    Q_OBJECT

public:
    using StatusProvider = std::function<CloudActivity::UploadStatus()>;
    /// The dialog's result when the user continues without waiting.
    static constexpr int Skipped = 2;

    CloudUploadWaitDialog(const QString &text, StatusProvider status, QWidget *parent = nullptr);

    /// Sets how often the status is checked, for tests.
    void setPollInterval(int milliseconds);

    /**
     * Shows the dialog while uploads are pending, until they finish or the
     * user continues. Returns false when the user cancels.
     */
    static bool wait(QWidget *parent, const QString &text, const StatusProvider &status);

private:
    void poll();

    StatusProvider m_status;
    QLabel *m_label;
    QProgressBar *m_progress;
    QTimer *m_timer;
    qint64 m_initialBytes = 0;
};
