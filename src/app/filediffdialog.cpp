// SPDX-License-Identifier: GPL-3.0-or-later

#include "filediffdialog.h"

#include "settings.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QVBoxLayout>

namespace
{
constexpr qsizetype MaxOutput = 4 * 1024 * 1024;
}

FileDiffDialog::FileDiffDialog(const QString &first, const QString &second, QWidget *parent)
    : QDialog(parent)
    , m_process(new QProcess(this))
    , m_status(new QLabel(i18nc("@info", "Comparing files…"), this))
    , m_output(new QPlainTextEdit(this))
{
    setObjectName(QStringLiteral("fileDiffDialog"));
    setWindowTitle(i18nc("@title:window", "Compare Files"));
    resize(800, 600);
    auto *layout = new QVBoxLayout(this);
    auto *paths = new QLabel(i18nc("@info", "First: %1\nSecond: %2", first, second), this);
    paths->setTextFormat(Qt::PlainText);
    paths->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(paths);
    m_status->setObjectName(QStringLiteral("fileDiffStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    m_output->setObjectName(QStringLiteral("fileDiffOutput"));
    m_output->setReadOnly(true);
    auto *wrap = new QCheckBox(i18nc("@option:check", "Wrap lines"), this);
    wrap->setObjectName(QStringLiteral("fileDiffWrapLines"));
    wrap->setChecked(Settings::fileDiffWrapLines());
    m_output->setLineWrapMode(wrap->isChecked() ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    connect(wrap, &QCheckBox::toggled, this, [this](bool enabled) {
        m_output->setLineWrapMode(enabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    });
    layout->addWidget(wrap);
    layout->addWidget(m_output);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);

    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    m_process->setProcessEnvironment(environment);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &FileDiffDialog::readOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &FileDiffDialog::readOutput);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_failedToStart = true;
            m_status->setText(i18nc("@info", "Could not start diff: %1", m_process->errorString()));
        }
    });
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        finish(exitCode, status == QProcess::NormalExit);
    });
    m_process->start(QStringLiteral("diff"), {QStringLiteral("-u"), QStringLiteral("--"), first, second});
}

void FileDiffDialog::readOutput()
{
    if (m_tooLarge) {
        m_process->readAllStandardOutput();
        m_process->readAllStandardError();
        return;
    }
    m_diff += m_process->readAllStandardOutput();
    m_errors += m_process->readAllStandardError();
    if (m_diff.size() > MaxOutput || m_errors.size() > MaxOutput) {
        m_tooLarge = true;
        m_diff.clear();
        m_errors.clear();
        m_process->kill();
    }
}

void FileDiffDialog::finish(int exitCode, bool normalExit)
{
    readOutput();
    if (m_failedToStart) {
        return;
    }
    if (m_tooLarge) {
        m_status->setText(i18nc("@info", "The comparison output exceeded 4 MiB and was stopped."));
    } else if (!normalExit || exitCode > 1) {
        const QString error = QString::fromLocal8Bit(m_errors).trimmed();
        m_status->setText(error.isEmpty() ? i18nc("@info", "The comparison failed.") : i18nc("@info", "The comparison failed: %1", error));
    } else if (exitCode == 0) {
        m_status->setText(i18nc("@info", "The files are identical."));
    } else {
        m_status->setText(i18nc("@info", "The files differ."));
        m_output->setPlainText(QString::fromUtf8(m_diff));
    }
}
