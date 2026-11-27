// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "archivewriter.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/**
 * Asks how to compress the selection: the archive's name, its format,
 * whether to split it into volumes, and whether to write it in the current
 * folder or the other pane's. When Ark is installed, it can also hand the
 * selection to Ark's own Compress dialog for options KArchive lacks.
 */
class CompressDialog : public QDialog
{
    Q_OBJECT

public:
    /// The exec() result when the user chose to compress with Ark instead.
    static constexpr int UseArk = QDialog::Accepted + 1;

    /**
     * @param defaultName the archive name offered, without an extension
     * @param otherPaneAvailable whether the other pane's folder can take the archive
     * @param parent the main window
     * @param arkAvailable whether to offer Ark's Compress dialog
     */
    CompressDialog(const QString &defaultName,
                   bool otherPaneAvailable,
                   QWidget *parent,
                   ArchiveWriter::Format defaultFormat = ArchiveWriter::Format::Zip,
                   bool otherPaneByDefault = false,
                   bool arkAvailable = false);

    /// The name typed, without surrounding spaces or an extension.
    QString archiveName() const;
    ArchiveWriter::Format format() const;
    QString password() const;
    /// Whether the archive goes to the other pane's folder.
    bool toOtherPane() const;
    /// The size of each volume in bytes, or 0 to keep a single archive.
    qint64 volumeSize() const;

private:
    /// Shows the extension for the format and enables OK only for a usable name.
    void updateState();

    QLineEdit *m_name = nullptr;
    QLineEdit *m_password = nullptr;
    QLineEdit *m_passwordConfirmation = nullptr;
    QLabel *m_suffix = nullptr;
    QComboBox *m_format = nullptr;
    QCheckBox *m_split = nullptr;
    QSpinBox *m_volumeSize = nullptr;
    QComboBox *m_location = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QPushButton *m_ark = nullptr;
};
