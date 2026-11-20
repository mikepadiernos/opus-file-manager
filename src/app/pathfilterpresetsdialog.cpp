// SPDX-License-Identifier: GPL-3.0-or-later

#include "pathfilterpresetsdialog.h"

#include "pathfiltereditor.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

PathFilterPresetsDialog::PathFilterPresetsDialog(QWidget *parent, KSharedConfig::Ptr config)
    : QDialog(parent)
    , m_config(config ? config : KSharedConfig::openConfig())
    , m_list(new QComboBox(this))
    , m_name(new QLineEdit(this))
    , m_editor(new PathFilterEditor(QStringLiteral("preset"), this, m_config, false))
    , m_status(new QLabel(this))
{
    setObjectName(QStringLiteral("pathFilterPresetsDialog"));
    setWindowTitle(i18nc("@title:window", "Manage Path Filters"));
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(i18nc("@info",
                                    "Filters are shared by search and selection. Edits are saved only with Save Filter. Saved or deleted filters take effect "
                                    "immediately, even if Preferences is canceled."),
                              this);
    notice->setWordWrap(true);
    layout->addWidget(notice);
    auto *row = new QHBoxLayout;
    m_list->setObjectName(QStringLiteral("pathFilterPresetList"));
    auto *newButton = new QPushButton(i18nc("@action:button", "New"), this);
    newButton->setObjectName(QStringLiteral("pathFilterPresetNew"));
    row->addWidget(m_list, 1);
    row->addWidget(newButton);
    layout->addLayout(row);
    auto *form = new QFormLayout;
    m_name->setObjectName(QStringLiteral("pathFilterPresetName"));
    form->addRow(i18nc("@label:textbox", "Name:"), m_name);
    layout->addLayout(form);
    layout->addWidget(m_editor);
    auto *actions = new QHBoxLayout;
    auto *saveButton = new QPushButton(i18nc("@action:button", "Save Filter"), this);
    saveButton->setObjectName(QStringLiteral("pathFilterPresetSave"));
    auto *deleteButton = new QPushButton(i18nc("@action:button", "Delete Filter"), this);
    deleteButton->setObjectName(QStringLiteral("pathFilterPresetDelete"));
    actions->addWidget(saveButton);
    actions->addWidget(deleteButton);
    actions->addStretch();
    layout->addLayout(actions);
    m_status->setObjectName(QStringLiteral("pathFilterPresetStatus"));
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!discardChanges()) {
            const QSignalBlocker blocker(m_list);
            m_list->setCurrentIndex(m_selectedIndex);
            return;
        }
        m_selectedIndex = index;
        if (index >= 0 && index < m_presets.size()) {
            m_name->setText(m_presets.at(index).name);
            m_editor->setFilter(m_presets.at(index).filter);
        }
    });
    connect(newButton, &QPushButton::clicked, this, [this] {
        if (!discardChanges()) {
            return;
        }
        const QSignalBlocker blocker(m_list);
        m_list->setCurrentIndex(-1);
        m_selectedIndex = -1;
        m_name->clear();
        m_editor->setFilter({});
        m_status->clear();
    });
    connect(saveButton, &QPushButton::clicked, this, &PathFilterPresetsDialog::save);
    connect(deleteButton, &QPushButton::clicked, this, &PathFilterPresetsDialog::remove);
    reload();
}

void PathFilterPresetsDialog::reload(const QString &selected)
{
    QString error;
    const auto presets = SavedPathFilters::load(m_config->group(QStringLiteral("SavedPathFilters")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    m_presets = presets;
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const auto &preset : m_presets) {
        m_list->addItem(preset.name);
    }
    const int index = selected.isEmpty() ? (m_presets.isEmpty() ? -1 : 0) : m_list->findText(selected);
    m_list->setCurrentIndex(index);
    m_selectedIndex = index;
    if (index >= 0) {
        m_name->setText(m_presets.at(index).name);
        m_editor->setFilter(m_presets.at(index).filter);
    } else {
        m_name->clear();
        m_editor->setFilter({});
    }
}

bool PathFilterPresetsDialog::discardChanges()
{
    const SavedPathFilters::Preset edited{m_name->text(), m_editor->filter()};
    const SavedPathFilters::Preset original = m_selectedIndex >= 0 ? m_presets.at(m_selectedIndex) : SavedPathFilters::Preset{};
    return edited == original
        || QMessageBox::question(this, i18nc("@title:window", "Discard Filter Changes"), i18nc("@info", "Discard unsaved changes to this filter?"))
        == QMessageBox::Yes;
}

void PathFilterPresetsDialog::reject()
{
    if (discardChanges()) {
        QDialog::reject();
    }
}

bool PathFilterPresetsDialog::currentPresets(QList<SavedPathFilters::Preset> *presets)
{
    QString error;
    *presets = SavedPathFilters::load(m_config->group(QStringLiteral("SavedPathFilters")), &error);
    if (!error.isEmpty()) {
        m_status->setText(error);
        return false;
    }
    const int index = m_list->currentIndex();
    if (index >= 0) {
        const auto &original = m_presets.at(index);
        const auto it = std::find_if(presets->cbegin(), presets->cend(), [&original](const auto &preset) {
            return preset.name == original.name;
        });
        if (it == presets->cend() || *it != original) {
            m_status->setText(i18nc("@info", "This filter changed elsewhere. Close and reopen the manager before editing it."));
            return false;
        }
    }
    return true;
}

void PathFilterPresetsDialog::save()
{
    const QString name = m_name->text().trimmed();
    const PathFilter filter = m_editor->filter();
    const QString filterError = filter.error();
    if (!filterError.isEmpty()) {
        m_status->setText(filterError);
        return;
    }
    QList<SavedPathFilters::Preset> updated;
    if (!currentPresets(&updated)) {
        return;
    }
    const int index = m_list->currentIndex();
    if (index >= 0) {
        const QString original = m_presets.at(index).name;
        const auto it = std::find_if(updated.begin(), updated.end(), [&original](const auto &preset) {
            return preset.name == original;
        });
        updated.erase(it);
    }
    updated.append({name, filter});
    QString error;
    if (!SavedPathFilters::save(m_config->group(QStringLiteral("SavedPathFilters")), updated, &error)) {
        m_status->setText(error);
        return;
    }
    reload(name);
    m_status->setText(i18nc("@info", "Filter saved."));
}

void PathFilterPresetsDialog::remove()
{
    const int index = m_list->currentIndex();
    if (index < 0) {
        m_status->setText(i18nc("@info", "Select a saved filter to delete."));
        return;
    }
    const QString name = m_presets.at(index).name;
    if (QMessageBox::question(this, i18nc("@title:window", "Delete Path Filter"), i18nc("@info", "Delete the saved filter “%1”?", name)) != QMessageBox::Yes) {
        return;
    }
    QList<SavedPathFilters::Preset> updated;
    if (!currentPresets(&updated)) {
        return;
    }
    const auto it = std::find_if(updated.begin(), updated.end(), [&name](const auto &preset) {
        return preset.name == name;
    });
    updated.erase(it);
    QString error;
    if (!SavedPathFilters::save(m_config->group(QStringLiteral("SavedPathFilters")), updated, &error)) {
        m_status->setText(error);
        return;
    }
    reload();
    m_status->setText(i18nc("@info", "Filter deleted."));
}
