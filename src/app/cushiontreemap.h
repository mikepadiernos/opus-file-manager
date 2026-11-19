// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QRectF>
#include <QWidget>

class CushionTreemap : public QWidget
{
    Q_OBJECT

public:
    struct Item {
        QString name;
        quint64 size;
    };

    explicit CushionTreemap(QWidget *parent = nullptr);
    void setItems(const QList<Item> &items);
    QList<QRectF> tiles() const;

Q_SIGNALS:
    void activated(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;

private:
    int itemAt(const QPointF &pos) const;
    void updateLayout();
    QList<Item> m_items;
    QList<QRectF> m_tiles;
};
