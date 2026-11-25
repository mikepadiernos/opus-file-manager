// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryrefreshdialog.h"

#include "localizednumbers.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrentRun>

LibraryRefreshDialog::LibraryRefreshDialog(const QString &root, QWidget *parent)
    : QDialog(parent)
    , m_root(root)
    , m_status(new QLabel(this))
    , m_progress(new QProgressBar(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Close, this))
    , m_canceled(std::make_shared<std::atomic_bool>(false))
    , m_scanned(std::make_shared<std::atomic_int>(0))
{
    setWindowTitle(i18nc("@title:window", "Refresh Library"));
    setObjectName(QStringLiteral("libraryRefreshDialog"));
    m_status->setObjectName(QStringLiteral("libraryRefreshStatus"));
    m_status->setWordWrap(true);
    m_progress->setObjectName(QStringLiteral("libraryRefreshProgress"));
    m_progress->setRange(0, 0);
    m_progress->setAccessibleName(i18nc("@info:accessible", "Library refresh progress"));
    auto *layout = new QVBoxLayout(this);
    auto *rootLabel = new QLabel(i18nc("@info", "Library: %1", root), this);
    rootLabel->setWordWrap(true);
    rootLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(rootLabel);
    layout->addWidget(m_progress);
    layout->addWidget(m_status);
    layout->addWidget(m_buttons);
    m_buttons->button(QDialogButtonBox::Close)->hide();
    connect(m_buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &LibraryRefreshDialog::cancel);
    connect(m_buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::accept);
    m_progressTimer.setInterval(100);
    connect(&m_progressTimer, &QTimer::timeout, this, [this] {
        m_status->setText(
            i18ncp("@info", "Scanned one item…", "Scanned %2 items…", m_scanned->load(), LocalizedNumbers::shape(QString::number(m_scanned->load()))));
    });
    connect(&m_watcher, &QFutureWatcher<LibraryScan::Result>::finished, this, &LibraryRefreshDialog::finish);
    resize(460, sizeHint().height());
}

LibraryRefreshDialog::~LibraryRefreshDialog()
{
    m_canceled->store(true);
    m_watcher.waitForFinished();
}

void LibraryRefreshDialog::start()
{
    if (isRunning()) {
        return;
    }
    m_canceled->store(false);
    m_scanned->store(0);
    m_message.clear();
    m_changes = {};
    m_status->setText(i18nc("@info", "Scanning…"));
    m_progress->setRange(0, 0);
    m_progress->show();
    m_buttons->button(QDialogButtonBox::Cancel)->show();
    m_buttons->button(QDialogButtonBox::Close)->hide();
    m_progressTimer.start();
    const QString root = m_root;
    const auto canceled = m_canceled;
    const auto scanned = m_scanned;
    m_watcher.setFuture(QtConcurrent::run([root, canceled, scanned] {
        return LibraryScan::scan(
            root,
            [canceled] {
                return canceled->load();
            },
            [scanned](int count, const QString &) {
                scanned->store(count);
            });
    }));
}

void LibraryRefreshDialog::cancel()
{
    m_canceled->store(true);
    m_status->setText(i18nc("@info", "Canceling…"));
}

bool LibraryRefreshDialog::isRunning() const
{
    return m_watcher.isRunning();
}

QString LibraryRefreshDialog::root() const
{
    return m_root;
}

TagLibrary::ScanChanges LibraryRefreshDialog::changes() const
{
    return m_changes;
}

QString LibraryRefreshDialog::message() const
{
    return m_message;
}

void LibraryRefreshDialog::reject()
{
    if (isRunning()) {
        cancel();
        return;
    }
    QDialog::reject();
}

void LibraryRefreshDialog::finish()
{
    m_progressTimer.stop();
    const LibraryScan::Result result = m_watcher.result();
    QString error;
    TagLibrary::ScanChanges changes;
    const bool complete = !result.canceled && result.errors.isEmpty();
    const bool saved = !result.items.isEmpty() || complete ? TagLibrary::update(
                                                                 m_root,
                                                                 [&](TagLibrary::Library &library, QString *) {
                                                                     changes = library.applyScan(result.items, complete);
                                                                     return true;
                                                                 },
                                                                 &error)
                                                           : true;
    m_changes = changes;
    QStringList lines;
    if (!saved) {
        lines.append(error);
    } else {
        lines.append(i18ncp("@info", "Added one entry.", "Added %1 entries.", changes.added));
        if (changes.updated) {
            lines.append(i18ncp("@info", "Updated one entry.", "Updated %1 entries.", changes.updated));
        }
        if (changes.found) {
            lines.append(i18ncp("@info", "Found one missing entry again.", "Found %1 missing entries again.", changes.found));
        }
        if (changes.missing) {
            lines.append(i18ncp("@info",
                                "One entry is missing; its tags and fields are kept until you review it.",
                                "%1 entries are missing; their tags and fields are kept until you review them.",
                                changes.missing));
        }
    }
    if (result.canceled) {
        lines.prepend(i18nc("@info", "Refresh canceled. Items found so far were recorded; no entries were marked missing."));
    } else if (!result.errors.isEmpty()) {
        lines.prepend(i18nc("@info", "Some folders couldn't be read, so no entries were marked missing."));
        lines.append(result.errors.mid(0, 5));
    }
    if (result.skipped) {
        lines.append(i18ncp("@info",
                            "Skipped one item whose name isn't valid in the system encoding.",
                            "Skipped %1 items whose names aren't valid in the system encoding.",
                            result.skipped));
    }
    m_message = lines.join(QLatin1Char('\n'));
    m_status->setText(m_message);
    m_progress->hide();
    m_buttons->button(QDialogButtonBox::Cancel)->hide();
    m_buttons->button(QDialogButtonBox::Close)->show();
    m_buttons->button(QDialogButtonBox::Close)->setFocus();
    if (saved) {
        Q_EMIT libraryChanged(m_root);
    }
    Q_EMIT refreshFinished();
}
