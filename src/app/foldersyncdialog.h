// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "foldersync.h"
#include "foldersyncprofiles.h"

#include <KSharedConfig>
#include <QDialog>
#include <QFutureWatcher>

#include <atomic>
#include <memory>

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
    QPushButton *m_compare;
    QPushButton *m_cancel;
    QFutureWatcher<FolderSync::Comparison> m_watcher;
    std::shared_ptr<std::atomic_bool> m_canceled;
};
