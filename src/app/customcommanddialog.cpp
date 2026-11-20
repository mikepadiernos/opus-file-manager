// SPDX-License-Identifier: GPL-3.0-or-later

#include "customcommanddialog.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr qsizetype MaxOutputBytes = 256 * 1024;
}

CustomCommandDialog::CustomCommandDialog(const QString &folder, const QStringList &selection, QWidget *parent, KSharedConfig::Ptr config, bool manageOnly)
    : QDialog(parent)
    , m_folder(folder)
    , m_selection(selection)
    , m_manageOnly(manageOnly)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_options(new QWidget(this))
    , m_program(new QLineEdit(m_options))
    , m_arguments(new QPlainTextEdit(m_options))
    , m_defaults(new QPlainTextEdit(m_options))
    , m_saved(new QComboBox(m_options))
    , m_status(new QLabel(this))
    , m_preview(new QTreeWidget(this))
    , m_output(new QPlainTextEdit(this))
    , m_previewButton(new QPushButton(i18nc("@action:button", "Preview"), this))
    , m_run(new QPushButton(i18nc("@action:button", "Run"), this))
    , m_stop(new QPushButton(i18nc("@action:button", "Stop"), this))
{
    setObjectName(QStringLiteral("customCommandDialog"));
    setWindowTitle(manageOnly ? i18nc("@title:window", "Manage Commands") : i18nc("@title:window", "Scripts and Custom Commands"));
    resize(850, 700);
    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(manageOnly ? i18nc("@info", "Manage saved scripts and commands. Changes are saved immediately.")
                                              : i18nc("@info", "Working folder: %1\nSelected items: %2", folder, selection.size()),
                                   this);
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *form = new QFormLayout(m_options);
    form->setContentsMargins(0, 0, 0, 0);
    auto *programRow = new QHBoxLayout;
    m_program->setObjectName(QStringLiteral("commandProgram"));
    auto *browse = new QPushButton(i18nc("@action:button", "Browse…"), m_options);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, i18nc("@title:window", "Choose Executable or Script"), m_folder);
        if (!path.isEmpty()) {
            m_program->setText(path);
        }
    });
    programRow->addWidget(m_program, 1);
    programRow->addWidget(browse);
    form->addRow(i18nc("@label", "Executable or script:"), programRow);
    m_arguments->setObjectName(QStringLiteral("commandArguments"));
    m_arguments->setPlaceholderText(i18nc("@info:placeholder", "One argument per line"));
    m_arguments->setMaximumHeight(m_arguments->fontMetrics().lineSpacing() * 4);
    form->addRow(i18nc("@label", "Arguments:"), m_arguments);
    m_defaults->setObjectName(QStringLiteral("commandDefaults"));
    m_defaults->setPlaceholderText(i18nc("@info:placeholder", "One name=value per line"));
    m_defaults->setMaximumHeight(m_defaults->fontMetrics().lineSpacing() * 3);
    form->addRow(i18nc("@label", "Placeholder defaults:"), m_defaults);
    auto *presetRow = new QHBoxLayout;
    m_saved->setObjectName(QStringLiteral("commandPresets"));
    auto *save = new QPushButton(i18nc("@action:button", "Save…"), m_options);
    auto *load = new QPushButton(i18nc("@action:button", "Load"), m_options);
    auto *remove = new QPushButton(i18nc("@action:button", "Delete"), m_options);
    auto *update = new QPushButton(i18nc("@action:button", "Update Selected…"), m_options);
    save->setObjectName(QStringLiteral("commandSavePreset"));
    load->setObjectName(QStringLiteral("commandLoadPreset"));
    remove->setObjectName(QStringLiteral("commandDeletePreset"));
    update->setObjectName(QStringLiteral("commandUpdatePreset"));
    presetRow->addWidget(m_saved, 1);
    presetRow->addWidget(save);
    presetRow->addWidget(load);
    presetRow->addWidget(remove);
    presetRow->addWidget(update);
    form->addRow(i18nc("@label", "Saved commands:"), presetRow);
    layout->addWidget(m_options);
    auto *help = new QLabel(i18nc("@info",
                                  "Use {path} or %f for the first selected item, {paths} or %F for all selected items, "
                                  "and {dir} or %d for the working folder. Each placeholder must be a whole argument. "
                                  "Custom defaults use {name}. Arguments are not passed through a shell automatically. "
                                  "Saved defaults are plain text; do not save credentials."),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    m_status->setObjectName(QStringLiteral("commandStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setText(i18nc("@info", "Preview the executable and arguments before running."));
    layout->addWidget(m_status);
    m_preview->setObjectName(QStringLiteral("commandPreviewList"));
    m_preview->setVisible(!manageOnly);
    m_preview->setRootIsDecorated(false);
    m_preview->setHeaderLabels({i18nc("@title:column", "Part"), i18nc("@title:column", "Value")});
    m_preview->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_preview);
    m_output->setObjectName(QStringLiteral("commandOutput"));
    m_output->setVisible(!manageOnly);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(i18nc("@info:placeholder", "Command output"));
    layout->addWidget(m_output);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_previewButton->setObjectName(QStringLiteral("commandPreview"));
    m_run->setObjectName(QStringLiteral("commandRun"));
    m_stop->setObjectName(QStringLiteral("commandStop"));
    m_run->setEnabled(false);
    m_stop->setEnabled(false);
    buttons->addButton(m_previewButton, QDialogButtonBox::ActionRole);
    buttons->addButton(m_run, QDialogButtonBox::ActionRole);
    buttons->addButton(m_stop, QDialogButtonBox::ActionRole);
    m_previewButton->setVisible(!manageOnly);
    m_run->setVisible(!manageOnly);
    m_stop->setVisible(!manageOnly);
    layout->addWidget(buttons);

    QString error;
    m_commands = CustomCommands::load(m_config->group(QStringLiteral("CustomCommands")), &error);
    updatePresets();
    if (!error.isEmpty()) {
        m_status->setText(error);
        save->setEnabled(false);
    }
    connect(m_saved, &QComboBox::currentIndexChanged, this, [load, remove, update](int index) {
        load->setEnabled(index >= 0);
        remove->setEnabled(index >= 0);
        update->setEnabled(index >= 0);
    });
    load->setEnabled(m_saved->currentIndex() >= 0);
    remove->setEnabled(m_saved->currentIndex() >= 0);
    update->setEnabled(m_saved->currentIndex() >= 0);
    connect(save, &QPushButton::clicked, this, [this] {
        QString error;
        auto value = command(&error);
        if (error.isEmpty()) {
            error = CustomCommands::validate(value);
        }
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        bool accepted = false;
        value.name = QInputDialog::getText(this, i18nc("@title:window", "Save Command"), i18nc("@label", "Name:"), QLineEdit::Normal, {}, &accepted).trimmed();
        if (!accepted) {
            return;
        }
        const auto group = m_config->group(QStringLiteral("CustomCommands"));
        auto commands = CustomCommands::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        commands.append(value);
        if (!CustomCommands::save(group, commands, &error)) {
            m_status->setText(error);
            return;
        }
        m_commands = commands;
        updatePresets();
        m_saved->setCurrentIndex(m_commands.size() - 1);
        m_status->setText(i18nc("@info", "Command saved."));
    });
    connect(load, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_commands.size()) {
            return;
        }
        const QString name = m_commands.at(index).name;
        QString error;
        const auto commands = CustomCommands::load(m_config->group(QStringLiteral("CustomCommands")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto it = std::find_if(commands.cbegin(), commands.cend(), [&name](const auto &command) {
            return command.name == name;
        });
        if (it == commands.cend()) {
            m_commands = commands;
            updatePresets();
            m_status->setText(i18nc("@info", "This saved command no longer exists."));
            return;
        }
        m_program->setText(it->program);
        m_arguments->setPlainText(it->arguments.join(QLatin1Char('\n')));
        QStringList defaults;
        for (auto entry = it->defaults.cbegin(); entry != it->defaults.cend(); ++entry) {
            defaults.append(entry.key() + QLatin1Char('=') + entry.value());
        }
        m_defaults->setPlainText(defaults.join(QLatin1Char('\n')));
        invalidatePreview();
        m_status->setText(i18nc("@info", "Command loaded. Review and preview it before running."));
    });
    connect(update, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_commands.size()) {
            return;
        }
        QString error;
        auto value = command(&error);
        if (error.isEmpty()) {
            error = CustomCommands::validate(value);
        }
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto original = m_commands.at(index);
        if (QMessageBox::question(this,
                                  i18nc("@title:window", "Update Command"),
                                  i18nc("@info", "Replace the saved command “%1” with the current command?", original.name),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        const auto group = m_config->group(QStringLiteral("CustomCommands"));
        auto commands = CustomCommands::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        auto it = std::find_if(commands.begin(), commands.end(), [&original](const auto &item) {
            return item.name == original.name;
        });
        if (it == commands.end() || it->program != original.program || it->arguments != original.arguments || it->defaults != original.defaults) {
            m_status->setText(i18nc("@info", "This command changed elsewhere. Close and reopen the dialog."));
            return;
        }
        value.name = original.name;
        *it = value;
        if (!CustomCommands::save(group, commands, &error)) {
            m_status->setText(error);
            return;
        }
        m_commands = commands;
        updatePresets();
        m_saved->setCurrentIndex(index);
        m_status->setText(i18nc("@info", "Command updated."));
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_commands.size()) {
            return;
        }
        const QString name = m_commands.at(index).name;
        if (QMessageBox::question(this,
                                  i18nc("@title:window", "Delete Command"),
                                  i18nc("@info", "Delete the saved command “%1”?", name),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        QString error;
        const auto group = m_config->group(QStringLiteral("CustomCommands"));
        auto commands = CustomCommands::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto it = std::find_if(commands.begin(), commands.end(), [&name](const auto &command) {
            return command.name == name;
        });
        if (it == commands.end()) {
            m_commands = commands;
            updatePresets();
            m_status->setText(i18nc("@info", "This saved command no longer exists."));
            return;
        }
        commands.erase(it);
        if (!CustomCommands::save(group, commands, &error)) {
            m_status->setText(error);
            return;
        }
        m_commands = commands;
        updatePresets();
        m_status->setText(i18nc("@info", "Saved command deleted."));
    });
    connect(m_program, &QLineEdit::textChanged, this, &CustomCommandDialog::invalidatePreview);
    connect(m_arguments, &QPlainTextEdit::textChanged, this, &CustomCommandDialog::invalidatePreview);
    connect(m_defaults, &QPlainTextEdit::textChanged, this, &CustomCommandDialog::invalidatePreview);
    connect(m_previewButton, &QPushButton::clicked, this, &CustomCommandDialog::preview);
    connect(m_run, &QPushButton::clicked, this, &CustomCommandDialog::run);
    connect(buttons, &QDialogButtonBox::rejected, this, &CustomCommandDialog::reject);
    connect(m_stop, &QPushButton::clicked, this, [this] {
        const qint64 pid = m_process.processId();
        m_process.terminate();
        m_stop->setEnabled(false);
        m_status->setText(i18nc("@info", "Stopping the command; child processes may continue."));
        QTimer::singleShot(3000, this, [this, pid] {
            if (pid != 0 && m_process.state() != QProcess::NotRunning && m_process.processId() == pid) {
                m_process.kill();
            }
        });
    });
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::started, this, [this] {
        m_stop->setEnabled(true);
        m_status->setText(i18nc("@info", "Command running."));
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &CustomCommandDialog::readOutput);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        m_processError = m_process.errorString();
        if (error == QProcess::FailedToStart) {
            m_options->setEnabled(true);
            m_previewButton->setEnabled(true);
            m_stop->setEnabled(false);
            m_status->setText(i18nc("@info", "Could not start command: %1", m_process.errorString()));
        } else {
            m_status->setText(i18nc("@info", "Command error: %1", m_process.errorString()));
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus exitStatus) {
        readOutput();
        m_options->setEnabled(true);
        m_previewButton->setEnabled(true);
        m_stop->setEnabled(false);
        m_status->setText(exitStatus == QProcess::CrashExit ? i18nc("@info", "The command crashed or was stopped.")
                                                            : i18nc("@info", "The command exited with code %1.", code));
        if (m_outputTruncated) {
            m_status->setText(m_status->text() + QLatin1Char(' ') + i18nc("@info", "Output was truncated."));
        }
        if (!m_processError.isEmpty()) {
            m_status->setText(m_status->text() + QLatin1Char(' ') + m_processError);
        }
    });
}

void CustomCommandDialog::reject()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_status->setText(i18nc("@info", "Stop the command and wait for it to finish before closing."));
        return;
    }
    QDialog::reject();
}

CustomCommands::Command CustomCommandDialog::command(QString *error) const
{
    error->clear();
    CustomCommands::Command value;
    value.program = m_program->text();
    if (!m_arguments->toPlainText().isEmpty()) {
        value.arguments = m_arguments->toPlainText().split(QLatin1Char('\n'));
    }
    for (const QString &line : m_defaults->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const qsizetype separator = line.indexOf(QLatin1Char('='));
        const QString key = line.left(separator).trimmed();
        if (separator <= 0 || key.isEmpty() || value.defaults.contains(key)) {
            *error = i18nc("@info", "Use a unique name=value on each default line.");
            return value;
        }
        value.defaults.insert(key, line.mid(separator + 1));
    }
    return value;
}

void CustomCommandDialog::updatePresets()
{
    m_saved->clear();
    for (const auto &value : std::as_const(m_commands)) {
        m_saved->addItem(value.name);
    }
}

void CustomCommandDialog::invalidatePreview()
{
    m_prepared = {};
    m_preview->clear();
    m_run->setEnabled(false);
    m_status->setText(i18nc("@info", "Preview the updated command before running."));
}

void CustomCommandDialog::preview()
{
    if (m_manageOnly) {
        return;
    }
    QString error;
    const auto value = command(&error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    m_prepared = CustomCommands::prepare(value, m_folder, m_selection);
    m_preview->clear();
    if (!m_prepared.error.isEmpty()) {
        m_run->setEnabled(false);
        m_status->setText(m_prepared.error);
        return;
    }
    new QTreeWidgetItem(m_preview, {i18nc("@item", "Working folder"), m_prepared.workingDirectory});
    new QTreeWidgetItem(m_preview, {i18nc("@item", "Executable"), m_prepared.program});
    for (qsizetype i = 0; i < m_prepared.arguments.size(); ++i) {
        new QTreeWidgetItem(m_preview, {i18nc("@item", "Argument %1", i + 1), m_prepared.arguments.at(i)});
    }
    m_run->setEnabled(true);
    m_status->setText(i18nc("@info", "Review the executable and each argument before running."));
}

void CustomCommandDialog::run()
{
    if (m_manageOnly) {
        return;
    }
    if (!m_prepared.error.isEmpty() || m_prepared.program.isEmpty() || m_process.state() != QProcess::NotRunning) {
        return;
    }
    if (QMessageBox::question(this,
                              i18nc("@title:window", "Run Command"),
                              i18nc("@info",
                                    "Run the previewed command? The program can modify files or execute code. "
                                    "It may interpret arguments as code; never pass untrusted paths to a code-evaluating argument."),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    m_output->clear();
    m_outputBytes = 0;
    m_outputTruncated = false;
    m_decoder.resetState();
    m_processError.clear();
    m_options->setEnabled(false);
    m_previewButton->setEnabled(false);
    m_run->setEnabled(false);
    m_stop->setEnabled(false);
    m_status->setText(i18nc("@info", "Starting command…"));
    m_process.setWorkingDirectory(m_prepared.workingDirectory);
    m_process.start(m_prepared.program, m_prepared.arguments);
}

void CustomCommandDialog::readOutput()
{
    const QByteArray output = m_process.readAllStandardOutput();
    const qsizetype available = std::max(qsizetype(0), MaxOutputBytes - m_outputBytes);
    if (output.size() > available) {
        m_outputTruncated = true;
    }
    const QByteArray kept = output.first(std::min(available, output.size()));
    m_outputBytes += kept.size();
    if (!kept.isEmpty()) {
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(m_decoder.decode(kept));
    }
}
