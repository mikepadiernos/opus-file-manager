// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "batchrename.h"
#include "batchrenamepresets.h"

#include <KSharedConfig>
#include <QDialog>
#include <QFutureWatcher>

#include <atomic>
#include <memory>

class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;

class BatchRenameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchRenameDialog(const QStringList &paths, QWidget *parent = nullptr, KSharedConfig::Ptr config = {}, bool manageOnly = false);
    ~BatchRenameDialog() override;
    void reject() override;

private:
    void preview();
    void apply();
    void invalidatePreview();
    BatchRename::Rule rule() const;
    void setRule(const BatchRename::Rule &rule);
    void updatePresets();

    QStringList m_paths;
    BatchRename::Plan m_plan;
    KSharedConfig::Ptr m_config;
    QList<BatchRenamePresets::Preset> m_presets;
    QWidget *m_options;
    QComboBox *m_mode;
    QLineEdit *m_pattern;
    QLineEdit *m_replacement;
    QSpinBox *m_startNumber;
    QSpinBox *m_padding;
    QComboBox *m_saved;
    QLabel *m_status;
    QTreeWidget *m_results;
    QPushButton *m_preview;
    QPushButton *m_apply;
    QPushButton *m_cancel;
    QFutureWatcher<BatchRename::Plan> m_previewWatcher;
    QFutureWatcher<BatchRename::Result> m_applyWatcher;
    std::shared_ptr<std::atomic_bool> m_canceled;
};
