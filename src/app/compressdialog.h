// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "archivewriter.h"

#include <QDialog>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;

/**
 * Asks how to compress the selection: the archive's name, its format, and
 * whether to write it in the current folder or the other pane's.
 */
class CompressDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @param defaultName the archive name offered, without an extension
     * @param otherPaneAvailable whether the other pane's folder can take the archive
     * @param parent the main window
     */
    CompressDialog(const QString &defaultName,
                   bool otherPaneAvailable,
                   QWidget *parent,
                   ArchiveWriter::Format defaultFormat = ArchiveWriter::Format::Zip,
                   bool otherPaneByDefault = false);

    /// The name typed, without surrounding spaces or an extension.
    QString archiveName() const;
    ArchiveWriter::Format format() const;
    QString password() const;
    /// Whether the archive goes to the other pane's folder.
    bool toOtherPane() const;

private:
    /// Shows the extension for the format and enables OK only for a usable name.
    void updateState();

    QLineEdit *m_name = nullptr;
    QLineEdit *m_password = nullptr;
    QLineEdit *m_passwordConfirmation = nullptr;
    QLabel *m_suffix = nullptr;
    QComboBox *m_format = nullptr;
    QComboBox *m_location = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};
