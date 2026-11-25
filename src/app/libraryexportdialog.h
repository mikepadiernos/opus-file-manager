// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "taglibrary.h"

#include <QDialog>

class KMessageWidget;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;

/**
 * Exports the metadata of one library to a JSON file. The dialog summarizes
 * what is exported and where before anything is written; it never writes the
 * library or its files, and only replaces earlier exports.
 */
class LibraryExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LibraryExportDialog(const QString &root, QWidget *parent = nullptr);

    QString destination() const;
    void setDestination(const QString &path);
    /// Writes the export; on success emits exported() and accepts the dialog.
    bool exportLibrary();

Q_SIGNALS:
    void exported(const QString &path);

private:
    void browse();
    void showSummary();
    void updateDestination();

    QString m_root;
    TagLibrary::Library m_library;
    QLabel *m_summary;
    QLineEdit *m_destination;
    QLabel *m_destinationStatus;
    KMessageWidget *m_error;
    QDialogButtonBox *m_buttons;
    QPushButton *m_export;
};
