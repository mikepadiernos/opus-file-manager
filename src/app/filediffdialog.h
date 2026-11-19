// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QProcess;

class FileDiffDialog : public QDialog
{
    Q_OBJECT

public:
    FileDiffDialog(const QString &first, const QString &second, QWidget *parent = nullptr);

private:
    void readOutput();
    void finish(int exitCode, bool normalExit);

    QProcess *m_process;
    QLabel *m_status;
    QPlainTextEdit *m_output;
    QByteArray m_diff;
    QByteArray m_errors;
    bool m_tooLarge = false;
    bool m_failedToStart = false;
};
