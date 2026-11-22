// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "workspacecatalog.h"

#include <QDialog>
#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;

class WorkspaceDialog : public QDialog
{
    Q_OBJECT

public:
    using CaptureLayout = std::function<bool(WorkspaceCatalog::Layout &, QString &)>;
    using ApplyLayout = std::function<bool(const WorkspaceCatalog::Layout &, QString &)>;

    /// Manages profiles in group and applies their layouts using the supplied callbacks.
    WorkspaceDialog(KConfigGroup group, CaptureLayout capture, ApplyLayout apply, const QString &currentId, QWidget *parent = nullptr);

Q_SIGNALS:
    void currentWorkspaceChanged(const QString &id);

private:
    void reload(const QString &selectedId = {});
    QString selectedId() const;
    bool commit(const WorkspaceCatalog::Catalog &updated);
    void createWorkspace();
    void renameWorkspace();
    void duplicateWorkspace();
    void deleteWorkspace();
    void activateWorkspace();
    void saveLayout();

    KConfigGroup m_group;
    CaptureLayout m_capture;
    ApplyLayout m_apply;
    WorkspaceCatalog::Catalog m_catalog;
    QString m_currentId;
    QListWidget *m_list = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_label = nullptr;
    QLabel *m_status = nullptr;
};
