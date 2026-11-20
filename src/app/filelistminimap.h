// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QAbstractItemModel;
class QAbstractItemView;
class QMouseEvent;
class QPaintEvent;
class QScrollBar;
class QWheelEvent;

class FileListMinimap : public QWidget
{
public:
    explicit FileListMinimap(QWidget *parent = nullptr);

    void setView(QAbstractItemView *view);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void scrollToPosition(int y);

    QAbstractItemView *m_view = nullptr;
    QAbstractItemModel *m_model = nullptr;
    QScrollBar *m_scrollBar = nullptr;
};
