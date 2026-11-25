// SPDX-License-Identifier: GPL-3.0-or-later

#include "embeddedmetadatasettings.h"

#include "embeddedmetadata.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QHeaderView>
#include <QIcon>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
constexpr int KeyRole = Qt::UserRole;

const QString TagsTarget = QStringLiteral("tags");
const QString FieldPrefix = QStringLiteral("field:");

/// The @p keys that are, or with @p shared unset aren't, also in @p other.
QStringList sharedKeys(const QStringList &keys, const QStringList &other, bool shared)
{
    QStringList result;
    for (const QString &key : keys) {
        if (other.contains(key) == shared) {
            result.append(key);
        }
    }
    return result;
}
}

EmbeddedMetadataKeyList::EmbeddedMetadataKeyList(QWidget *parent)
    : QWidget(parent)
    , m_tree(new QTreeWidget(this))
{
    m_tree->setObjectName(QStringLiteral("embeddedMetadataKeyTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setAccessibleName(i18nc("@info:accessible", "Embedded metadata properties to read"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_tree);

    const QStringList images = EmbeddedMetadata::imageKeys();
    const QStringList audio = EmbeddedMetadata::audioKeys();
    const QList<std::pair<QString, QStringList>> groups{
        {i18nc("@item:inlistbox embedded metadata group", "Images"), sharedKeys(images, audio, false)},
        {i18nc("@item:inlistbox embedded metadata group", "Audio"), sharedKeys(audio, images, false)},
        {i18nc("@item:inlistbox embedded metadata group", "Images and Audio"), sharedKeys(images, audio, true)},
    };
    for (const auto &[title, keys] : groups) {
        if (keys.isEmpty()) {
            continue;
        }
        auto *heading = new QTreeWidgetItem(m_tree, {title});
        heading->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
        for (const QString &key : keys) {
            auto *item = new QTreeWidgetItem(heading, {EmbeddedMetadata::displayName(key)});
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Checked);
            item->setData(0, KeyRole, key);
            item->setToolTip(0, i18nc("@info:tooltip %1 is a property name such as photoFNumber", "Property name: %1", key));
            m_items.insert(key, item);
        }
        heading->setExpanded(true);
    }
    m_tree->setMinimumHeight(220);
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int) {
        if (item->data(0, KeyRole).isValid()) {
            Q_EMIT disabledKeysChanged();
        }
    });
}

QStringList EmbeddedMetadataKeyList::disabledKeys() const
{
    QStringList keys;
    for (const QString &key : EmbeddedMetadata::keys()) {
        if (!isKeyEnabled(key)) {
            keys.append(key);
        }
    }
    return keys + m_unknown;
}

void EmbeddedMetadataKeyList::setDisabledKeys(const QStringList &keys)
{
    {
        const QSignalBlocker blocker(m_tree);
        for (auto it = m_items.cbegin(); it != m_items.cend(); ++it) {
            it.value()->setCheckState(0, keys.contains(it.key()) ? Qt::Unchecked : Qt::Checked);
        }
    }
    m_unknown.clear();
    for (const QString &key : keys) {
        if (!m_items.contains(key) && !m_unknown.contains(key)) {
            m_unknown.append(key);
        }
    }
    Q_EMIT disabledKeysChanged();
}

QStringList EmbeddedMetadataKeyList::enabledKeys() const
{
    QStringList keys;
    for (const QString &key : EmbeddedMetadata::keys()) {
        if (isKeyEnabled(key)) {
            keys.append(key);
        }
    }
    return keys;
}

bool EmbeddedMetadataKeyList::isKeyEnabled(const QString &key) const
{
    const QTreeWidgetItem *item = m_items.value(key);
    return item && item->checkState(0) == Qt::Checked;
}

void EmbeddedMetadataKeyList::setKeyEnabled(const QString &key, bool enabled)
{
    if (QTreeWidgetItem *item = m_items.value(key)) {
        item->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
    }
}

EmbeddedMetadataMappingTable::EmbeddedMetadataMappingTable(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTreeWidget(this))
{
    m_table->setObjectName(QStringLiteral("embeddedMetadataMappingTable"));
    m_table->setHeaderLabels({i18nc("@title:column", "Property"), i18nc("@title:column", "Import As")});
    m_table->setRootIsDecorated(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setAccessibleName(i18nc("@info:accessible", "How embedded metadata properties are imported"));
    m_table->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->header()->setStretchLastSection(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_table);

    for (const QString &key : EmbeddedMetadata::keys()) {
        const QString name = EmbeddedMetadata::displayName(key);
        auto *row = new QTreeWidgetItem(m_table, {name});
        row->setToolTip(0, i18nc("@info:tooltip %1 is a property name such as photoFNumber", "Property name: %1", key));
        auto *combo = new QComboBox(m_table);
        combo->setObjectName(QStringLiteral("embeddedMetadataMapping_") + key);
        combo->setAccessibleName(name);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        m_table->setItemWidget(row, 1, combo);
        m_rows.insert(key, row);
        m_targets.insert(key, combo);
        fillTarget(key);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, key, combo] {
            const QString target = combo->currentData().toString();
            if (target.isEmpty()) {
                m_mappings.remove(key);
            } else if (target == TagsTarget) {
                m_mappings.insert(key, QString());
            } else {
                m_mappings.insert(key, target.mid(FieldPrefix.size()));
            }
            Q_EMIT mappingsChanged();
        });
    }
    m_table->setMinimumHeight(260);
}

QStringList EmbeddedMetadataMappingTable::mappings() const
{
    return MetadataImport::serializeMappings(m_mappings);
}

void EmbeddedMetadataMappingTable::setMappings(const QStringList &entries)
{
    m_mappings = MetadataImport::parseMappings(entries);
    for (const QString &key : EmbeddedMetadata::keys()) {
        fillTarget(key);
    }
    Q_EMIT mappingsChanged();
}

void EmbeddedMetadataMappingTable::setFields(const QList<TagLibrary::Field> &fields)
{
    m_fields = fields;
    for (const QString &key : EmbeddedMetadata::keys()) {
        fillTarget(key);
    }
}

void EmbeddedMetadataMappingTable::setVisibleKeys(const QStringList &keys)
{
    for (auto it = m_rows.cbegin(); it != m_rows.cend(); ++it) {
        it.value()->setHidden(!keys.contains(it.key()));
    }
}

bool EmbeddedMetadataMappingTable::hasVisibleMappings() const
{
    for (auto it = m_mappings.cbegin(); it != m_mappings.cend(); ++it) {
        const QTreeWidgetItem *row = m_rows.value(it.key());
        if (row && !row->isHidden()) {
            return true;
        }
    }
    return false;
}

QComboBox *EmbeddedMetadataMappingTable::target(const QString &key) const
{
    return m_targets.value(key);
}

void EmbeddedMetadataMappingTable::fillTarget(const QString &key)
{
    QComboBox *combo = m_targets.value(key);
    if (!combo) {
        return;
    }
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(i18nc("@item:inlistbox embedded metadata target", "Not imported"), QString());
    combo->addItem(QIcon::fromTheme(QStringLiteral("tag")), i18nc("@item:inlistbox embedded metadata target", "Tags"), TagsTarget);
    QStringList names;
    for (const TagLibrary::Field &field : std::as_const(m_fields)) {
        if (MetadataImport::canMap(key, field.type) && !names.contains(field.name, Qt::CaseInsensitive)) {
            names.append(field.name);
        }
    }
    const auto saved = m_mappings.constFind(key);
    const bool savedField = saved != m_mappings.cend() && !saved->isEmpty();
    if (savedField && !names.contains(*saved, Qt::CaseInsensitive)) {
        names.append(*saved);
    }
    for (const QString &name : std::as_const(names)) {
        combo->addItem(i18nc("@item:inlistbox %1 is a field name", "Field “%1”", name), QString(FieldPrefix + name));
        if (savedField && name.compare(*saved, Qt::CaseInsensitive) == 0) {
            combo->setCurrentIndex(combo->count() - 1);
        }
    }
    if (saved == m_mappings.cend()) {
        combo->setCurrentIndex(0);
    } else if (!savedField) {
        combo->setCurrentIndex(1);
    }
    combo->setToolTip(i18nc("@info:tooltip",
                            "Fields are matched by name in each library, ignoring case. Libraries without a suitable field of this name skip "
                            "the property."));
}
