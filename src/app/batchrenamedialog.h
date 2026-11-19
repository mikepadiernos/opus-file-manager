// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "batchrename.h"

#include <QDialog>
#include <QFutureWatcher>

#include <atomic>
#include <memory>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;

class BatchRenameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchRenameDialog(const QStringList &paths, QWidget *parent = nullptr);
    ~BatchRenameDialog() override;
    void reject() override;

private:
    void preview();
    void apply();
    void invalidatePreview();

    QStringList m_paths;
    BatchRename::Plan m_plan;
    QWidget *m_options;
    QLineEdit *m_pattern;
    QSpinBox *m_startNumber;
    QSpinBox *m_padding;
    QLabel *m_status;
    QTreeWidget *m_results;
    QPushButton *m_preview;
    QPushButton *m_apply;
    QPushButton *m_cancel;
    QFutureWatcher<BatchRename::Plan> m_previewWatcher;
    QFutureWatcher<BatchRename::Result> m_applyWatcher;
    std::shared_ptr<std::atomic_bool> m_canceled;
};
