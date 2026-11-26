// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mountbackend.h"

#include <QDialog>
#include <QUrl>

#include <optional>

class KMessageWidget;
class QCheckBox;
class QPushButton;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

/// Lists mounted and fstab-defined file systems with their size and usage,
/// and opens, mounts, unmounts or ejects them, like Krusader's MountMan.
class MountManagerDialog : public QDialog
{
    Q_OBJECT

public:
    enum Column {
        NameColumn,
        StateColumn,
        DeviceColumn,
        TypeColumn,
        MountPointColumn,
        SizeColumn,
        FreeColumn,
        UsageColumn,
    };

    explicit MountManagerDialog(MountBackend *backend, QWidget *parent = nullptr);

    /// The id of the selected file system, or an empty string.
    QString currentId() const;
    void selectDevice(const QString &id);

    /// Opens the selected file system, or mounts it first and opens it once mounted.
    void activateCurrent();
    void startOperation(MountOperation operation);

Q_SIGNALS:
    /// Asks the main window to open a mounted file system in the active pane.
    void openRequested(const QUrl &url);

private:
    void populate();
    void updateButtons();
    void onFinished(const QString &id, MountOperation operation, const QString &error, const QString &mountPoint);
    std::optional<MountDevice> deviceById(const QString &id) const;

    MountBackend *m_backend;
    KMessageWidget *m_message;
    QCheckBox *m_removableOnly;
    QTreeWidget *m_tree;
    QPushButton *m_open;
    QPushButton *m_mount;
    QPushButton *m_unmount;
    QPushButton *m_eject;
    QPushButton *m_refresh;
    QPushButton *m_partitionManager;
    QTimer *m_poll;
    QString m_openAfterMount;
};
