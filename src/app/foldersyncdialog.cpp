// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersyncdialog.h"

#include "remotemounts.h"
#include "settings.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>

namespace
{
QString differenceText(FolderSync::Difference difference)
{
    using Difference = FolderSync::Difference;
    switch (difference) {
    case Difference::LeftOnly:
        return i18nc("@item", "Only in first folder");
    case Difference::RightOnly:
        return i18nc("@item", "Only in second folder");
    case Difference::Type:
        return i18nc("@item", "Different types");
    case Difference::Size:
        return i18nc("@item", "Different sizes");
    case Difference::Time:
        return i18nc("@item", "Different modification times");
    case Difference::MetadataEqual:
        return i18nc("@item", "Matching size and time");
    case Difference::ContentEqual:
        return i18nc("@item", "Matching contents");
    case Difference::ContentDifferent:
        return i18nc("@item", "Different contents");
    case Difference::LinkEqual:
        return i18nc("@item", "Matching link targets");
    case Difference::LinkDifferent:
        return i18nc("@item", "Different link targets");
    case Difference::Directory:
        return i18nc("@item", "Folders");
    case Difference::Unsupported:
        return i18nc("@item", "Unsupported item");
    }
    return {};
}

QStringList patterns(const QPlainTextEdit *edit)
{
    return edit->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

QString actionText(FolderSync::Action action, const FolderSync::Row &row)
{
    using Action = FolderSync::Action;
    switch (action) {
    case Action::Skip:
        return i18nc("@item", "Skip");
    case Action::CopyToLeft:
        if (row.right.kind == FolderSync::Kind::Directory) {
            return i18nc("@item", "Create folder in first");
        }
        return row.left.kind == FolderSync::Kind::Missing ? i18nc("@item", "Copy second to first") : i18nc("@item", "Overwrite in first from second");
    case Action::CopyToRight:
        if (row.left.kind == FolderSync::Kind::Directory) {
            return i18nc("@item", "Create folder in second");
        }
        return row.right.kind == FolderSync::Kind::Missing ? i18nc("@item", "Copy first to second") : i18nc("@item", "Overwrite in second from first");
    case Action::DeleteLeft:
        return i18nc("@item", "Delete from first");
    case Action::DeleteRight:
        return i18nc("@item", "Delete from second");
    }
    return {};
}

}

FolderSyncDialog::FolderSyncDialog(const QString &left, const QString &right, QWidget *parent, KSharedConfig::Ptr config)
    : FolderSyncDialog(QUrl::fromLocalFile(left), QUrl::fromLocalFile(right), parent, config)
{
}

FolderSyncDialog::FolderSyncDialog(const QUrl &left, const QUrl &right, QWidget *parent, KSharedConfig::Ptr config)
    : QDialog(parent)
    , m_left(left.isLocalFile() ? left.toLocalFile() : QString())
    , m_right(right.isLocalFile() ? right.toLocalFile() : QString())
    , m_leftUrl(left)
    , m_rightUrl(right)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_options(new QWidget(this))
    , m_include(new QPlainTextEdit(m_options))
    , m_exclude(new QPlainTextEdit(m_options))
    , m_recursive(new QCheckBox(i18nc("@option:check", "Compare subfolders"), m_options))
    , m_contents(new QCheckBox(i18nc("@option:check", "Compare file contents"), m_options))
    , m_links(new QCheckBox(i18nc("@option:check", "Compare symbolic link targets"), m_options))
    , m_saved(new QComboBox(m_options))
    , m_backend(new QComboBox(m_options))
    , m_copyPolicy(new QComboBox(m_options))
    , m_speedLimit(new QSpinBox(m_options))
    , m_status(new QLabel(this))
    , m_results(new QTreeWidget(this))
    , m_planControls(new QWidget(this))
    , m_action(new QComboBox(this))
    , m_report(new QPlainTextEdit(this))
    , m_compare(new QPushButton(i18nc("@action:button", "Compare"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Operation"), this))
    , m_sync(new QPushButton(i18nc("@action:button", "Synchronize…"), this))
{
    setObjectName(QStringLiteral("folderSyncDialog"));
    setWindowTitle(i18nc("@title:window", "Compare and Synchronize Folders"));
    resize(900, 600);
    auto *layout = new QVBoxLayout(this);
    auto *paths =
        new QLabel(i18nc("@info", "First: %1\nSecond: %2", left.toDisplayString(QUrl::PreferLocalFile), right.toDisplayString(QUrl::PreferLocalFile)), this);
    paths->setTextFormat(Qt::PlainText);
    paths->setWordWrap(true);
    layout->addWidget(paths);
    auto *form = new QFormLayout(m_options);
    form->setContentsMargins(0, 0, 0, 0);
    m_include->setObjectName(QStringLiteral("folderSyncInclude"));
    m_exclude->setObjectName(QStringLiteral("folderSyncExclude"));
    m_include->setPlaceholderText(i18nc("@info:placeholder", "One relative-path wildcard per line; empty includes all"));
    m_exclude->setPlaceholderText(i18nc("@info:placeholder", "One relative-path wildcard per line"));
    m_include->setMaximumHeight(m_include->fontMetrics().lineSpacing() * 3);
    m_exclude->setMaximumHeight(m_exclude->fontMetrics().lineSpacing() * 3);
    form->addRow(i18nc("@label", "Include:"), m_include);
    form->addRow(i18nc("@label", "Exclude:"), m_exclude);
    m_recursive->setObjectName(QStringLiteral("folderSyncRecursive"));
    m_contents->setObjectName(QStringLiteral("folderSyncContents"));
    m_links->setObjectName(QStringLiteral("folderSyncLinks"));
    m_recursive->setChecked(Settings::folderSyncRecursive());
    m_contents->setChecked(Settings::folderSyncCompareContents());
    m_links->setChecked(Settings::folderSyncCompareLinks());
    form->addRow(m_recursive);
    form->addRow(m_contents);
    form->addRow(m_links);
    m_backend->setObjectName(QStringLiteral("folderSyncBackend"));
    m_backend->addItem(i18nc("@item:inlistbox synchronization backend", "Plasma Opus (built-in)"), QStringLiteral("builtin"));
    m_backend->addItem(i18nc("@item:inlistbox synchronization backend", "rsync"), QStringLiteral("rsync"));
    m_backend->setToolTip(i18nc("@info:tooltip",
                                "rsync also synchronizes with folders on SSH hosts. It copies and deletes in one direction at a time, "
                                "and deletes only files that are missing from the other folder."));
    form->addRow(i18nc("@label:listbox", "Synchronize with:"), m_backend);
    m_copyPolicy->setObjectName(QStringLiteral("folderSyncCopyPolicy"));
    m_copyPolicy->addItem(i18nc("@item:inlistbox", "Copy all eligible files"), int(FolderSync::CopyPolicy::All));
    m_copyPolicy->addItem(i18nc("@item:inlistbox", "Skip existing files"), int(FolderSync::CopyPolicy::SkipExisting));
    m_copyPolicy->addItem(i18nc("@item:inlistbox", "Only replace older files"), int(FolderSync::CopyPolicy::NewerOnly));
    m_copyPolicy->setCurrentIndex(int(Settings::folderSyncCopyPolicy()));
    form->addRow(i18nc("@label:listbox", "Copy policy:"), m_copyPolicy);
    m_speedLimit->setObjectName(QStringLiteral("folderSyncSpeedLimit"));
    m_speedLimit->setRange(0, 1048576);
    m_speedLimit->setSpecialValueText(i18nc("@item:inlistbox", "Unlimited"));
    m_speedLimit->setSuffix(i18nc("@item:valuesuffix", " KiB/s"));
    m_speedLimit->setValue(Settings::folderSyncSpeedLimitKiB());
    form->addRow(i18nc("@label:spinbox", "Copy speed limit:"), m_speedLimit);
    auto *profileRow = new QHBoxLayout;
    m_saved->setObjectName(QStringLiteral("folderSyncProfiles"));
    auto *save = new QPushButton(i18nc("@action:button", "Save…"), m_options);
    auto *load = new QPushButton(i18nc("@action:button", "Load"), m_options);
    auto *remove = new QPushButton(i18nc("@action:button", "Delete"), m_options);
    save->setObjectName(QStringLiteral("folderSyncSaveProfile"));
    load->setObjectName(QStringLiteral("folderSyncLoadProfile"));
    remove->setObjectName(QStringLiteral("folderSyncDeleteProfile"));
    profileRow->addWidget(m_saved, 1);
    profileRow->addWidget(save);
    profileRow->addWidget(load);
    profileRow->addWidget(remove);
    form->addRow(i18nc("@label", "Profiles:"), profileRow);
    layout->addWidget(m_options);
    auto *help = new QLabel(i18nc("@info",
                                  "Patterns are case-sensitive and match relative paths. Excluded folders are not traversed. "
                                  "Symbolic links are never followed; without content comparison, matching size and time do not prove equal contents. "
                                  "Every item starts as Skip. Plan copies in either direction or select individual actions, including missing parent folders."),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    m_status->setObjectName(QStringLiteral("folderSyncStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_results->setObjectName(QStringLiteral("folderSyncResults"));
    m_results->setRootIsDecorated(false);
    m_results->setHeaderLabels({i18nc("@title:column", "Relative path"), i18nc("@title:column", "Comparison"), i18nc("@title:column", "Planned action")});
    m_results->header()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_results);
    auto *actionRow = new QHBoxLayout(m_planControls);
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->addWidget(new QLabel(i18nc("@label", "Selected item action:"), this));
    m_action->setObjectName(QStringLiteral("folderSyncAction"));
    m_action->setEnabled(false);
    actionRow->addWidget(m_action, 1);
    auto *toLeft = new QPushButton(i18nc("@action:button", "Plan second to first"), m_planControls);
    auto *toRight = new QPushButton(i18nc("@action:button", "Plan first to second"), m_planControls);
    auto *skip = new QPushButton(i18nc("@action:button", "Skip all"), m_planControls);
    toLeft->setObjectName(QStringLiteral("folderSyncPlanToLeft"));
    toRight->setObjectName(QStringLiteral("folderSyncPlanToRight"));
    skip->setObjectName(QStringLiteral("folderSyncSkipAll"));
    actionRow->addWidget(toLeft);
    actionRow->addWidget(toRight);
    actionRow->addWidget(skip);
    m_planControls->setEnabled(false);
    layout->addWidget(m_planControls);
    m_report->setObjectName(QStringLiteral("folderSyncReport"));
    m_report->setReadOnly(true);
    m_report->setMaximumHeight(m_report->fontMetrics().lineSpacing() * 6);
    m_report->hide();
    layout->addWidget(m_report);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_compare->setObjectName(QStringLiteral("folderSyncCompare"));
    m_cancel->setObjectName(QStringLiteral("folderSyncCancel"));
    m_sync->setObjectName(QStringLiteral("folderSyncApply"));
    m_cancel->setEnabled(false);
    m_sync->setEnabled(false);
    buttons->addButton(m_compare, QDialogButtonBox::ActionRole);
    buttons->addButton(m_sync, QDialogButtonBox::ActionRole);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &FolderSyncDialog::reject);
    layout->addWidget(buttons);

    QString error;
    m_profiles = FolderSyncProfiles::load(m_config->group(QStringLiteral("FolderSyncProfiles")), &error);
    refreshProfiles();
    if (!error.isEmpty()) {
        m_status->setText(error);
        save->setEnabled(false);
    } else {
        m_status->setText(i18nc("@info", "Choose filters and compare the folders."));
    }
    QString leftError;
    QString rightError;
    m_leftEndpoint = RsyncSync::endpoint(left, &leftError);
    m_rightEndpoint = RsyncSync::endpoint(right, &rightError);
    m_endpointError = leftError.isEmpty() ? rightError : leftError;
    auto *backends = qobject_cast<QStandardItemModel *>(m_backend->model());
    const bool haveRsync = !RsyncSync::program().isEmpty();
    if (!haveRsync) {
        backends->item(1)->setEnabled(false);
        backends->item(1)->setToolTip(i18nc("@info:tooltip", "Install rsync from your distribution to synchronize with it."));
    }
    if (!isLocal()) {
        backends->item(0)->setEnabled(false);
        backends->item(0)->setToolTip(i18nc("@info:tooltip", "Plasma Opus synchronizes local folders only."));
        m_backend->setCurrentIndex(1);
        QString unavailable = m_endpointError;
        if (unavailable.isEmpty() && !haveRsync) {
            unavailable = i18nc("@info", "Comparing SSH locations requires rsync. Install it from your distribution.");
        }
        if (unavailable.isEmpty()) {
            unavailable = RsyncSync::endpointsError(*m_leftEndpoint, *m_rightEndpoint);
        }
        if (!unavailable.isEmpty()) {
            m_canCompare = false;
            m_compare->setEnabled(false);
            m_status->setText(unavailable);
        }
    }
    connect(m_saved, &QComboBox::currentIndexChanged, this, [load, remove](int index) {
        load->setEnabled(index >= 0);
        remove->setEnabled(index >= 0);
    });
    load->setEnabled(m_saved->currentIndex() >= 0);
    remove->setEnabled(m_saved->currentIndex() >= 0);
    connect(save, &QPushButton::clicked, this, [this] {
        const QString invalid = FolderSync::optionsError(options());
        if (!invalid.isEmpty()) {
            m_status->setText(invalid);
            return;
        }
        bool accepted = false;
        const QString name =
            QInputDialog::getText(this, i18nc("@title:window", "Save Comparison Profile"), i18nc("@label", "Name:"), QLineEdit::Normal, {}, &accepted)
                .trimmed();
        if (!accepted) {
            return;
        }
        const auto group = m_config->group(QStringLiteral("FolderSyncProfiles"));
        QString error;
        auto profiles = FolderSyncProfiles::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        profiles.append({name, options()});
        if (!FolderSyncProfiles::save(group, profiles, &error)) {
            m_status->setText(error);
            return;
        }
        m_profiles = profiles;
        refreshProfiles();
        m_saved->setCurrentIndex(m_profiles.size() - 1);
        m_status->setText(i18nc("@info", "Comparison profile saved."));
    });
    connect(load, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_profiles.size()) {
            return;
        }
        const QString name = m_profiles.at(index).name;
        QString error;
        const auto profiles = FolderSyncProfiles::load(m_config->group(QStringLiteral("FolderSyncProfiles")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto it = std::find_if(profiles.cbegin(), profiles.cend(), [&name](const auto &profile) {
            return profile.name == name;
        });
        if (it == profiles.cend()) {
            m_profiles = profiles;
            refreshProfiles();
            m_status->setText(i18nc("@info", "This profile no longer exists."));
            return;
        }
        setOptions(it->options);
        invalidate();
        m_status->setText(i18nc("@info", "Profile loaded. Compare the folders to update the results."));
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_profiles.size()) {
            return;
        }
        const QString name = m_profiles.at(index).name;
        if (QMessageBox::question(this,
                                  i18nc("@title:window", "Delete Comparison Profile"),
                                  i18nc("@info", "Delete the profile “%1”?", name),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        const auto group = m_config->group(QStringLiteral("FolderSyncProfiles"));
        QString error;
        auto profiles = FolderSyncProfiles::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto it = std::find_if(profiles.begin(), profiles.end(), [&name](const auto &profile) {
            return profile.name == name;
        });
        if (it == profiles.end()) {
            m_profiles = profiles;
            refreshProfiles();
            m_status->setText(i18nc("@info", "This profile no longer exists."));
            return;
        }
        profiles.erase(it);
        if (!FolderSyncProfiles::save(group, profiles, &error)) {
            m_status->setText(error);
            return;
        }
        m_profiles = profiles;
        refreshProfiles();
        m_status->setText(i18nc("@info", "Profile deleted."));
    });
    connect(m_include, &QPlainTextEdit::textChanged, this, &FolderSyncDialog::invalidate);
    connect(m_exclude, &QPlainTextEdit::textChanged, this, &FolderSyncDialog::invalidate);
    connect(m_recursive, &QCheckBox::toggled, this, &FolderSyncDialog::invalidate);
    connect(m_contents, &QCheckBox::toggled, this, &FolderSyncDialog::invalidate);
    connect(m_links, &QCheckBox::toggled, this, &FolderSyncDialog::invalidate);
    connect(m_copyPolicy, &QComboBox::currentIndexChanged, this, [this] {
        if (m_comparison) {
            planCopies(FolderSync::Action::Skip);
            m_status->setText(i18nc("@info", "Copy policy changed. Plan and review the actions again."));
        }
    });
    connect(m_compare, &QPushButton::clicked, this, &FolderSyncDialog::compare);
    connect(m_sync, &QPushButton::clicked, this, &FolderSyncDialog::synchronize);
    connect(toLeft, &QPushButton::clicked, this, [this] {
        planCopies(FolderSync::Action::CopyToLeft);
    });
    connect(toRight, &QPushButton::clicked, this, [this] {
        planCopies(FolderSync::Action::CopyToRight);
    });
    connect(skip, &QPushButton::clicked, this, [this] {
        planCopies(FolderSync::Action::Skip);
    });
    connect(m_results, &QTreeWidget::currentItemChanged, this, [this] {
        updateSelectedAction();
    });
    connect(m_action, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!m_comparison || index < 0 || !m_results->currentItem()) {
            return;
        }
        const int row = m_results->indexOfTopLevelItem(m_results->currentItem());
        if (row < 0 || row >= m_actions.size()) {
            return;
        }
        m_actions[row] = static_cast<FolderSync::Action>(m_action->itemData(index).toInt());
        m_results->currentItem()->setText(2, actionText(m_actions.at(row), m_comparison->rows.at(row)));
    });
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        if (m_rsync) {
            m_rsync->cancel();
            m_cancel->setEnabled(false);
            m_status->setText(i18nc("@info", "Canceling operation…"));
        } else if (m_canceled) {
            m_canceled->store(true);
            m_cancel->setEnabled(false);
            m_status->setText(i18nc("@info", "Canceling operation…"));
        }
    });
    connect(&m_watcher, &QFutureWatcher<FolderSync::Comparison>::finished, this, [this] {
        const auto result = m_watcher.result();
        m_canceled.reset();
        m_options->setEnabled(true);
        m_compare->setEnabled(true);
        m_cancel->setEnabled(false);
        if (!result.error.isEmpty()) {
            m_status->setText(result.error);
            return;
        }
        showComparison(result);
    });
    connect(&m_syncWatcher, &QFutureWatcher<FolderSync::ApplyResult>::finished, this, [this] {
        const auto result = m_syncWatcher.result();
        if (m_syncJob) {
            if (m_canceled && m_canceled->load()) {
                m_syncJob->cancel();
            } else if (!result.error.isEmpty()) {
                m_syncJob->finish(i18np("Synchronization stopped after one operation. Details are shown in Plasma Opus.",
                                        "Synchronization stopped after %1 operations. Details are shown in Plasma Opus.",
                                        result.completed.size()));
            }
        }
        m_canceled.reset();
        m_options->setEnabled(true);
        m_results->setEnabled(true);
        m_compare->setEnabled(true);
        m_cancel->setEnabled(false);
        m_comparison.reset();
        m_actions.clear();
        updateSelectedAction();
        const QString completed = i18np("Completed one operation.", "Completed %1 operations.", result.completed.size());
        m_status->setText((result.error.isEmpty() ? completed : completed + QLatin1Char(' ') + result.error) + QLatin1Char(' ')
                          + i18nc("@info", "Compare again before synchronizing."));
        QStringList report;
        if (!result.error.isEmpty()) {
            report.append(result.error);
        }
        report.append(completed);
        report.append(result.completed);
        m_report->setPlainText(report.join(QLatin1Char('\n')));
        m_report->show();
    });
}

FolderSyncDialog::~FolderSyncDialog()
{
    if (m_rsync) {
        m_rsync->disconnect(this);
        delete m_rsync;
        if (m_syncJob) {
            m_syncJob->cancel();
        }
    }
    if (m_canceled) {
        m_canceled->store(true);
    }
    m_watcher.waitForFinished();
    m_syncWatcher.waitForFinished();
}

void FolderSyncDialog::reject()
{
    if (m_watcher.isRunning() || m_syncWatcher.isRunning() || m_rsync) {
        if (m_rsync) {
            m_rsync->cancel();
        }
        if (m_canceled) {
            m_canceled->store(true);
        }
        m_status->setText(i18nc("@info", "Canceling operation; wait for it to finish before closing."));
        return;
    }
    QDialog::reject();
}

FolderSync::Options FolderSyncDialog::options() const
{
    return {patterns(m_include), patterns(m_exclude), m_recursive->isChecked(), m_contents->isChecked(), m_links->isChecked()};
}

void FolderSyncDialog::setOptions(const FolderSync::Options &value)
{
    m_include->setPlainText(value.includePatterns.join(QLatin1Char('\n')));
    m_exclude->setPlainText(value.excludePatterns.join(QLatin1Char('\n')));
    m_recursive->setChecked(value.recursive);
    m_contents->setChecked(value.compareContents);
    m_links->setChecked(value.compareSymlinks);
}

void FolderSyncDialog::refreshProfiles()
{
    m_saved->clear();
    for (const auto &profile : std::as_const(m_profiles)) {
        m_saved->addItem(profile.name);
    }
}

void FolderSyncDialog::invalidate()
{
    m_results->clear();
    m_comparison.reset();
    m_actions.clear();
    m_action->clear();
    m_action->setEnabled(false);
    m_planControls->setEnabled(false);
    m_sync->setEnabled(false);
    m_report->hide();
    m_status->setText(i18nc("@info", "Filters changed. Compare again to update the results."));
}

void FolderSyncDialog::compare()
{
    const auto value = options();
    QString error = FolderSync::optionsError(value);
    if (error.isEmpty() && !isLocal()) {
        if (!m_leftEndpoint || !m_rightEndpoint) {
            error = m_endpointError;
        } else {
            error = RsyncSync::endpointsError(*m_leftEndpoint, *m_rightEndpoint);
        }
        if (error.isEmpty()) {
            RsyncSync::filterRules(value, &error);
        }
    }
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    invalidate();
    if (!isLocal()) {
        compareWithRsync(value);
        return;
    }
    m_options->setEnabled(false);
    m_compare->setEnabled(false);
    m_cancel->setEnabled(true);
    m_status->setText(i18nc("@info", "Comparing folders…"));
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const auto canceled = m_canceled;
    const QString left = m_left;
    const QString right = m_right;
    m_watcher.setFuture(QtConcurrent::run([left, right, value, canceled] {
        return FolderSync::compare(left, right, value, [canceled] {
            return canceled->load();
        });
    }));
}

void FolderSyncDialog::compareWithRsync(const FolderSync::Options &options)
{
    setRsyncRunning(true);
    m_status->setText(i18nc("@info", "Comparing folders with rsync…"));
    const QString askPass = RemoteMounts::findAskPass();
    const RsyncSync::Endpoint left = *m_leftEndpoint;
    const RsyncSync::Endpoint right = *m_rightEndpoint;
    auto *process =
        new RsyncSync::Process(RsyncSync::program(),
                               {RsyncSync::listStep(left, right, options, askPass.isEmpty()), RsyncSync::listStep(right, left, options, askPass.isEmpty())},
                               RsyncSync::environment(askPass),
                               this);
    m_rsync = process;
    connect(process, &RsyncSync::Process::finished, this, [this, process, left, right, options] {
        process->deleteLater();
        m_rsync.clear();
        setRsyncRunning(false);
        if (!process->succeeded()) {
            m_status->setText(process->wasCanceled() ? i18nc("@info", "Comparison canceled.") : process->errorText());
            return;
        }
        showComparison(RsyncSync::comparison(left, right, process->lines(0), process->lines(1), options));
    });
    process->start();
}

void FolderSyncDialog::showComparison(const FolderSync::Comparison &comparison)
{
    m_comparison = comparison;
    m_actions = QList<FolderSync::Action>(comparison.rows.size(), FolderSync::Action::Skip);
    for (const auto &row : comparison.rows) {
        new QTreeWidgetItem(m_results, {row.relativePath, differenceText(row.difference), actionText(FolderSync::Action::Skip, row)});
    }
    m_planControls->setEnabled(!comparison.rows.isEmpty());
    m_sync->setEnabled(!comparison.rows.isEmpty());
    m_status->setText(i18np("Compared one item.", "Compared %1 items.", comparison.rows.size()));
}

void FolderSyncDialog::updateSelectedAction()
{
    const QSignalBlocker blocker(m_action);
    m_action->clear();
    if (!m_comparison || !m_results->currentItem()) {
        m_action->setEnabled(false);
        return;
    }
    const int index = m_results->indexOfTopLevelItem(m_results->currentItem());
    if (index < 0 || index >= m_comparison->rows.size()) {
        m_action->setEnabled(false);
        return;
    }
    const auto &row = m_comparison->rows.at(index);
    const auto add = [this, &row](FolderSync::Action action) {
        m_action->addItem(actionText(action, row), int(action));
    };
    add(FolderSync::Action::Skip);
    if (FolderSync::copyAllowed(row.right, row.left, copyPolicy())) {
        add(FolderSync::Action::CopyToLeft);
    }
    if (FolderSync::copyAllowed(row.left, row.right, copyPolicy())) {
        add(FolderSync::Action::CopyToRight);
    }
    if (row.left.kind == FolderSync::Kind::File) {
        add(FolderSync::Action::DeleteLeft);
    }
    if (row.right.kind == FolderSync::Kind::File) {
        add(FolderSync::Action::DeleteRight);
    }
    m_action->setCurrentIndex(m_action->findData(int(m_actions.at(index))));
    m_action->setEnabled(true);
}

void FolderSyncDialog::planCopies(FolderSync::Action direction)
{
    if (!m_comparison) {
        return;
    }
    for (qsizetype i = 0; i < m_actions.size(); ++i) {
        const auto &row = m_comparison->rows.at(i);
        const bool allowed = direction == FolderSync::Action::CopyToLeft ? FolderSync::copyAllowed(row.right, row.left, copyPolicy())
            : direction == FolderSync::Action::CopyToRight               ? FolderSync::copyAllowed(row.left, row.right, copyPolicy())
                                                                         : false;
        m_actions[i] = allowed ? direction : FolderSync::Action::Skip;
        m_results->topLevelItem(int(i))->setText(2, actionText(m_actions.at(i), row));
    }
    updateSelectedAction();
    m_status->setText(i18nc("@info", "Review the planned actions before synchronizing."));
}

void FolderSyncDialog::synchronize()
{
    if (!m_comparison) {
        return;
    }
    QString error = isLocal() ? FolderSync::planError(*m_comparison, m_actions, true, true) : QString();
    RsyncSync::Transfer transfer;
    if (error.isEmpty() && usesRsync()) {
        if (!m_leftEndpoint || !m_rightEndpoint) {
            error = m_endpointError;
        } else {
            error = RsyncSync::endpointsError(*m_leftEndpoint, *m_rightEndpoint);
        }
        if (error.isEmpty()) {
            error = RsyncSync::planError(*m_comparison, m_actions, &transfer);
        }
    }
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    int overwrites = 0;
    int deletions = 0;
    QStringList details;
    for (qsizetype i = 0; i < m_actions.size(); ++i) {
        const auto action = m_actions.at(i);
        if (action == FolderSync::Action::Skip) {
            continue;
        }
        const auto &row = m_comparison->rows.at(i);
        if ((action == FolderSync::Action::CopyToLeft && !FolderSync::copyAllowed(row.right, row.left, copyPolicy()))
            || (action == FolderSync::Action::CopyToRight && !FolderSync::copyAllowed(row.left, row.right, copyPolicy()))) {
            m_status->setText(i18nc("@info", "The copy policy does not allow the planned action: %1", row.relativePath));
            return;
        }
        if ((action == FolderSync::Action::CopyToLeft && row.left.kind == FolderSync::Kind::File)
            || (action == FolderSync::Action::CopyToRight && row.right.kind == FolderSync::Kind::File)) {
            ++overwrites;
        }
        if (action == FolderSync::Action::DeleteLeft || action == FolderSync::Action::DeleteRight) {
            ++deletions;
        }
        details.append(i18nc("@info", "%1: %2", actionText(action, row), row.relativePath));
    }
    if (usesRsync()) {
        checkWithRsync(transfer, details);
        return;
    }
    const QString counts = i18np("One operation", "%1 operations", details.size()) + QStringLiteral("\n") + i18np("One overwrite", "%1 overwrites", overwrites)
        + QStringLiteral("\n") + i18np("One deletion", "%1 deletions", deletions);
    QMessageBox confirm(QMessageBox::Warning,
                        i18nc("@title:window", "Confirm Synchronization"),
                        i18nc("@info",
                              "Run the following plan?\n%1\n\n"
                              "Deletions are permanent and these operations cannot be undone. "
                              "Do not change either folder while synchronization runs. Review the planned actions in the details.",
                              counts),
                        QMessageBox::Yes | QMessageBox::No,
                        this);
    confirm.setTextFormat(Qt::PlainText);
    confirm.setDefaultButton(QMessageBox::No);
    confirm.setDetailedText(details.join(QLatin1Char('\n')));
    if (confirm.exec() != QMessageBox::Yes) {
        return;
    }
    const auto comparison = *m_comparison;
    const auto actions = m_actions;
    m_options->setEnabled(false);
    m_results->setEnabled(false);
    m_planControls->setEnabled(false);
    m_compare->setEnabled(false);
    m_sync->setEnabled(false);
    m_cancel->setEnabled(true);
    m_report->hide();
    m_status->setText(i18nc("@info", "Synchronizing folders…"));
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const auto canceled = m_canceled;
    const quint64 speedLimit = quint64(m_speedLimit->value()) * 1024;
    m_syncWatcher.setFuture(QtConcurrent::run([comparison, actions, canceled, speedLimit] {
        return FolderSync::apply(
            comparison,
            actions,
            true,
            true,
            [canceled] {
                return canceled->load();
            },
            speedLimit);
    }));
    m_syncJob = OperationJob::track(new OperationJob(i18nc("@title job", "Synchronizing folders")), &m_syncWatcher);
    m_syncJob->setSource(QUrl::fromLocalFile(m_left));
    m_syncJob->setDestination(QUrl::fromLocalFile(m_right));
    m_syncJob->setCancelHandler([button = QPointer<QPushButton>(m_cancel)] {
        if (button && button->isEnabled()) {
            button->click();
        }
    });
}

FolderSync::CopyPolicy FolderSyncDialog::copyPolicy() const
{
    return static_cast<FolderSync::CopyPolicy>(m_copyPolicy->currentData().toInt());
}

void FolderSyncDialog::checkWithRsync(const RsyncSync::Transfer &transfer, const QStringList &details)
{
    setRsyncRunning(true);
    m_report->hide();
    m_status->setText(i18nc("@info", "Checking the plan with rsync…"));
    const QString askPass = RemoteMounts::findAskPass();
    auto *process = new RsyncSync::Process(
        RsyncSync::program(),
        RsyncSync::transferSteps(*m_leftEndpoint, *m_rightEndpoint, transfer, copyPolicy(), quint64(m_speedLimit->value()), true, askPass.isEmpty()),
        RsyncSync::environment(askPass),
        this);
    m_rsync = process;
    connect(process, &RsyncSync::Process::finished, this, [this, process, transfer, details] {
        process->deleteLater();
        m_rsync.clear();
        setRsyncRunning(false);
        if (!process->succeeded()) {
            m_status->setText(process->wasCanceled() ? i18nc("@info", "Synchronization canceled.") : process->errorText());
            return;
        }
        const QStringList lines = process->allLines();
        const RsyncSync::Summary summary = RsyncSync::summarize(transfer, lines);
        QStringList counts{i18np("One new file", "%1 new files", summary.created),
                           i18np("One new folder", "%1 new folders", summary.folders),
                           i18np("One overwrite", "%1 overwrites", summary.overwritten),
                           i18np("One deletion", "%1 deletions", summary.deleted)};
        if (!summary.missing.isEmpty()) {
            counts.append(i18np("rsync would leave one planned item unchanged", "rsync would leave %1 planned items unchanged", summary.missing.size()));
        }
        QStringList detailed = details;
        detailed << QString() << i18nc("@info", "Changes reported by rsync:") << lines;
        if (!summary.missing.isEmpty()) {
            detailed << QString() << i18nc("@info", "Planned items rsync would leave unchanged:") << summary.missing;
        }
        QMessageBox confirm(QMessageBox::Warning,
                            i18nc("@title:window", "Confirm Synchronization"),
                            i18nc("@info",
                                  "Run the following plan with rsync?\n%1\n\n"
                                  "Deletions are permanent and these operations cannot be undone. "
                                  "Do not change either folder while synchronization runs. Review the planned actions in the details.",
                                  counts.join(QLatin1Char('\n'))),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
        confirm.setTextFormat(Qt::PlainText);
        confirm.setDefaultButton(QMessageBox::No);
        confirm.setDetailedText(detailed.join(QLatin1Char('\n')));
        if (confirm.exec() != QMessageBox::Yes) {
            m_status->setText(i18nc("@info", "Review the planned actions before synchronizing."));
            return;
        }
        synchronizeWithRsync(transfer);
    });
    process->start();
}

void FolderSyncDialog::synchronizeWithRsync(const RsyncSync::Transfer &transfer)
{
    if (!m_comparison) {
        return;
    }
    setRsyncRunning(true);
    m_status->setText(i18nc("@info", "Synchronizing folders with rsync…"));
    const QString askPass = RemoteMounts::findAskPass();
    auto *process = new RsyncSync::Process(
        RsyncSync::program(),
        RsyncSync::transferSteps(*m_leftEndpoint, *m_rightEndpoint, transfer, copyPolicy(), quint64(m_speedLimit->value()), false, askPass.isEmpty()),
        RsyncSync::environment(askPass),
        this);
    m_rsync = process;
    const bool toRight = transfer.direction == RsyncSync::Direction::ToRight;
    m_syncJob = OperationJob::track(new OperationJob(i18nc("@title job", "Synchronizing folders")));
    m_syncJob->setSource(toRight ? m_leftUrl : m_rightUrl);
    m_syncJob->setDestination(toRight ? m_rightUrl : m_leftUrl);
    m_syncJob->setCancelHandler([button = QPointer<QPushButton>(m_cancel)] {
        if (button && button->isEnabled()) {
            button->click();
        }
    });
    connect(process, &RsyncSync::Process::progressChanged, this, [this](int percent) {
        if (m_syncJob) {
            m_syncJob->setProgress(qulonglong(percent), 100);
        }
    });
    connect(process, &RsyncSync::Process::finished, this, [this, process, transfer] {
        process->deleteLater();
        m_rsync.clear();
        const QStringList lines = process->allLines();
        const RsyncSync::Summary summary = RsyncSync::summarize(transfer, lines);
        const QString error = process->succeeded() ? QString() : process->errorText();
        if (m_syncJob) {
            if (process->wasCanceled()) {
                m_syncJob->cancel();
            } else {
                m_syncJob->finish(error.isEmpty() ? QString() : i18nc("@info", "Synchronization with rsync stopped. Details are shown in Plasma Opus."));
            }
        }
        m_comparison.reset();
        m_actions.clear();
        updateSelectedAction();
        setRsyncRunning(false);
        const int changed = summary.created + summary.folders + summary.overwritten + summary.deleted;
        const QString completed = i18np("rsync changed one item.", "rsync changed %1 items.", changed);
        m_status->setText((error.isEmpty() ? completed : completed + QLatin1Char(' ') + error.section(QLatin1Char('\n'), 0, 0)) + QLatin1Char(' ')
                          + i18nc("@info", "Compare again before synchronizing."));
        QStringList report;
        if (!error.isEmpty()) {
            report.append(error);
        }
        report.append(completed);
        report.append(i18np("One new file", "%1 new files", summary.created));
        report.append(i18np("One new folder", "%1 new folders", summary.folders));
        report.append(i18np("One overwrite", "%1 overwrites", summary.overwritten));
        report.append(i18np("One deletion", "%1 deletions", summary.deleted));
        for (const QString &path : summary.missing) {
            report.append(i18nc("@info", "Unchanged by rsync: %1", path));
        }
        report.append(lines);
        m_report->setPlainText(report.join(QLatin1Char('\n')));
        m_report->show();
    });
    process->start();
}

void FolderSyncDialog::setRsyncRunning(bool running)
{
    m_options->setEnabled(!running);
    m_results->setEnabled(!running);
    const bool planned = !running && m_comparison && !m_comparison->rows.isEmpty();
    m_planControls->setEnabled(planned);
    m_sync->setEnabled(planned);
    m_compare->setEnabled(!running && m_canCompare);
    m_cancel->setEnabled(running);
}

bool FolderSyncDialog::isLocal() const
{
    return !m_left.isEmpty() && !m_right.isEmpty();
}

bool FolderSyncDialog::usesRsync() const
{
    return m_backend->currentData().toString() == QLatin1String("rsync");
}
