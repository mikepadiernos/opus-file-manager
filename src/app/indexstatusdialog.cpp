// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexstatusdialog.h"

#include "indexbackend.h"

#include <KGuiItem>
#include <KLocalizedString>
#include <KMessageBox>

#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeDatabase>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QLabel *makeLabel(const QString &name, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QString listText(const QStringList &items)
{
    return items.isEmpty() ? i18nc("@info no folders", "None") : items.join(QLatin1Char('\n'));
}
}

IndexStatusDialog::IndexStatusDialog(const QString &folder, IndexBackend *backend, QWidget *parent)
    : QDialog(parent)
    , m_folder(folder)
    , m_backend(backend)
    , m_warnings(makeLabel(QStringLiteral("indexWarnings"), this))
    , m_serviceLabel(makeLabel(QStringLiteral("indexService"), this))
    , m_progress(makeLabel(QStringLiteral("indexProgress"), this))
    , m_mode(makeLabel(QStringLiteral("indexMode"), this))
    , m_includes(makeLabel(QStringLiteral("indexIncludes"), this))
    , m_excludes(makeLabel(QStringLiteral("indexExcludes"), this))
    , m_filters(makeLabel(QStringLiteral("indexFilters"), this))
    , m_coverage(makeLabel(QStringLiteral("indexFolderCoverage"), this))
    , m_confirmed(makeLabel(QStringLiteral("indexConfirmed"), this))
    , m_extractors(makeLabel(QStringLiteral("indexExtractors"), this))
    , m_actionStatus(makeLabel(QStringLiteral("indexActionStatus"), this))
    , m_checkCoverage(new QPushButton(i18nc("@action:button", "Check Index Coverage"), this))
    , m_suspend(new QPushButton(this))
    , m_checkUnindexed(new QPushButton(i18nc("@action:button", "Check for Unindexed Files…"), this))
{
    setObjectName(QStringLiteral("indexStatusDialog"));
    setWindowTitle(i18nc("@title:window", "Indexing Status"));
    auto *layout = new QVBoxLayout(this);
    QFont bold = m_warnings->font();
    bold.setBold(true);
    m_warnings->setFont(bold);
    layout->addWidget(m_warnings);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label", "File indexer:"), m_serviceLabel);
    form->addRow(i18nc("@label", "Progress:"), m_progress);
    form->addRow(i18nc("@label", "Indexing:"), m_mode);
    form->addRow(i18nc("@label", "Indexed folders:"), m_includes);
    form->addRow(i18nc("@label", "Excluded folders:"), m_excludes);
    form->addRow(i18nc("@label", "Exclusions:"), m_filters);
    form->addRow(i18nc("@label", "This folder:"), m_coverage);
    form->addRow(i18nc("@label", "In the index:"), m_confirmed);
    form->addRow(i18nc("@label", "Extractors:"), m_extractors);
    layout->addLayout(form);

    auto *note = new QLabel(i18nc("@info",
                                  "Indexed folders show what KDE File Search settings include. Only checking the index confirms which files it "
                                  "contains; new and changed files appear after the indexer reaches them."),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);

    m_checkCoverage->setObjectName(QStringLiteral("indexCheckCoverage"));
    m_checkCoverage->setToolTip(i18nc("@info:tooltip", "Look up the first %1 files of this folder in the index without changing it", SampleLimit));
    m_suspend->setObjectName(QStringLiteral("indexSuspend"));
    m_suspend->setToolTip(i18nc("@info:tooltip", "Pause or resume the shared file indexer for all applications"));
    m_checkUnindexed->setObjectName(QStringLiteral("indexCheckUnindexed"));
    m_checkUnindexed->setToolTip(i18nc("@info:tooltip", "Ask the shared file indexer to find and index files missing from the index"));
    auto *settings = new QPushButton(QIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:button", "Open KDE File Search Settings"), this);
    settings->setObjectName(QStringLiteral("indexOpenSettings"));
    settings->setToolTip(i18nc("@info:tooltip", "Change indexed folders, exclusions and whether indexing is on, for all applications"));
    auto *refreshButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action:button", "Refresh"), this);
    refreshButton->setObjectName(QStringLiteral("indexRefresh"));
    auto *row = new QHBoxLayout;
    row->addWidget(m_checkCoverage);
    row->addWidget(m_suspend);
    row->addWidget(m_checkUnindexed);
    row->addWidget(settings);
    row->addStretch();
    row->addWidget(refreshButton);
    layout->addLayout(row);
    layout->addWidget(m_actionStatus);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_checkCoverage, &QPushButton::clicked, this, &IndexStatusDialog::checkCoverage);
    connect(m_suspend, &QPushButton::clicked, this, &IndexStatusDialog::suspendOrResume);
    connect(m_checkUnindexed, &QPushButton::clicked, this, &IndexStatusDialog::checkUnindexed);
    connect(settings, &QPushButton::clicked, this, &IndexStatusDialog::openSettings);
    connect(refreshButton, &QPushButton::clicked, this, &IndexStatusDialog::refresh);
    if (m_backend) {
        connect(m_backend, &IndexBackend::serviceChanged, this, &IndexStatusDialog::refresh);
    }
    m_confirmed->setText(i18nc("@info", "Not checked."));
    refresh();
}

void IndexStatusDialog::setConfirmation(Confirmation confirmation)
{
    m_confirmation = std::move(confirmation);
}

void IndexStatusDialog::refresh()
{
    if (!m_backend) {
        return;
    }
    m_config = m_backend->config();
    m_service = m_backend->service();
    const QStringList warnings = IndexStatus::warnings(m_config, m_service, m_folder);
    m_warnings->setText(warnings.join(QLatin1Char('\n')));
    m_warnings->setVisible(!warnings.isEmpty());
    m_serviceLabel->setText(IndexStatus::serviceText(m_config, m_service));
    if (m_service.running && IndexStatus::isBusy(m_service.state)) {
        m_progress->setText(m_service.currentFile.isEmpty() ? i18nc("@info", "Working") : i18nc("@info", "Indexing %1", m_service.currentFile));
    } else {
        m_progress->setText(i18nc("@info", "No indexing in progress"));
    }
    if (!m_config.available || !m_config.enabled) {
        m_mode->setText(i18nc("@info", "Off"));
    } else {
        QString mode = m_config.onlyBasic ? i18nc("@info", "On: file names and basic details only") : i18nc("@info", "On: file names, contents and metadata");
        if (m_config.indexHidden) {
            mode = i18nc("@info indexing mode", "%1, including hidden files", mode);
        }
        m_mode->setText(mode);
    }
    m_includes->setText(listText(m_config.includeFolders));
    m_excludes->setText(listText(m_config.excludeFolders));
    m_filters->setText(i18nc("@info file name patterns, file types",
                             "%1, %2",
                             i18np("%1 file name pattern", "%1 file name patterns", m_config.excludeFilters.size()),
                             i18np("%1 file type", "%1 file types", m_config.excludeMimeTypes.size())));
    m_filters->setToolTip((m_config.excludeFilters + m_config.excludeMimeTypes).join(QStringLiteral(", ")));
    m_coverage->setText(m_folder.isEmpty() ? i18nc("@info", "Not a local folder, so it can't be indexed.")
                                           : IndexStatus::coverageText(IndexStatus::classify(m_folder, m_config)));
    m_checkCoverage->setEnabled(!m_folder.isEmpty() && m_config.available && m_config.enabled);
    const bool suspended = m_service.state == static_cast<int>(IndexStatus::State::Suspended);
    m_suspend->setText(suspended ? i18nc("@action:button", "Resume Indexing") : i18nc("@action:button", "Suspend Indexing"));
    m_suspend->setIcon(QIcon::fromTheme(suspended ? QStringLiteral("media-playback-start") : QStringLiteral("media-playback-pause")));
    m_suspend->setEnabled(m_service.running && m_service.state >= 0);
    m_checkUnindexed->setEnabled(m_service.running && !suspended);
}

void IndexStatusDialog::checkCoverage()
{
    if (!m_backend || m_folder.isEmpty()) {
        return;
    }
    const QFileInfoList entries = QDir(m_folder).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
    if (entries.isEmpty()) {
        m_confirmed->setText(i18nc("@info", "This folder has no files to check."));
        m_confirmed->setToolTip({});
        m_extractors->setText({});
        return;
    }
    const QMimeDatabase database;
    int configured = 0;
    int indexed = 0;
    int excluded = 0;
    QStringList missing;
    QStringList types;
    const int sampled = std::min<int>(entries.size(), SampleLimit);
    for (int index = 0; index < sampled; ++index) {
        const QFileInfo &info = entries.at(index);
        const QString type = database.mimeTypeForFile(info).name();
        if (!IndexStatus::classify(info.absoluteFilePath(), m_config, type).configured()) {
            ++excluded;
            continue;
        }
        ++configured;
        types.append(type);
        if (m_backend->isIndexed(info.absoluteFilePath())) {
            ++indexed;
        } else {
            missing.append(info.fileName());
        }
    }
    QStringList text;
    if (configured > 0) {
        text.append(i18nc("@info", "%1 of %2 files that the settings include are in the index.", indexed, configured));
    }
    if (excluded > 0) {
        text.append(i18np("%1 file is excluded by the settings.", "%1 files are excluded by the settings.", excluded));
    }
    if (!missing.isEmpty()) {
        text.append(i18nc("@info", "Missing files may be waiting to be indexed, or the indexer may have skipped them."));
    }
    if (entries.size() > sampled) {
        text.append(i18nc("@info", "Checked the first %1 of %2 files.", sampled, entries.size()));
    }
    m_confirmed->setText(text.join(QLatin1Char(' ')));
    m_confirmed->setToolTip(missing.isEmpty() ? QString() : i18nc("@info:tooltip", "Not in the index: %1", missing.mid(0, 20).join(QStringLiteral(", "))));

    if (m_config.onlyBasic) {
        m_extractors->setText(i18nc("@info", "Not used, because only file names and basic details are indexed."));
        return;
    }
    if (types.isEmpty()) {
        m_extractors->setText(i18nc("@info", "No included files to check."));
        return;
    }
    const QStringList missingTypes = IndexStatus::missingExtractors(types, [this](const QString &type) {
        return m_backend && m_backend->hasExtractor(type);
    });
    m_extractors->setText(missingTypes.isEmpty() ? i18nc("@info", "Installed extractors handle the checked file types.")
                                                 : i18nc("@info",
                                                         "No metadata extractor is installed for %1, so only names and basic details of these files "
                                                         "are indexed.",
                                                         missingTypes.join(QStringLiteral(", "))));
}

void IndexStatusDialog::suspendOrResume()
{
    if (!m_backend) {
        return;
    }
    QString error;
    if (m_service.state == static_cast<int>(IndexStatus::State::Suspended)) {
        report(m_backend->resume(&error), i18nc("@info", "Asked the file indexer to resume."), error);
    } else {
        report(m_backend->suspend(&error), i18nc("@info", "Asked the file indexer to suspend. Results may become stale until it resumes."), error);
    }
}

void IndexStatusDialog::checkUnindexed()
{
    if (!m_backend) {
        return;
    }
    const QString title = i18nc("@title:window", "Check for Unindexed Files");
    const QString text = i18nc("@info",
                               "The shared file indexer will look through the indexed folders for files missing from the index and index them. "
                               "This affects all applications and may take a while.");
    const QString action = i18nc("@action:button", "Check");
    const bool accepted = m_confirmation
        ? m_confirmation(title, text, action)
        : KMessageBox::warningContinueCancel(this, text, title, KGuiItem(action, QStringLiteral("view-refresh"))) == KMessageBox::Continue;
    if (!accepted) {
        return;
    }
    QString error;
    report(m_backend->checkUnindexedFiles(&error), i18nc("@info", "Asked the file indexer to check for unindexed files."), error);
}

void IndexStatusDialog::openSettings()
{
    if (!m_backend) {
        return;
    }
    QString error;
    report(m_backend->openSettings(&error), i18nc("@info", "Requested KDE File Search settings. Changes there affect the shared index."), error);
}

void IndexStatusDialog::report(bool succeeded, const QString &done, const QString &error)
{
    m_actionStatus->setText(succeeded ? done : error);
    refresh();
}
