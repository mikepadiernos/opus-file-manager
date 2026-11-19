// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "metadataeditor.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QMap>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QVBoxLayout;

class MetadataEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetadataEditDialog(const QString &path, QWidget *parent = nullptr);
    ~MetadataEditDialog() override;
    void reject() override;

private:
    void load();
    void preview();
    void apply();
    void invalidate();

    QString m_path;
    MetadataEditor::Snapshot m_snapshot;
    QMap<QString, QString> m_previewValues;
    QMap<QString, QLineEdit *> m_fields;
    QWidget *m_options;
    QVBoxLayout *m_fieldsLayout;
    QLabel *m_status;
    QTreeWidget *m_changes;
    QPushButton *m_preview;
    QPushButton *m_apply;
    QFutureWatcher<MetadataEditor::Snapshot> m_reader;
    QFutureWatcher<QString> m_writer;
    bool m_busy = false;
};
