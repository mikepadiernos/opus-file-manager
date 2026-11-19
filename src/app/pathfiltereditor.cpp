// SPDX-License-Identifier: GPL-3.0-or-later

#include "pathfiltereditor.h"

#include <KLocalizedString>

#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>

#include <limits>

PathFilterEditor::PathFilterEditor(const QString &objectPrefix, QWidget *parent)
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
