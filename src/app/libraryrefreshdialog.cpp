// SPDX-License-Identifier: GPL-3.0-or-later

#include "libraryrefreshdialog.h"

#include "libraryautorefresh.h"
#include "localizednumbers.h"
#include "taggingsettings.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QIcon>
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
    , m_review(m_buttons->addButton(i18nc("@action:button", "&Review Missing Entries…"), QDialogButtonBox::ActionRole))
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
    m_review->setObjectName(QStringLiteral("libraryReviewMissing"));
    m_review->setIcon(QIcon::fromTheme(QStringLiteral("link")));
    m_review->hide();
    connect(m_review, &QPushButton::clicked, this, [this] {
        Q_EMIT reviewRequested(m_root);
    });
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
    if (m_manual) {
        LibraryAutoRefresh::instance()->endManualRefresh(m_root, false);
    }
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
    m_review->hide();
    m_progressTimer.start();
    if (!m_manual) {
        LibraryAutoRefresh::instance()->beginManualRefresh(m_root);
        m_manual = true;
    }
    m_options = TaggingSettings::scanOptions();
    const QString root = m_root;
    const LibraryScan::Options options = m_options;
    const auto canceled = m_canceled;
    const auto scanned = m_scanned;
    m_watcher.setFuture(QtConcurrent::run([root, options, canceled, scanned] {
        return LibraryScan::scan(
            root,
            options,
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

int LibraryRefreshDialog::missingEntries() const
{
    return m_missingEntries;
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
    const LibraryScan::Applied applied = LibraryScan::apply(m_root, result, m_options);
    if (m_manual) {
        LibraryAutoRefresh::instance()->endManualRefresh(m_root, !result.canceled && applied.saved);
        m_manual = false;
    }
    m_changes = applied.changes;
    m_missingEntries = applied.missing;
    m_message = LibraryScan::summary(result, applied);
    m_status->setText(m_message);
    m_progress->hide();
    m_buttons->button(QDialogButtonBox::Cancel)->hide();
    m_buttons->button(QDialogButtonBox::Close)->show();
    m_review->setVisible(m_missingEntries > 0);
    m_buttons->button(QDialogButtonBox::Close)->setFocus();
    if (applied.saved) {
        Q_EMIT libraryChanged(m_root);
    }
    Q_EMIT refreshFinished();
}
