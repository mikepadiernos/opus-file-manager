// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "customcommands.h"

#include <KSharedConfig>
#include <QDialog>
#include <QProcess>
#include <QStringConverter>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;

class CustomCommandDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomCommandDialog(const QString &folder, const QStringList &selection, QWidget *parent = nullptr, KSharedConfig::Ptr config = {});
    void reject() override;

private:
    CustomCommands::Command command(QString *error) const;
    void updatePresets();
    void invalidatePreview();
    void preview();
    void run();
    void readOutput();

    QString m_folder;
    QStringList m_selection;
    KSharedConfig::Ptr m_config;
    QList<CustomCommands::Command> m_commands;
    CustomCommands::Prepared m_prepared;
    QWidget *m_options;
    QLineEdit *m_program;
    QPlainTextEdit *m_arguments;
    QPlainTextEdit *m_defaults;
    QComboBox *m_saved;
    QLabel *m_status;
    QTreeWidget *m_preview;
    QPlainTextEdit *m_output;
    QPushButton *m_previewButton;
    QPushButton *m_run;
    QPushButton *m_stop;
    QProcess m_process;
    qsizetype m_outputBytes = 0;
    bool m_outputTruncated = false;
    QStringDecoder m_decoder{QStringDecoder::Utf8};
    QString m_processError;
};
