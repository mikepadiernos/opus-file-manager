// SPDX-License-Identifier: GPL-3.0-or-later

#include "batchrenamedialog.h"

#include "imagedetails.h"
#include "librarymovetracker.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>

BatchRenameDialog::BatchRenameDialog(const QStringList &paths, QWidget *parent, KSharedConfig::Ptr config, bool manageOnly)
    : QDialog(parent)
    , m_paths(paths)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_options(new QWidget(this))
    , m_mode(new QComboBox(m_options))
    , m_pattern(new QLineEdit(QStringLiteral("{name}-{nn}"), m_options))
    , m_replacement(new QLineEdit(m_options))
    , m_startNumber(new QSpinBox(m_options))
    , m_padding(new QSpinBox(m_options))
    , m_saved(new QComboBox(m_options))
    , m_status(new QLabel(this))
    , m_results(new QTreeWidget(this))
    , m_preview(new QPushButton(i18nc("@action:button", "Preview"), this))
    , m_apply(new QPushButton(i18nc("@action:button", "Rename"), this))
    , m_cancel(new QPushButton(i18nc("@action:button", "Cancel Operation"), this))
{
    setObjectName(QStringLiteral("batchRenameDialog"));
    setWindowTitle(manageOnly ? i18nc("@title:window", "Manage Rename Presets") : i18nc("@title:window", "Batch Rename"));
    resize(850, 520);
    m_paths.sort();
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout(m_options);
    form->setContentsMargins(0, 0, 0, 0);
    m_mode->setObjectName(QStringLiteral("batchRenameMode"));
    m_mode->addItem(i18nc("@item:inlistbox", "Template"));
    m_mode->addItem(i18nc("@item:inlistbox", "Regular expression"));
    m_pattern->setObjectName(QStringLiteral("batchRenamePattern"));
    m_replacement->setObjectName(QStringLiteral("batchRenameReplacement"));
    m_replacement->setEnabled(false);
    m_startNumber->setObjectName(QStringLiteral("batchRenameStartNumber"));
    m_startNumber->setRange(0, 1000000000);
    m_startNumber->setValue(1);
    m_padding->setObjectName(QStringLiteral("batchRenamePadding"));
    m_padding->setRange(1, 16);
    m_padding->setValue(2);
    form->addRow(i18nc("@label", "Mode:"), m_mode);
    form->addRow(i18nc("@label", "Pattern:"), m_pattern);
    form->addRow(i18nc("@label", "Regex replacement:"), m_replacement);
    form->addRow(i18nc("@label", "Start number:"), m_startNumber);
    form->addRow(i18nc("@label", "Number padding:"), m_padding);
    auto *presetRow = new QHBoxLayout;
    m_saved->setObjectName(QStringLiteral("batchRenamePresets"));
    auto *savePreset = new QPushButton(i18nc("@action:button", "Save…"), m_options);
    auto *loadPreset = new QPushButton(i18nc("@action:button", "Load"), m_options);
    auto *deletePreset = new QPushButton(i18nc("@action:button", "Delete"), m_options);
    auto *updatePreset = new QPushButton(i18nc("@action:button", "Update Selected…"), m_options);
    auto *defaultPreset = new QPushButton(i18nc("@action:button", "Set Default"), m_options);
    auto *clearDefault = new QPushButton(i18nc("@action:button", "Clear Default"), m_options);
    savePreset->setObjectName(QStringLiteral("batchRenameSavePreset"));
    loadPreset->setObjectName(QStringLiteral("batchRenameLoadPreset"));
    deletePreset->setObjectName(QStringLiteral("batchRenameDeletePreset"));
    updatePreset->setObjectName(QStringLiteral("batchRenameUpdatePreset"));
    defaultPreset->setObjectName(QStringLiteral("batchRenameSetDefault"));
    clearDefault->setObjectName(QStringLiteral("batchRenameClearDefault"));
    presetRow->addWidget(m_saved, 1);
    presetRow->addWidget(savePreset);
    presetRow->addWidget(loadPreset);
    presetRow->addWidget(deletePreset);
    form->addRow(i18nc("@label", "Saved presets:"), presetRow);
    auto *presetActions = new QHBoxLayout;
    presetActions->addWidget(updatePreset);
    presetActions->addWidget(defaultPreset);
    presetActions->addWidget(clearDefault);
    presetActions->addStretch();
    form->addRow(presetActions);
    auto *defaultLabel = new QLabel(m_options);
    defaultLabel->setObjectName(QStringLiteral("batchRenameDefault"));
    const auto refreshDefault = [this, defaultLabel] {
        const QString name = m_config->group(QStringLiteral("BatchRenamePresets")).readEntry("DefaultPreset", QString());
        defaultLabel->setText(name.isEmpty() ? i18nc("@info", "Default preset: none") : i18nc("@info", "Default preset: %1", name));
    };
    refreshDefault();
    form->addRow(defaultLabel);
    layout->addWidget(m_options);
    auto *help = new QLabel(i18nc("@info",
                                  "Tokens: {name}, {ext}, {n}, {nn}, {title}, {artist}, {album}, {year}, {date}, {camera}. "
                                  "The original extension is retained unless {ext} is used. Regex is case-sensitive and applies to the base name; "
                                  "use $1 for a capture group and $$ for a literal dollar."),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);
    m_status->setObjectName(QStringLiteral("batchRenameStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setText(i18nc("@info", "Preview changes before renaming. Existing items are never overwritten."));
    layout->addWidget(m_status);
    QString presetError;
    m_presets = BatchRenamePresets::load(m_config->group(QStringLiteral("BatchRenamePresets")), &presetError);
    updatePresets();
    if (!presetError.isEmpty()) {
        m_status->setText(presetError);
        savePreset->setEnabled(false);
    }
    connect(m_saved, &QComboBox::currentIndexChanged, this, [loadPreset, deletePreset, updatePreset, defaultPreset](int index) {
        loadPreset->setEnabled(index >= 0);
        deletePreset->setEnabled(index >= 0);
        updatePreset->setEnabled(index >= 0);
        defaultPreset->setEnabled(index >= 0);
    });
    loadPreset->setEnabled(m_saved->currentIndex() >= 0);
    deletePreset->setEnabled(m_saved->currentIndex() >= 0);
    updatePreset->setEnabled(m_saved->currentIndex() >= 0);
    defaultPreset->setEnabled(m_saved->currentIndex() >= 0);
    const auto setDefault = [this, refreshDefault](const QString &name) {
        auto group = m_config->group(QStringLiteral("BatchRenamePresets"));
        const QString previous = group.readEntry("DefaultPreset", QString());
        group.writeEntry("DefaultPreset", name);
        if (!group.sync()) {
            group.writeEntry("DefaultPreset", previous);
            m_status->setText(i18nc("@info", "Could not save the default rename preset."));
            return;
        }
        refreshDefault();
        m_status->setText(name.isEmpty() ? i18nc("@info", "Default rename preset cleared.") : i18nc("@info", "Default rename preset: %1", name));
    };
    connect(defaultPreset, &QPushButton::clicked, this, [this, setDefault] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_presets.size()) {
            return;
        }
        QString error;
        const auto presets = BatchRenamePresets::load(m_config->group(QStringLiteral("BatchRenamePresets")), &error);
        const QString name = m_presets.at(index).name;
        if (!error.isEmpty()) {
            m_status->setText(error);
        } else if (std::none_of(presets.cbegin(), presets.cend(), [&name](const auto &preset) {
                       return preset.name == name;
                   })) {
            m_status->setText(i18nc("@info", "This preset no longer exists."));
        } else {
            setDefault(name);
        }
    });
    connect(clearDefault, &QPushButton::clicked, this, [setDefault] {
        setDefault({});
    });
    connect(savePreset, &QPushButton::clicked, this, [this] {
        const QString ruleError = BatchRename::ruleError(rule());
        if (!ruleError.isEmpty()) {
            m_status->setText(ruleError);
            return;
        }
        bool accepted = false;
        const QString name =
            QInputDialog::getText(this, i18nc("@title:window", "Save Rename Preset"), i18nc("@label", "Name:"), QLineEdit::Normal, {}, &accepted).trimmed();
        if (!accepted) {
            return;
        }
        QString error;
        const auto group = m_config->group(QStringLiteral("BatchRenamePresets"));
        auto presets = BatchRenamePresets::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        presets.append({name, rule()});
        if (!BatchRenamePresets::save(group, presets, &error)) {
            m_status->setText(error);
            return;
        }
        m_presets = presets;
        updatePresets();
        m_saved->setCurrentIndex(m_presets.size() - 1);
        m_status->setText(i18nc("@info", "Rename preset saved."));
    });
    connect(loadPreset, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_presets.size()) {
            return;
        }
        const QString name = m_presets.at(index).name;
        QString error;
        const auto presets = BatchRenamePresets::load(m_config->group(QStringLiteral("BatchRenamePresets")), &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto it = std::find_if(presets.cbegin(), presets.cend(), [&name](const auto &preset) {
            return preset.name == name;
        });
        if (it == presets.cend()) {
            m_presets = presets;
            updatePresets();
            m_status->setText(i18nc("@info", "This preset no longer exists."));
            return;
        }
        setRule(it->rule);
        invalidatePreview();
        m_status->setText(i18nc("@info", "Preset loaded. Review it and preview before renaming."));
    });
    connect(updatePreset, &QPushButton::clicked, this, [this] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_presets.size()) {
            return;
        }
        const QString invalid = BatchRename::ruleError(rule());
        if (!invalid.isEmpty()) {
            m_status->setText(invalid);
            return;
        }
        const auto original = m_presets.at(index);
        QString error;
        auto group = m_config->group(QStringLiteral("BatchRenamePresets"));
        auto presets = BatchRenamePresets::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        auto it = std::find_if(presets.begin(), presets.end(), [&original](const auto &preset) {
            return preset.name == original.name;
        });
        if (it == presets.end() || it->rule.pattern != original.rule.pattern || it->rule.replacement != original.rule.replacement
            || it->rule.regularExpression != original.rule.regularExpression || it->rule.start != original.rule.start
            || it->rule.padding != original.rule.padding) {
            m_status->setText(i18nc("@info", "This preset changed elsewhere. Close and reopen the dialog."));
            return;
        }
        if (QMessageBox::question(this,
                                  i18nc("@title:window", "Update Rename Preset"),
                                  i18nc("@info", "Replace the saved rule for “%1” with the current rule?", original.name),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        it->rule = rule();
        if (!BatchRenamePresets::save(group, presets, &error)) {
            m_status->setText(error);
            return;
        }
        m_presets = presets;
        updatePresets();
        m_saved->setCurrentIndex(index);
        m_status->setText(i18nc("@info", "Rename preset updated."));
    });
    connect(deletePreset, &QPushButton::clicked, this, [this, refreshDefault] {
        const int index = m_saved->currentIndex();
        if (index < 0 || index >= m_presets.size()) {
            return;
        }
        const QString name = m_presets.at(index).name;
        if (QMessageBox::question(this,
                                  i18nc("@title:window", "Delete Rename Preset"),
                                  i18nc("@info", "Delete the preset “%1”?", name),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
        QString error;
        auto group = m_config->group(QStringLiteral("BatchRenamePresets"));
        auto presets = BatchRenamePresets::load(group, &error);
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        const auto it = std::find_if(presets.begin(), presets.end(), [&name](const auto &preset) {
            return preset.name == name;
        });
        if (it == presets.end()) {
            m_presets = presets;
            updatePresets();
            refreshDefault();
            m_status->setText(i18nc("@info", "This preset no longer exists."));
            return;
        }
        presets.erase(it);
        const bool wasDefault = group.readEntry("DefaultPreset", QString()) == name;
        if (wasDefault) {
            group.deleteEntry("DefaultPreset");
        }
        if (!BatchRenamePresets::save(group, presets, &error)) {
            if (wasDefault) {
                group.writeEntry("DefaultPreset", name);
            }
            m_status->setText(error);
            return;
        }
        m_presets = presets;
        updatePresets();
        refreshDefault();
        m_status->setText(i18nc("@info", "Rename preset deleted."));
    });
    m_results->setObjectName(QStringLiteral("batchRenameResults"));
    m_results->setVisible(!manageOnly);
    m_results->setRootIsDecorated(false);
    m_results->setColumnCount(3);
    m_results->setHeaderLabels({i18nc("@title:column", "Current path"), i18nc("@title:column", "New path"), i18nc("@title:column", "Problem")});
    for (int i = 0; i < 3; ++i) {
        m_results->header()->setSectionResizeMode(i, QHeaderView::Stretch);
    }
    layout->addWidget(m_results);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_preview->setObjectName(QStringLiteral("batchRenamePreview"));
    m_apply->setObjectName(QStringLiteral("batchRenameApply"));
    m_cancel->setObjectName(QStringLiteral("batchRenameCancel"));
    m_apply->setEnabled(false);
    m_cancel->setEnabled(false);
    buttons->addButton(m_preview, QDialogButtonBox::ActionRole);
    buttons->addButton(m_apply, QDialogButtonBox::ActionRole);
    buttons->addButton(m_cancel, QDialogButtonBox::ActionRole);
    m_preview->setVisible(!manageOnly);
    m_apply->setVisible(!manageOnly);
    m_cancel->setVisible(!manageOnly);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &BatchRenameDialog::reject);
    connect(m_preview, &QPushButton::clicked, this, &BatchRenameDialog::preview);
    connect(m_apply, &QPushButton::clicked, this, &BatchRenameDialog::apply);
    connect(m_cancel, &QPushButton::clicked, this, [this] {
        m_canceled->store(true);
        m_cancel->setEnabled(false);
        m_status->setText(i18nc("@info", "Canceling; completed renames are not undone."));
    });
    connect(m_pattern, &QLineEdit::textChanged, this, &BatchRenameDialog::invalidatePreview);
    connect(m_replacement, &QLineEdit::textChanged, this, &BatchRenameDialog::invalidatePreview);
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_replacement->setEnabled(index == 1);
        m_startNumber->setEnabled(index == 0);
        m_padding->setEnabled(index == 0);
        invalidatePreview();
    });
    connect(m_startNumber, &QSpinBox::valueChanged, this, &BatchRenameDialog::invalidatePreview);
    connect(m_padding, &QSpinBox::valueChanged, this, &BatchRenameDialog::invalidatePreview);
    connect(&m_previewWatcher, &QFutureWatcher<BatchRename::Plan>::finished, this, [this] {
        m_plan = m_previewWatcher.result();
        m_results->clear();
        for (const BatchRename::Row &row : std::as_const(m_plan.rows)) {
            new QTreeWidgetItem(m_results, {row.source, row.destination, row.error});
        }
        m_options->setEnabled(true);
        m_preview->setEnabled(true);
        m_apply->setEnabled(m_plan.valid());
        m_status->setText(!m_plan.error.isEmpty() ? m_plan.error
                              : m_plan.valid()    ? i18nc("@info", "Review the preview before renaming.")
                                                  : i18nc("@info", "Resolve the problems in the preview before renaming."));
    });
    connect(&m_applyWatcher, &QFutureWatcher<BatchRename::Result>::finished, this, [this] {
        const auto result = m_applyWatcher.result();
        if (m_applyJob) {
            if (m_canceled && m_canceled->load() && result.error.isEmpty()) {
                m_applyJob->cancel();
            } else if (!result.error.isEmpty()) {
                m_applyJob->finish(i18np("Renaming stopped after one item. Details are shown in Plasma Opus.",
                                         "Renaming stopped after %1 items. Details are shown in Plasma Opus.",
                                         result.renamed.size()));
            }
        }
        m_cancel->setEnabled(false);
        m_apply->setEnabled(false);
        QList<LibraryMoves::Move> moves;
        for (const auto &row : std::as_const(m_plan.rows)) {
            if (row.source != row.destination && result.renamed.contains(row.destination)) {
                moves.append({row.source, row.destination, row.device, row.inode});
            }
        }
        LibraryMoveTracker::instance()->apply(moves);
        for (int i = 0; i < m_plan.rows.size(); ++i) {
            const auto &row = m_plan.rows.at(i);
            const QString state = result.renamed.contains(row.destination) ? i18nc("@info:status", "Renamed")
                : row.source == row.destination                            ? i18nc("@info:status", "Unchanged")
                                                                           : i18nc("@info:status", "Not renamed");
            m_results->topLevelItem(i)->setText(2, state);
        }
        m_status->setText(
            result.error.isEmpty()
                ? i18np("Renamed %1 item.", "Renamed %1 items.", result.renamed.size())
                : i18nc("@info", "%1 Completed renames: %2. Close and review the filesystem before trying again.", result.error, result.renamed.size()));
    });
    if (!manageOnly && presetError.isEmpty()) {
        const QString name = m_config->group(QStringLiteral("BatchRenamePresets")).readEntry("DefaultPreset", QString());
        if (!name.isEmpty()) {
            const auto it = std::find_if(m_presets.cbegin(), m_presets.cend(), [&name](const auto &preset) {
                return preset.name == name;
            });
            if (it == m_presets.cend()) {
                m_status->setText(i18nc("@info", "The default rename preset no longer exists. Choose another preset."));
            } else {
                m_saved->setCurrentIndex(int(it - m_presets.cbegin()));
                setRule(it->rule);
                m_status->setText(i18nc("@info", "Default preset loaded. Review it and preview before renaming."));
            }
        }
    }
}

BatchRenameDialog::~BatchRenameDialog()
{
    if (m_canceled) {
        m_canceled->store(true);
    }
}

void BatchRenameDialog::reject()
{
    if (m_applyWatcher.isRunning()) {
        m_canceled->store(true);
        m_status->setText(i18nc("@info", "Canceling; close this window after the operation stops."));
        return;
    }
    QDialog::reject();
}

void BatchRenameDialog::invalidatePreview()
{
    m_plan = {};
    m_apply->setEnabled(false);
    m_results->clear();
    m_status->setText(i18nc("@info", "Preview the updated pattern before renaming."));
}

BatchRename::Rule BatchRenameDialog::rule() const
{
    return {m_pattern->text(), m_replacement->text(), m_mode->currentIndex() == 1, m_startNumber->value(), m_padding->value()};
}

void BatchRenameDialog::setRule(const BatchRename::Rule &saved)
{
    m_mode->setCurrentIndex(saved.regularExpression ? 1 : 0);
    m_pattern->setText(saved.pattern);
    m_replacement->setText(saved.replacement);
    m_startNumber->setValue(saved.start);
    m_padding->setValue(saved.padding);
}

void BatchRenameDialog::updatePresets()
{
    m_saved->clear();
    for (const auto &preset : std::as_const(m_presets)) {
        m_saved->addItem(preset.name);
    }
}

void BatchRenameDialog::preview()
{
    const QStringList paths = m_paths;
    const auto currentRule = rule();
    m_options->setEnabled(false);
    m_preview->setEnabled(false);
    m_apply->setEnabled(false);
    m_status->setText(i18nc("@info", "Preparing preview…"));
    m_previewWatcher.setFuture(QtConcurrent::run([paths, currentRule] {
        const QString pattern = currentRule.pattern;
        const bool metadataNeeded = !currentRule.regularExpression
            && (pattern.contains(QStringLiteral("{title}")) || pattern.contains(QStringLiteral("{artist}")) || pattern.contains(QStringLiteral("{album}"))
                || pattern.contains(QStringLiteral("{year}")) || pattern.contains(QStringLiteral("{date}")) || pattern.contains(QStringLiteral("{camera}")));
        QMimeDatabase mime;
        QList<BatchRename::Input> inputs;
        for (const QString &path : paths) {
            const QFileInfo info(path);
            KFileMetaData::PropertyMultiMap metadata;
            if (metadataNeeded && info.isFile() && !info.isSymLink()) {
                metadata = ImageDetails::extractMetadata(path, mime.mimeTypeForFile(path).name());
            }
            inputs.append({path, metadata});
        }
        return BatchRename::plan(inputs, currentRule);
    }));
}

void BatchRenameDialog::apply()
{
    if (!m_plan.valid() || m_previewWatcher.isRunning() || m_applyWatcher.isRunning()) {
        return;
    }
    if (QMessageBox::question(
            this,
            i18nc("@title:window", "Confirm Batch Rename"),
            i18nc("@info",
                  "Rename the previewed items? Completed renames cannot be undone by this tool. "
                  "Folder labels, colors, ratings, notes and pins remain attached to the old names; tags in libraries follow the renamed items."),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    const auto plan = m_plan;
    m_canceled = std::make_shared<std::atomic_bool>(false);
    const auto canceled = m_canceled;
    m_options->setEnabled(false);
    m_preview->setEnabled(false);
    m_apply->setEnabled(false);
    m_cancel->setEnabled(true);
    m_status->setText(i18nc("@info", "Renaming…"));
    m_applyWatcher.setFuture(QtConcurrent::run([plan, canceled] {
        return BatchRename::apply(plan, [canceled] {
            return canceled->load();
        });
    }));
    m_applyJob = OperationJob::track(new OperationJob(OperationJob::Kind::BatchRename, i18nc("@title job", "Renaming items")), &m_applyWatcher);
    if (!plan.rows.isEmpty()) {
        m_applyJob->setSource(QUrl::fromLocalFile(QFileInfo(plan.rows.first().source).absolutePath()));
    }
    m_applyJob->setCancelHandler([button = QPointer<QPushButton>(m_cancel)] {
        if (button && button->isEnabled()) {
            button->click();
        }
    });
}
