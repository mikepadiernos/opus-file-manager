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
#include <QUrl>
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
        const int count = m_scanned->load();
        if (m_importWatcher.isRunning()) {
            m_progress->setValue(count);
            if (m_job) {
                m_job->setProgress(qulonglong(count), qulonglong(m_importFiles));
            }
            m_status->setText(i18ncp("@info",
                                     "Reading the embedded metadata of one new item: %2 read…",
                                     "Reading the embedded metadata of %1 new items: %2 read…",
                                     m_importFiles,
                                     LocalizedNumbers::shape(QString::number(count))));
            return;
        }
        m_status->setText(i18ncp("@info", "Scanned one item…", "Scanned %2 items…", count, LocalizedNumbers::shape(QString::number(count))));
    });
    connect(&m_watcher, &QFutureWatcher<LibraryScan::Result>::finished, this, &LibraryRefreshDialog::finish);
    connect(&m_importWatcher, &QFutureWatcher<QList<EmbeddedMetadata::Reading>>::finished, this, &LibraryRefreshDialog::finishImport);
    resize(460, sizeHint().height());
}

LibraryRefreshDialog::~LibraryRefreshDialog()
{
    m_canceled->store(true);
    m_watcher.waitForFinished();
    m_importWatcher.waitForFinished();
    if (m_job) {
        m_job->cancel();
    }
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
    m_importedItems = 0;
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
    if (m_job) {
        m_job->cancel();
    }
    m_job = OperationJob::track(new OperationJob(i18nc("@title job", "Refreshing tag library")));
    m_job->setSource(QUrl::fromLocalFile(m_root));
    m_job->setCancelHandler([dialog = QPointer<LibraryRefreshDialog>(this)] {
        if (dialog) {
            dialog->cancel();
        }
    });
}

void LibraryRefreshDialog::cancel()
{
    m_canceled->store(true);
    m_status->setText(i18nc("@info", "Canceling…"));
}

bool LibraryRefreshDialog::isRunning() const
{
    return m_watcher.isRunning() || m_importWatcher.isRunning();
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

int LibraryRefreshDialog::importedItems() const
{
    return m_importedItems;
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
    if (applied.saved) {
        Q_EMIT libraryChanged(m_root);
    }
    if (applied.saved && !result.canceled && !m_changes.addedFiles.isEmpty() && TaggingSettings::importsEmbeddedMetadataOnRefresh()) {
        startImport(m_changes.addedFiles);
        return;
    }
    showFinished();
}

void LibraryRefreshDialog::startImport(const QStringList &files)
{
    m_mappings = TaggingSettings::embeddedMetadataMappings();
    m_keys = TaggingSettings::embeddedMetadataKeys();
    m_policy = TaggingSettings::embeddedMetadataPolicy();
    m_tagColor = TaggingSettings::defaultTagColor();
    m_importFiles = int(files.size());
    m_scanned->store(0);
    m_progress->setRange(0, m_importFiles);
    m_progress->setValue(0);
    m_status->setText(i18nc("@info", "Reading embedded metadata…"));
    m_progressTimer.start();
    const QString root = m_root;
    const QStringList keys = MetadataImport::mappedKeys(m_mappings, m_keys);
    const auto canceled = m_canceled;
    const auto read = m_scanned;
    m_importWatcher.setFuture(QtConcurrent::run([root, files, keys, canceled, read] {
        return MetadataImport::readFiles(
            root,
            files,
            keys,
            [canceled] {
                return canceled->load();
            },
            [read](int count) {
                read->store(count);
            });
    }));
}

void LibraryRefreshDialog::finishImport()
{
    m_progressTimer.stop();
    if (m_canceled->load()) {
        m_message += QLatin1Char('\n') + i18nc("@info", "Embedded metadata import canceled; nothing was imported.");
    } else {
        MetadataImport::Result result;
        QString error;
        if (MetadataImport::importReadings(m_root, m_importWatcher.result(), m_mappings, m_keys, m_policy, m_tagColor, &result, &error)) {
            m_importedItems = result.items;
            m_message +=
                QLatin1Char('\n') + i18ncp("@info", "Imported embedded metadata for one item.", "Imported embedded metadata for %1 items.", result.items);
            if (result.items > 0) {
                Q_EMIT libraryChanged(m_root);
            }
        } else {
            m_message += QLatin1Char('\n') + i18nc("@info %1 is the reason", "Could not import embedded metadata: %1", error);
        }
    }
    showFinished();
}

void LibraryRefreshDialog::showFinished()
{
    m_status->setText(m_message);
    m_progress->hide();
    m_buttons->button(QDialogButtonBox::Cancel)->hide();
    m_buttons->button(QDialogButtonBox::Close)->show();
    m_review->setVisible(m_missingEntries > 0);
    m_buttons->button(QDialogButtonBox::Close)->setFocus();
    if (m_job) {
        if (m_canceled->load()) {
            m_job->cancel();
        } else {
            m_job->finish();
        }
    }
    Q_EMIT refreshFinished();
}
