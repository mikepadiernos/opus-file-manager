// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QTreeView>

class QRubberBand;

/**
 * The details view. Unless dragging from anywhere is on, pressing on a row
 * outside its icon and name starts a selection rectangle instead of a drag,
 * as in Dolphin; a click there without moving selects the row.
 */
class DetailsView : public QTreeView
{
    Q_OBJECT

public:
    explicit DetailsView(QWidget *parent = nullptr);

    bool dragFromAnywhere() const;
    /// @param enabled true to drag items from any part of their row
    void setDragFromAnywhere(bool enabled);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    /// Whether @p pos lies on a row but outside its icon and name.
    bool isOnEmptyPartOfRow(const QPoint &pos) const;
    /// Selects every row the rectangle from the press to @p pos crosses.
    void selectRowsInBand(const QPoint &pos, bool keepSelection);

    bool m_dragFromAnywhere = false;
    QRubberBand *m_band = nullptr;
    QPoint m_bandOrigin;
    QModelIndex m_bandPressedIndex;
    bool m_bandPending = false;
    bool m_bandKeepsSelection = false;
};
