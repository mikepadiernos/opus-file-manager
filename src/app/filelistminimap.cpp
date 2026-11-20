// SPDX-License-Identifier: GPL-3.0-or-later

#include "filelistminimap.h"

#include <KDirModel>
#include <KFileItem>
#include <KLocalizedString>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

namespace
{
constexpr int DefaultMinimapWidth = 14;
constexpr int MinimumMinimapWidth = 8;
constexpr int MaximumMinimapWidth = 64;
constexpr int MinimumViewportHeight = 8;
constexpr int MaximumSamples = 512;
}

FileListMinimap::FileListMinimap(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("fileListMinimap"));
    setAccessibleName(i18nc("@label", "File list minimap"));
    setToolTip(i18nc("@info:tooltip", "Click or drag to scroll the file list"));
    setFocusPolicy(Qt::NoFocus);
    setMinimapWidth(DefaultMinimapWidth);
    setMinimumHeight(MinimumViewportHeight);
}

void FileListMinimap::setMinimapWidth(int width)
{
    setFixedWidth(qBound(MinimumMinimapWidth, width, MaximumMinimapWidth));
}

void FileListMinimap::setView(QAbstractItemView *view)
{
    if (m_view == view) {
        return;
    }
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    if (m_scrollBar) {
        disconnect(m_scrollBar, nullptr, this, nullptr);
    }

    m_view = view;
    m_model = view ? view->model() : nullptr;
    m_scrollBar = view ? view->verticalScrollBar() : nullptr;
    if (m_model) {
        connect(m_model, &QAbstractItemModel::dataChanged, this, [this] {
            update();
        });
        connect(m_model, &QAbstractItemModel::layoutChanged, this, [this] {
            update();
        });
        connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
            update();
        });
        connect(m_model, &QAbstractItemModel::rowsInserted, this, [this] {
            update();
        });
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this] {
            update();
        });
    }
    if (m_scrollBar) {
        connect(m_scrollBar, &QScrollBar::valueChanged, this, [this] {
            update();
        });
        connect(m_scrollBar, &QScrollBar::rangeChanged, this, [this] {
            update();
        });
    }
    update();
}

void FileListMinimap::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QColor background = palette().color(QPalette::AlternateBase);
    painter.fillRect(rect(), background);

    if (m_model && height() > 0) {
        const int rows = m_model->rowCount();
        const int samples = qMin(rows, qMin(height(), MaximumSamples));
        const int stripeHeight = qMax(1, height() / qMax(1, samples));
        for (int sample = 0; sample < samples; ++sample) {
            const int row = int(qint64(sample) * rows / samples);
            const QModelIndex index = m_model->index(row, 0);
            const KFileItem item = index.data(KDirModel::FileItemRole).value<KFileItem>();
            QColor color = item.isNull() ? palette().color(QPalette::Mid) : palette().color(item.isDir() ? QPalette::Highlight : QPalette::Text);
            color.setAlpha(item.isDir() ? 200 : 110);
            const int y = sample * height() / samples;
            painter.fillRect(QRect(2, y, width() - 4, stripeHeight), color);
        }
    }

    if (!m_scrollBar) {
        return;
    }
    const int range = m_scrollBar->maximum();
    const int page = m_scrollBar->pageStep();
    const qint64 extent = qint64(range) + page;
    const int thumbHeight = extent > 0 ? qBound(MinimumViewportHeight, int(qint64(height()) * page / extent), height()) : height();
    const int travel = height() - thumbHeight;
    const int top = range > 0 ? int(qint64(travel) * m_scrollBar->value() / range) : 0;
    QColor viewport = palette().color(QPalette::Highlight);
    viewport.setAlpha(70);
    painter.fillRect(QRect(0, top, width(), thumbHeight), viewport);
    painter.setPen(palette().color(QPalette::Highlight));
    painter.drawRect(QRect(0, top, width() - 1, thumbHeight - 1));
}

void FileListMinimap::scrollToPosition(int y)
{
    if (!m_scrollBar || m_scrollBar->maximum() <= 0 || height() <= 0) {
        return;
    }
    const int page = m_scrollBar->pageStep();
    const qint64 extent = qint64(m_scrollBar->maximum()) + page;
    const int thumbHeight = extent > 0 ? qBound(MinimumViewportHeight, int(qint64(height()) * page / extent), height()) : height();
    const int travel = height() - thumbHeight;
    if (travel <= 0) {
        return;
    }
    const int top = qBound(0, y - thumbHeight / 2, travel);
    m_scrollBar->setValue(int(qint64(top) * m_scrollBar->maximum() / travel));
}

void FileListMinimap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        scrollToPosition(event->position().toPoint().y());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FileListMinimap::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons().testFlag(Qt::LeftButton)) {
        scrollToPosition(event->position().toPoint().y());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void FileListMinimap::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void FileListMinimap::wheelEvent(QWheelEvent *event)
{
    if (!m_scrollBar) {
        event->ignore();
        return;
    }
    const int delta = event->pixelDelta().y() != 0 ? event->pixelDelta().y() : event->angleDelta().y() * m_scrollBar->singleStep() / 120;
    m_scrollBar->setValue(m_scrollBar->value() - delta);
    event->accept();
}
