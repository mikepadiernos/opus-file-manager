// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cloudsync.h"
#include "cloudsyncrunner.h"
#include "foldersyncprofiles.h"
#include "rclone.h"

#include <KSharedConfig>
#include <QDialog>
#include <QPointer>
#include <QSet>

#include <optional>
#include <utility>

class KUrlRequester;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

/// Edits a two-way synchronization pair.
class CloudSyncPairDialog : public QDialog
{
    Q_OBJECT

public:
    /// @p others are the other pairs, which the pair must not duplicate.
    CloudSyncPairDialog(const CloudSync::Pair &pair,
                        const QList<CloudSync::Pair> &others,
                        KSharedConfig::Ptr config,
                        RcloneMounts *rclone,
                        QWidget *parent = nullptr);

    CloudSync::Pair pair() const;
    /// When the pair is inside another pair and the user chose to exclude it
    /// from that pair: the other pair's identifier and the pattern to exclude.
    std::optional<std::pair<QString, QString>> outerExclusion() const;

    /// Replaces where other synchronization clients are looked for, for tests.
    void setEngineLocations(const CloudSync::EngineLocations &locations);

    void accept() override;

private:
    void updateKind();
    void updateMode();
    void validate();
    void populateAccounts();
    void populateRemotes(const QList<Rclone::Remote> &remotes, const QString &error);

    CloudSync::Pair m_pair;
    QList<CloudSync::Pair> m_others;
    KSharedConfig::Ptr m_config;
    QPointer<RcloneMounts> m_rclone;
    CloudSync::EngineLocations m_engineLocations;
    QList<FolderSyncProfiles::Profile> m_profiles;
    std::optional<std::pair<QString, QString>> m_outerExclusion;
    KUrlRequester *m_local;
    QComboBox *m_kind;
    QLineEdit *m_remote;
    QToolButton *m_accounts;
    QWidget *m_remoteRow;
    QLineEdit *m_rcloneRemote;
    QToolButton *m_remotes;
    QWidget *m_rcloneRow;
    QComboBox *m_mode;
    QCheckBox *m_mirror;
    QComboBox *m_saved;
    QPlainTextEdit *m_include;
    QPlainTextEdit *m_exclude;
    QCheckBox *m_recursive;
    QSpinBox *m_interval;
    QCheckBox *m_paused;
    QLabel *m_error;
    QPushButton *m_ok;
};

/// Shows what a synchronization run would change and lets the user resolve
/// its conflicts before it runs.
class CloudSyncReviewDialog : public QDialog
{
    Q_OBJECT

public:
    CloudSyncReviewDialog(const CloudSync::Pair &pair, const CloudSync::Report &report, QWidget *parent = nullptr);

    /// The chosen resolution of each resolvable conflict, by path.
    QHash<QString, CloudSync::Resolution> resolutions() const;

private:
    QTreeWidget *m_changes;
    QTreeWidget *m_conflicts;
    QList<std::pair<QString, QComboBox *>> m_choices;
};

/// Manages two-way synchronization pairs and shows their status.
class CloudSyncDialog : public QDialog
{
    Q_OBJECT

public:
    CloudSyncDialog(CloudSync::Scheduler *scheduler, KSharedConfig::Ptr config, RcloneMounts *rclone, QWidget *parent = nullptr);

    /// Opens the review of @p id once its preview is ready.
    void review(const QString &id);

private:
    void refresh();
    void updateRow(const QString &id);
    void updateButtons();
    QString currentId() const;
    QTreeWidgetItem *itemFor(const QString &id) const;
    void addPair();
    void editPair();
    void removePair();
    void editPair(const CloudSync::Pair &pair, bool isNew);
    void showReview(const QString &id, const CloudSync::Report &report);
    void showMessage(const QString &message);

    QPointer<CloudSync::Scheduler> m_scheduler;
    KSharedConfig::Ptr m_config;
    QPointer<RcloneMounts> m_rclone;
    QTreeWidget *m_pairs;
    QPushButton *m_add;
    QPushButton *m_edit;
    QPushButton *m_remove;
    QPushButton *m_syncNow;
    QPushButton *m_review;
    QPushButton *m_cancel;
    QPushButton *m_pause;
    QPushButton *m_pauseAccount;
    QCheckBox *m_startAtLogin;
    QCheckBox *m_metered;
    QLabel *m_message;
    /// Pairs whose review opens once their requested preview or run finishes.
    QSet<QString> m_awaitingReview;
    QPointer<CloudSyncReviewDialog> m_reviewDialog;
};
