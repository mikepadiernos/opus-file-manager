// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "metadataimport.h"
#include "taglibrary.h"

#include <QHash>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * Checks which embedded metadata properties are read, grouped under Images,
 * Audio, and Images and Audio for the properties both have. Bound to the
 * DisabledEmbeddedMetadataKeys setting.
 */
class EmbeddedMetadataKeyList : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList disabledKeys READ disabledKeys WRITE setDisabledKeys NOTIFY disabledKeysChanged USER true)

public:
    explicit EmbeddedMetadataKeyList(QWidget *parent = nullptr);

    /// The unchecked properties in EmbeddedMetadata::keys() order, followed by unknown keys that were set.
    QStringList disabledKeys() const;
    void setDisabledKeys(const QStringList &keys);
    /// The checked properties in EmbeddedMetadata::keys() order.
    QStringList enabledKeys() const;

    bool isKeyEnabled(const QString &key) const;
    void setKeyEnabled(const QString &key, bool enabled);

Q_SIGNALS:
    void disabledKeysChanged();

private:
    QTreeWidget *m_tree;
    QHash<QString, QTreeWidgetItem *> m_items;
    /// Disabled keys this version doesn't read, kept so they are saved again.
    QStringList m_unknown;
};

/**
 * Chooses how each embedded metadata property is imported: not at all, as
 * tags, or into a field chosen by name. Field names are matched in each
 * library when importing. Bound to the EmbeddedMetadataMappings setting.
 */
class EmbeddedMetadataMappingTable : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QStringList mappings READ mappings WRITE setMappings NOTIFY mappingsChanged USER true)

public:
    explicit EmbeddedMetadataMappingTable(QWidget *parent = nullptr);

    /// The mappings as saved, in EmbeddedMetadata::keys() order.
    QStringList mappings() const;
    void setMappings(const QStringList &entries);

    /**
     * Offers the fields named in @p fields as targets; each name is offered for
     * the properties a field of that name and type can hold. Names repeated
     * ignoring case are offered once.
     */
    void setFields(const QList<TagLibrary::Field> &fields);
    /// Shows the rows of @p keys only; the mappings of hidden properties are kept.
    void setVisibleKeys(const QStringList &keys);
    /// Whether a shown property is mapped.
    bool hasVisibleMappings() const;

    /// The target choice of property @p key.
    QComboBox *target(const QString &key) const;

Q_SIGNALS:
    void mappingsChanged();

private:
    void fillTarget(const QString &key);

    QTreeWidget *m_table;
    QHash<QString, QTreeWidgetItem *> m_rows;
    QHash<QString, QComboBox *> m_targets;
    MetadataImport::NamedMappings m_mappings;
    QList<TagLibrary::Field> m_fields;
};
