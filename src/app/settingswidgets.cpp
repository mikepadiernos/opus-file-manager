// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingswidgets.h"

#include "filecolorrules.h"
#include "ignorelist.h"

#include <KIO/PreviewJob>
#include <KLocalizedString>
#include <KPluginMetaData>

#include <QColorDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace
{
constexpr qulonglong MiB = 1024 * 1024;
constexpr qulonglong NoLimit = std::numeric_limits<qulonglong>::max();
constexpr int MaximumMiB = 100000;

QStringList sanitizedRules(RuleListEditor::Kind kind, const QStringList &rules)
{
    if (kind == RuleListEditor::Kind::Ignore) {
        return IgnoreList(rules).entries();
    }
    if (kind == RuleListEditor::Kind::Color) {
        return FileColorRules(rules).entries();
    }
    QStringList result;
    for (const QString &rule : rules) {
        const QString pattern = rule.trimmed();
        if (!pattern.isEmpty() && !result.contains(pattern)) {
            result.append(pattern);
        }
    }
    return result;
}
}

RuleListEditor::RuleListEditor(Kind kind, QWidget *parent)
    : QWidget(parent)
    , m_kind(kind)
    , m_list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(i18nc("@action:button", "Add..."), this);
    auto *edit = new QPushButton(i18nc("@action:button", "Edit..."), this);
    auto *remove = new QPushButton(i18nc("@action:button", "Remove"), this);
    buttons->addWidget(add);
    buttons->addWidget(edit);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this] {
        editRule(-1);
    });
    connect(edit, &QPushButton::clicked, this, [this] {
        if (m_list->currentRow() >= 0) {
            editRule(m_list->currentRow());
        }
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        editRule(m_list->row(item));
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        if (row >= 0) {
            delete m_list->takeItem(row);
            Q_EMIT rulesChanged();
        }
    });
}

QStringList RuleListEditor::rules() const
{
    QStringList result;
    for (int row = 0; row < m_list->count(); ++row) {
        result.append(m_list->item(row)->data(Qt::UserRole).toString());
    }
    return result;
}

void RuleListEditor::setRules(const QStringList &rules)
{
    m_list->clear();
    const QStringList sanitized = sanitizedRules(m_kind, rules);
    for (const QString &rule : sanitized) {
        auto *item = new QListWidgetItem(rule, m_list);
        item->setData(Qt::UserRole, rule);
        if (m_kind == Kind::Color) {
            item->setForeground(QColor(rule.sliced(rule.indexOf(u'=') + 1)));
        }
    }
    Q_EMIT rulesChanged();
}

void RuleListEditor::editRule(int row)
{
    if (row < -1 || row >= m_list->count()) {
        return;
    }
    QStringList current = rules();
    const QString old = row >= 0 ? current.takeAt(row) : QString();
    bool accepted = false;
    const QString label = m_kind == Kind::Color ? i18nc("@label:textbox", "File extension:") : i18nc("@label:textbox", "File name or glob pattern:");
    const QString initial = m_kind == Kind::Color ? old.section(u'=', 0, 0) : old;
    QString value = QInputDialog::getText(this, i18nc("@title:window", "Edit Rule"), label, QLineEdit::Normal, initial, &accepted).trimmed();
    if (!accepted) {
        return;
    }
    if (m_kind == Kind::Color) {
        const QColor previous = row < 0 ? QColor(Qt::black) : QColor(old.section(u'=', 1));
        const QColor color = QColorDialog::getColor(previous, this, i18nc("@title:window", "File Colour"));
        if (!color.isValid()) {
            return;
        }
        value += u'=' + color.name(QColor::HexRgb);
    }
    current.append(value);
    const QStringList sanitized = sanitizedRules(m_kind, current);
    if (sanitized.size() != current.size()) {
        QMessageBox::warning(this, i18nc("@title:window", "Invalid Rule"), i18nc("@info", "The rule is invalid, duplicated, or the list is full."));
        return;
    }
    setRules(sanitized);
}

IconInformationList::IconInformationList(QWidget *parent)
    : QListWidget(parent)
{
    const QList<QPair<QString, QString>> options{
        {QStringLiteral("size"), i18nc("@option:check", "Size")},
        {QStringLiteral("permissions"), i18nc("@option:check", "Permissions")},
        {QStringLiteral("octalPermissions"), i18nc("@option:check", "Permissions (octal)")},
        {QStringLiteral("owner"), i18nc("@option:check", "Owner")},
        {QStringLiteral("ownerAndGroup"), i18nc("@option:check", "Owner and group")},
        {QStringLiteral("creationTime"), i18nc("@option:check", "Created")},
        {QStringLiteral("modificationTime"), i18nc("@option:check", "Modified")},
        {QStringLiteral("accessTime"), i18nc("@option:check", "Accessed")},
        {QStringLiteral("mimeType"), i18nc("@option:check", "MIME type")},
        {QStringLiteral("friendlyMimeType"), i18nc("@option:check", "File type")},
        {QStringLiteral("linkDest"), i18nc("@option:check", "Link target")},
        {QStringLiteral("localPathOrUrl"), i18nc("@option:check", "Path or URL")},
        {QStringLiteral("comment"), i18nc("@option:check", "Comment")},
    };
    for (const auto &[key, label] : options) {
        auto *item = new QListWidgetItem(label, this);
        item->setData(Qt::UserRole, key);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    connect(this, &QListWidget::itemChanged, this, &IconInformationList::informationChanged);
}

QStringList IconInformationList::information() const
{
    QStringList result;
    for (int row = 0; row < count(); ++row) {
        if (item(row)->checkState() == Qt::Checked) {
            result.append(item(row)->data(Qt::UserRole).toString());
        }
    }
    return result;
}

void IconInformationList::setInformation(const QStringList &information)
{
    for (int row = 0; row < count(); ++row) {
        item(row)->setCheckState(information.contains(item(row)->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
    }
}

HiddenItemsList::HiddenItemsList(QWidget *parent)
    : QListWidget(parent)
{
    connect(this, &QListWidget::itemChanged, this, &HiddenItemsList::hiddenItemsChanged);
}

void HiddenItemsList::addItem(const QString &id, const QString &label)
{
    auto *entry = new QListWidgetItem(label, this);
    entry->setData(Qt::UserRole, id);
    entry->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    entry->setCheckState(Qt::Checked);
}

QStringList HiddenItemsList::hiddenItems() const
{
    QStringList hidden = m_missingItems;
    for (int row = 0; row < count(); ++row) {
        if (item(row)->checkState() == Qt::Unchecked) {
            hidden.append(item(row)->data(Qt::UserRole).toString());
        }
    }
    return hidden;
}

void HiddenItemsList::setHiddenItems(const QStringList &hidden)
{
    const QSignalBlocker blocker(this);
    m_missingItems = hidden;
    for (int row = 0; row < count(); ++row) {
        const QString id = item(row)->data(Qt::UserRole).toString();
        item(row)->setCheckState(hidden.contains(id) ? Qt::Unchecked : Qt::Checked);
        m_missingItems.removeAll(id);
    }
}

PreviewPluginList::PreviewPluginList(QWidget *parent)
    : QListWidget(parent)
{
    QList<KPluginMetaData> available = KIO::PreviewJob::availableThumbnailerPlugins();
    std::sort(available.begin(), available.end(), [](const KPluginMetaData &a, const KPluginMetaData &b) {
        return QString::localeAwareCompare(a.name(), b.name()) < 0;
    });
    for (const KPluginMetaData &plugin : std::as_const(available)) {
        auto *entry = new QListWidgetItem(plugin.name(), this);
        entry->setData(Qt::UserRole, plugin.pluginId());
        entry->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        entry->setCheckState(Qt::Unchecked);
    }
    connect(this, &QListWidget::itemChanged, this, &PreviewPluginList::pluginsChanged);
}

QStringList PreviewPluginList::plugins() const
{
    QStringList ids;
    for (int row = 0; row < count(); ++row) {
        if (item(row)->checkState() == Qt::Checked) {
            ids.append(item(row)->data(Qt::UserRole).toString());
        }
    }
    return ids;
}

void PreviewPluginList::setPlugins(const QStringList &plugins)
{
    for (int row = 0; row < count(); ++row) {
        const bool enabled = plugins.contains(item(row)->data(Qt::UserRole).toString());
        item(row)->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
}

PreviewSizeSpinBox::PreviewSizeSpinBox(QWidget *parent)
    : QSpinBox(parent)
{
    setRange(0, MaximumMiB);
    setSuffix(i18nc("@item:valuesuffix mebibytes", " MiB"));
    setSpecialValueText(i18nc("@item:inlistbox preview size limit", "No limit"));
    connect(this, &QSpinBox::valueChanged, this, &PreviewSizeSpinBox::bytesChanged);
}

qulonglong PreviewSizeSpinBox::bytes() const
{
    return value() == 0 ? NoLimit : qulonglong(value()) * MiB;
}

void PreviewSizeSpinBox::setBytes(qulonglong bytes)
{
    if (bytes == NoLimit) {
        setValue(0);
        return;
    }
    const qulonglong mib = std::max<qulonglong>(1, (bytes + MiB - 1) / MiB);
    setValue(int(std::min<qulonglong>(mib, MaximumMiB)));
}
