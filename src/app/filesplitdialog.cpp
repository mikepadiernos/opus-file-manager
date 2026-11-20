// SPDX-License-Identifier: GPL-3.0-or-later

#include "filesplitdialog.h"

#include "settings.h"

#include <KLocalizedString>

#include <QCollator>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>

FileSplitDialog::FileSplitDialog(Mode mode, const QStringList &paths, QWidget *parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_options(new QWidget(this))
    , m_inputs(new QListWidget(m_options))
    , m_destination(new QLineEdit(m_options))
    , m_partSize(new QLineEdit(QString::number(qint64(Settings::splitPartSizeMiB()) * 1024 * 1024), m_options))
    , m_status(new QLabel(this))
    , m_outputs(new QListWidget(this))
    , m_start(new QPushButton(mode == Mode::Split ? i18nc("@action:button", "Split") : i18nc("@action:button", "Combine"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Operation"), this))
{
    setObjectName(QStringLiteral("fileSplitDialog"));
    setWindowTitle(mode == Mode::Split ? i18nc("@title:window", "Split File") : i18nc("@title:window", "Combine Files"));
    resize(650, 540);
    auto *layout = new QVBoxLayout(this);
    auto *optionsLayout = new QVBoxLayout(m_options);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->addWidget(
        new QLabel(mode == Mode::Split ? i18nc("@label", "Source file:") : i18nc("@label", "Input files in order (check and adjust before combining):"),
                   m_options));
    m_inputs->setObjectName(QStringLiteral("fileSplitInputs"));
    QStringList sorted = paths;
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(sorted.begin(), sorted.end(), [&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    });
    m_inputs->addItems(sorted);
    optionsLayout->addWidget(m_inputs);
    if (mode == Mode::Combine) {
        auto *order = new QHBoxLayout;
        auto *up = new QPushButton(i18nc("@action:button", "Move Up"), m_options);
        auto *down = new QPushButton(i18nc("@action:button", "Move Down"), m_options);
        up->setObjectName(QStringLiteral("fileSplitUp"));
        down->setObjectName(QStringLiteral("fileSplitDown"));
        const auto move = [this](int offset) {
            const int row = m_inputs->currentRow();
            const int target = row + offset;
            if (row < 0 || target < 0 || target >= m_inputs->count()) {
                return;
            }
            m_inputs->insertItem(target, m_inputs->takeItem(row));
            m_inputs->setCurrentRow(target);
        };
        connect(up, &QPushButton::clicked, this, [move] {
            move(-1);
        });
        connect(down, &QPushButton::clicked, this, [move] {
            move(1);
        });
        order->addWidget(up);
        order->addWidget(down);
        order->addStretch();
        optionsLayout->addLayout(order);
        auto *warning = new QLabel(i18nc("@info", "Missing, changed or incorrectly ordered parts cannot be detected."), m_options);
        warning->setWordWrap(true);
        optionsLayout->addWidget(warning);
    }
    auto *form = new QFormLayout;
    auto *destinationRow = new QHBoxLayout;
    m_destination->setObjectName(QStringLiteral("fileSplitDestination"));
    if (!sorted.isEmpty()) {
        m_destination->setText(mode == Mode::Split ? sorted.first() : QFileInfo(sorted.first()).absolutePath() + QStringLiteral("/combined"));
    }
    auto *browse = new QPushButton(i18nc("@action:button", "Browse…"), m_options);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this,
                                                          i18nc("@title:window", "Choose Output Path"),
                                                          m_destination->text(),
                                                          QString(),
                                                          nullptr,
                                                          QFileDialog::DontConfirmOverwrite);
        if (!path.isEmpty()) {
            m_destination->setText(path);
        }
    });
    destinationRow->addWidget(m_destination);
    destinationRow->addWidget(browse);
    form->addRow(mode == Mode::Split ? i18nc("@label", "Output prefix:") : i18nc("@label", "Output file:"), destinationRow);
    m_partSize->setObjectName(QStringLiteral("fileSplitPartSize"));
    if (mode == Mode::Split) {
        form->addRow(i18nc("@label", "Part size (bytes):"), m_partSize);
    }
    optionsLayout->addLayout(form);
    optionsLayout->addWidget(new QLabel(i18nc("@info", "Existing output files are never overwritten."), m_options));
    layout->addWidget(m_options);
    m_status->setObjectName(QStringLiteral("fileSplitStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_outputs->setObjectName(QStringLiteral("fileSplitOutputs"));
    layout->addWidget(m_outputs);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_start->setObjectName(QStringLiteral("fileSplitStart"));
    m_cancel->setObjectName(QStringLiteral("fileSplitCancel"));
    m_cancel->setEnabled(false);
    buttons->addButton(m_start, QDialogButtonBox::ActionRole);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &FileSplitDialog::reject);
    connect(m_start, &QPushButton::clicked, this, &FileSplitDialog::start);
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        m_canceled->store(true);
        m_cancel->setEnabled(false);
        m_status->setText(i18nc("@info", "Canceling…"));
    });
    connect(&m_watcher, &QFutureWatcher<FileSplitter::Result>::finished, this, [this] {
        const FileSplitter::Result result = m_watcher.result();
        m_outputs->addItems(result.outputs);
        m_status->setText(result.error.isEmpty() ? i18nc("@info", "Operation finished.")
                                                 : i18nc("@info", "%1 Created files are listed below and may be incomplete.", result.error));
        m_options->setEnabled(true);
        m_start->setEnabled(true);
        m_cancel->setEnabled(false);
    });
}

FileSplitDialog::~FileSplitDialog()
{
    if (m_canceled) {
        m_canceled->store(true);
    }
}

void FileSplitDialog::reject()
{
    if (m_watcher.isRunning()) {
        m_canceled->store(true);
        m_cancel->setEnabled(false);
        m_status->setText(i18nc("@info", "Canceling… Close this window after the operation stops."));
        return;
    }
    QDialog::reject();
}

void FileSplitDialog::start()
{
    bool validSize = false;
    const qint64 size = m_partSize->text().toLongLong(&validSize);
    if (!QDir::isAbsolutePath(m_destination->text()) || m_inputs->count() == 0) {
        m_status->setText(i18nc("@info", "Choose input files and an absolute output path."));
        return;
    }
    if (m_mode == Mode::Split && (!validSize || size <= 0)) {
        m_status->setText(i18nc("@info", "Enter a positive part size in bytes."));
        return;
    }
    QStringList inputs;
    for (int i = 0; i < m_inputs->count(); ++i) {
        inputs.append(m_inputs->item(i)->text());
    }
    const QString destination = m_destination->text();
    const Mode mode = m_mode;
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const auto canceled = m_canceled;
    m_outputs->clear();
    m_status->setText(i18nc("@info", "Working…"));
    m_options->setEnabled(false);
    m_start->setEnabled(false);
    m_cancel->setEnabled(true);
    m_watcher.setFuture(QtConcurrent::run([mode, inputs, destination, size, canceled] {
        const FileSplitter::CancelCheck check = [canceled] {
            return canceled->load();
        };
        return mode == Mode::Split ? FileSplitter::split(inputs.first(), destination, size, check) : FileSplitter::combine(inputs, destination, check);
    }));
}
