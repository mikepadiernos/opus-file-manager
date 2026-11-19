// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QTreeView>

class QRubberBand;

class DetailsView : public QTreeView
{
    Q_OBJECT

public:
    explicit DetailsView(QWidget *parent = nullptr);

    void setDragFromAnywhere(bool enabled);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool isOnEmptyPartOfRow(const QPoint &pos) const;
    void selectRowsInBand(const QPoint &pos, bool keepSelection);

    QRubberBand *m_band = nullptr;
    QPoint m_bandOrigin;
    QModelIndex m_bandPressedIndex;
    bool m_bandPending = false;
    bool m_bandKeepsSelection = false;
    bool m_dragFromAnywhere = false;
};
