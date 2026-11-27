// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "indexstatus.h"

#include <QDialog>
#include <QPointer>

#include <functional>

class IndexBackend;
class QLabel;
class QPushButton;

/**
 * Shows the state of KDE's shared file index: the indexer's progress, the
 * indexed and excluded folders, whether a folder is in the configured scope,
 * and on request, how many of its files the index confirms and which file
 * types lack a metadata extractor.
 *
 * It offers KDE's indexing controls, but changes nothing until one is used.
 * It never enables indexing or changes the shared settings itself.
 */
class IndexStatusDialog : public QDialog
{
    Q_OBJECT

public:
    using Confirmation = std::function<bool(const QString &title, const QString &text, const QString &action)>;

    /// Files sampled from the folder to confirm coverage.
    static constexpr int SampleLimit = 200;

    /**
     * @param folder a local folder whose coverage is shown, or empty
     * @param backend stays owned by the caller
     */
    IndexStatusDialog(const QString &folder, IndexBackend *backend, QWidget *parent = nullptr);

    /// Replaces the confirmation question, for tests.
    void setConfirmation(Confirmation confirmation);
    /// Re-reads the settings and the indexer's state.
    void refresh();
    /// Checks a sample of the folder's files against the index and installed extractors.
    void checkCoverage();

private:
    void suspendOrResume();
    void checkUnindexed();
    void openSettings();
    void report(bool succeeded, const QString &done, const QString &error);

    QString m_folder;
    QPointer<IndexBackend> m_backend;
    Confirmation m_confirmation;
    IndexStatus::Config m_config;
    IndexStatus::Service m_service;
    QLabel *m_warnings;
    QLabel *m_serviceLabel;
    QLabel *m_progress;
    QLabel *m_mode;
    QLabel *m_includes;
    QLabel *m_excludes;
    QLabel *m_filters;
    QLabel *m_coverage;
    QLabel *m_confirmed;
    QLabel *m_extractors;
    QLabel *m_actionStatus;
    QPushButton *m_checkCoverage;
    QPushButton *m_suspend;
    QPushButton *m_checkUnindexed;
};
