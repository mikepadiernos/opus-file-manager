// SPDX-License-Identifier: GPL-3.0-or-later

#include "pathfiltereditor.h"

#include "savedpathfilters.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>

#include <limits>

PathFilterEditor::PathFilterEditor(const QString &objectPrefix, QWidget *parent, KSharedConfig::Ptr config, bool showPresets)
    : QWidget(parent)
    , m_path(new QLineEdit(this))
    , m_parent(new QLineEdit(this))
    , m_excluded(new QPlainTextEdit(this))
    , m_depth(new QSpinBox(this))
{
    m_path->setObjectName(objectPrefix + QStringLiteral("PathFilter"));
    m_parent->setObjectName(objectPrefix + QStringLiteral("ParentFilter"));
    m_excluded->setObjectName(objectPrefix + QStringLiteral("ExcludedFolders"));
    m_excluded->setPlaceholderText(i18nc("@info:placeholder", "One absolute local folder path per line"));
    m_excluded->setMaximumHeight(m_excluded->fontMetrics().lineSpacing() * 3);
    m_depth->setObjectName(objectPrefix + QStringLiteral("MaxDepth"));
    m_depth->setRange(-1, std::numeric_limits<int>::max());
    m_depth->setSpecialValueText(i18nc("@item:inlistbox", "Unlimited"));
    m_depth->setValue(-1);
    auto *form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    if (showPresets) {
        const auto store = config ? config : KSharedConfig::openConfig();
        auto *row = new QHBoxLayout;
        auto *presets = new QComboBox(this);
        presets->setObjectName(objectPrefix + QStringLiteral("FilterPresets"));
        auto *load = new QPushButton(i18nc("@action:button", "Load"), this);
        load->setObjectName(objectPrefix + QStringLiteral("LoadFilterPreset"));
        auto *refresh = new QPushButton(i18nc("@action:button", "Refresh"), this);
        refresh->setObjectName(objectPrefix + QStringLiteral("RefreshFilterPresets"));
        row->addWidget(presets, 1);
        row->addWidget(load);
        row->addWidget(refresh);
        form->addRow(i18nc("@label:listbox", "Saved path filter:"), row);
        auto *status = new QLabel(this);
        status->setObjectName(objectPrefix + QStringLiteral("FilterPresetsStatus"));
        status->setTextFormat(Qt::PlainText);
        status->setWordWrap(true);
        form->addRow(status);
        const auto update = [store, presets, status, load] {
            QString error;
            const auto filters = SavedPathFilters::load(store->group(QStringLiteral("SavedPathFilters")), &error);
            const QString selected = presets->currentText();
            presets->clear();
            for (const auto &preset : filters) {
                presets->addItem(preset.name);
            }
            const int index = presets->findText(selected);
            if (index >= 0) {
                presets->setCurrentIndex(index);
            }
            load->setEnabled(!filters.isEmpty());
            status->setText(error);
        };
        connect(refresh, &QPushButton::clicked, this, update);
        connect(load, &QPushButton::clicked, this, [this, store, presets, status] {
            const QString name = presets->currentText();
            QString error;
            const auto filters = SavedPathFilters::load(store->group(QStringLiteral("SavedPathFilters")), &error);
            if (!error.isEmpty()) {
                status->setText(error);
                return;
            }
            for (const auto &preset : filters) {
                if (preset.name == name) {
                    setFilter(preset.filter);
                    status->setText(i18nc("@info", "Filter loaded. Review its criteria before applying it."));
                    return;
                }
            }
            status->setText(i18nc("@info", "This saved filter no longer exists. Refresh the list."));
        });
        update();
    }
    form->addRow(i18nc("@label", "Full path contains:"), m_path);
    form->addRow(i18nc("@label", "Parent path contains:"), m_parent);
    form->addRow(i18nc("@label", "Exclude folders:"), m_excluded);
    form->addRow(i18nc("@label", "Maximum depth (0 = current folder):"), m_depth);
}

PathFilter PathFilterEditor::filter() const
{
    return {m_path->text(), m_parent->text(), m_excluded->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts), m_depth->value()};
}

void PathFilterEditor::setFilter(const PathFilter &filter)
{
    m_path->setText(filter.pathText);
    m_parent->setText(filter.parentText);
    m_excluded->setPlainText(filter.excludedFolders.join(QLatin1Char('\n')));
    m_depth->setValue(filter.maxDepth);
}
