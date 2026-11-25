// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libraryarchive.h"

#include <QDialog>

class KMessageWidget;
class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

/**
 * Restores the metadata of one library from an export. The export is
 * previewed against the current library, and the user must confirm replacing
 * the current metadata; a copy of the previous library document is kept. The
 * files in the library are never changed.
 */
class LibraryRestoreDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LibraryRestoreDialog(const QString &root, QWidget *parent = nullptr);

    QString source() const;
    void setSource(const QString &path);
    /// Previews restoring from source(); returns false when the export can't be read.
    bool preview();
    /// Restores the previewed export after confirmation; on success emits libraryChanged() and accepts the dialog.
    bool restore();
    const LibraryArchive::Plan &plan() const;
    /// The copy of the previous library document kept by the restore, or an empty string.
    QString backupPath() const;

Q_SIGNALS:
    void libraryChanged(const QString &root);

private:
    void browse();
    void invalidatePreview();
    void updateButtons();
    static QString describe(const LibraryArchive::Change &change);

    QString m_root;
    bool m_readOnly = false;
    LibraryArchive::Plan m_plan;
    QString m_backup;
    QLineEdit *m_source;
    QPushButton *m_preview;
    QLabel *m_details;
    QTreeWidget *m_changes;
    QLabel *m_summary;
    QCheckBox *m_confirm;
    KMessageWidget *m_error;
    QDialogButtonBox *m_buttons;
    QPushButton *m_restore;
};
