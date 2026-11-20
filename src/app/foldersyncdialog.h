// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldersync.h"
#include "foldersyncprofiles.h"

#include <KSharedConfig>
#include <QDialog>
#include <QFutureWatcher>

#include <atomic>
#include <memory>
#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;

class FolderSyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FolderSyncDialog(const QString &left, const QString &right, QWidget *parent = nullptr, KSharedConfig::Ptr config = {});
    ~FolderSyncDialog() override;
    void reject() override;

private:
    FolderSync::Options options() const;
    void setOptions(const FolderSync::Options &options);
    void refreshProfiles();
    void invalidate();
    void compare();
    void updateSelectedAction();
    void planCopies(FolderSync::Action direction);
    void synchronize();

    QString m_left;
    QString m_right;
    KSharedConfig::Ptr m_config;
    QList<FolderSyncProfiles::Profile> m_profiles;
    QWidget *m_options;
    QPlainTextEdit *m_include;
    QPlainTextEdit *m_exclude;
    QCheckBox *m_recursive;
    QCheckBox *m_contents;
    QCheckBox *m_links;
    QComboBox *m_saved;
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
    std::shared_ptr<std::atomic_bool> m_canceled;
    std::optional<FolderSync::Comparison> m_comparison;
    QList<FolderSync::Action> m_actions;
};
