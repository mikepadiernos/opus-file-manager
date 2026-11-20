// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldersyncdialog.h"

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
}

FolderSyncDialog::FolderSyncDialog(const QString &left, const QString &right, QWidget *parent, KSharedConfig::Ptr config)
    : QDialog(parent)
    , m_left(left)
    , m_right(right)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_options(new QWidget(this))
    , m_include(new QPlainTextEdit(m_options))
    , m_exclude(new QPlainTextEdit(m_options))
    , m_recursive(new QCheckBox(i18nc("@option:check", "Compare subfolders"), m_options))
    , m_contents(new QCheckBox(i18nc("@option:check", "Compare file contents"), m_options))
    , m_links(new QCheckBox(i18nc("@option:check", "Compare symbolic link targets"), m_options))
    , m_saved(new QComboBox(m_options))
    , m_status(new QLabel(this))
    , m_results(new QTreeWidget(this))
    , m_compare(new QPushButton(i18nc("@action:button", "Compare"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Comparison"), this))
{
    setObjectName(QStringLiteral("folderSyncDialog"));
    setWindowTitle(i18nc("@title:window", "Compare Folders with Filters"));
    resize(900, 600);
    auto *layout = new QVBoxLayout(this);
    auto *paths = new QLabel(i18nc("@info", "First: %1\nSecond: %2", left, right), this);
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
    m_recursive->setChecked(true);
    form->addRow(m_recursive);
    form->addRow(m_contents);
    form->addRow(m_links);
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
                                  "Symbolic links are never followed; without content comparison, matching size and time do not prove equal contents."),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    m_status->setObjectName(QStringLiteral("folderSyncStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_results->setObjectName(QStringLiteral("folderSyncResults"));
    m_results->setRootIsDecorated(false);
    m_results->setHeaderLabels({i18nc("@title:column", "Relative path"), i18nc("@title:column", "Comparison")});
    m_results->header()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_results);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_compare->setObjectName(QStringLiteral("folderSyncCompare"));
    m_cancel->setObjectName(QStringLiteral("folderSyncCancel"));
    m_cancel->setEnabled(false);
    buttons->addButton(m_compare, QDialogButtonBox::ActionRole);
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
    connect(m_compare, &QPushButton::clicked, this, &FolderSyncDialog::compare);
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        if (m_canceled) {
            m_canceled->store(true);
            m_cancel->setEnabled(false);
            m_status->setText(i18nc("@info", "Canceling comparison…"));
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
        for (const auto &row : result.rows) {
            new QTreeWidgetItem(m_results, {row.relativePath, differenceText(row.difference)});
        }
        m_status->setText(i18np("Compared one item.", "Compared %1 items.", result.rows.size()));
    });
}

FolderSyncDialog::~FolderSyncDialog()
{
    if (m_canceled) {
        m_canceled->store(true);
    }
    m_watcher.waitForFinished();
}

void FolderSyncDialog::reject()
{
    if (m_watcher.isRunning()) {
        if (m_canceled) {
            m_canceled->store(true);
        }
        m_status->setText(i18nc("@info", "Canceling comparison; wait for it to finish before closing."));
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
    m_status->setText(i18nc("@info", "Filters changed. Compare again to update the results."));
}

void FolderSyncDialog::compare()
{
    const auto value = options();
    const QString error = FolderSync::optionsError(value);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    m_results->clear();
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
