// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>
#include <QStyledItemDelegate>

#include <functional>

/// Item data roles that the folder listing provides for cards.
namespace CardRoles
{
enum {
    /// The item's rating from 1 to 5, or nothing.
    Rating = Qt::UserRole + 0x4300,
    /// The stored key of the item's colour label, such as "red".
    Label,
    /// The item's KDE tags as a QStringList.
    Tags,
};
}

/**
 * Paints each item as a card with its preview or icon, its name and chosen
 * metadata, and renames items in place.
 */
class CardDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    static constexpr int Padding = 8;
    static constexpr int MinimumTextWidth = 120;
    static constexpr int NameLines = 2;

    explicit CardDelegate(QObject *parent = nullptr);

    int previewSize() const;
    void setPreviewSize(int size);
    /// Space around each card, inside its grid cell.
    void setItemSpace(int space);
    int itemSpace() const;
    /// Metadata keys shown below the name, in order.
    void setInformation(const QStringList &keys);
    QStringList information() const;
    /// Width of each grid cell, or 0 for the natural width.
    void setCellWidth(int width);
    void setSelectionMarkersVisible(std::function<bool()> visible);

    /// The cell size of a card, including the space around it.
    QSize cellSize(const QFont &font) const;
    /// The narrowest cell that still fits the preview.
    int minimumCellWidth() const;

    /// The text shown for one metadata key, or an empty string.
    static QString informationText(const QModelIndex &index, const QString &key);
    static QRect cardRect(const QRect &cell, int space);
    static QRect markerRect(const QRect &card, Qt::LayoutDirection direction);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QRect previewRect(const QRect &card) const;
    QRect nameRect(const QRect &card, const QFontMetrics &metrics) const;
    QStringList shownInformation(const QModelIndex &index) const;

    int m_previewSize = 96;
    int m_space = 2;
    int m_cellWidth = 0;
    QStringList m_information;
    std::function<bool()> m_markersVisible;
};
