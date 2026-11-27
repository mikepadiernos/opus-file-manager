// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldersync.h"
#include "foldersyncprofiles.h"
#include "operationjob.h"
#include "rsyncsync.h"

#include <KSharedConfig>
#include <QDialog>
#include <QFutureWatcher>
#include <QPointer>

#include <atomic>
#include <memory>
#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;

class FolderSyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FolderSyncDialog(const QString &left, const QString &right, QWidget *parent = nullptr, KSharedConfig::Ptr config = {});
    /// Compares local folders, or a local folder and an SSH location through rsync.
    explicit FolderSyncDialog(const QUrl &left, const QUrl &right, QWidget *parent = nullptr, KSharedConfig::Ptr config = {});
    ~FolderSyncDialog() override;
    void reject() override;

private:
    FolderSync::Options options() const;
    void setOptions(const FolderSync::Options &options);
    void refreshProfiles();
    void invalidate();
    void compare();
    void compareWithRsync(const FolderSync::Options &options);
    void showComparison(const FolderSync::Comparison &comparison);
    void updateSelectedAction();
    void planCopies(FolderSync::Action direction);
    void synchronize();
    void checkWithRsync(const RsyncSync::Transfer &transfer, const QStringList &details);
    void synchronizeWithRsync(const RsyncSync::Transfer &transfer);
    void setRsyncRunning(bool running);
    bool isLocal() const;
    bool usesRsync() const;
    FolderSync::CopyPolicy copyPolicy() const;

    /// The local paths, or empty strings for SSH locations.
    QString m_left;
    QString m_right;
    QUrl m_leftUrl;
    QUrl m_rightUrl;
    std::optional<RsyncSync::Endpoint> m_leftEndpoint;
    std::optional<RsyncSync::Endpoint> m_rightEndpoint;
    QString m_endpointError;
    bool m_canCompare = true;
    KSharedConfig::Ptr m_config;
    QList<FolderSyncProfiles::Profile> m_profiles;
    QWidget *m_options;
    QPlainTextEdit *m_include;
    QPlainTextEdit *m_exclude;
    QCheckBox *m_recursive;
    QCheckBox *m_contents;
    QCheckBox *m_links;
    QComboBox *m_saved;
    QComboBox *m_backend;
    QComboBox *m_copyPolicy;
    QSpinBox *m_speedLimit;
    QLabel *m_status;
    QTreeWidget *m_results;
    QWidget *m_planControls;
    QComboBox *m_action;
    QPlainTextEdit *m_report;
    QPushButton *m_compare;
    QPushButton *m_cancel;
    QPushButton *m_sync;
    QFutureWatcher<FolderSync::Comparison> m_watcher;
    QFutureWatcher<FolderSync::ApplyResult> m_syncWatcher;
    QPointer<OperationJob> m_syncJob;
    QPointer<RsyncSync::Process> m_rsync;
    std::shared_ptr<std::atomic_bool> m_canceled;
    std::optional<FolderSync::Comparison> m_comparison;
    QList<FolderSync::Action> m_actions;
};
