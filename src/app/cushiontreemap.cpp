// SPDX-License-Identifier: GPL-3.0-or-later

#include "cushiontreemap.h"

#include "treemap.h"

#include <KIO/Global>
#include <KLocalizedString>

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QToolTip>

#include <algorithm>

CushionTreemap::CushionTreemap(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("contentTreemap"));
    setMinimumHeight(120);
    setAccessibleName(i18nc("@info", "Folder size treemap"));
}

void CushionTreemap::setItems(const QList<Item> &items)
{
    m_items = items;
    updateLayout();
}

QList<QRectF> CushionTreemap::tiles() const
{
    return m_tiles;
}

void CushionTreemap::updateLayout()
{
    QList<quint64> weights;
    weights.reserve(m_items.size());
    for (const Item &item : m_items) {
        weights.append(item.size);
    }
    m_tiles = Treemap::layout(weights, QRectF(rect()));
    update();
}

void CushionTreemap::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void CushionTreemap::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());
    const auto rectangles = tiles();
    for (qsizetype i = 0; i < rectangles.size(); ++i) {
        const QRectF tile = rectangles[i];
        if (tile.isEmpty()) {
            continue;
        }
        QColor color = palette().highlight().color();
        if (i % 2) {
            color = color.lighter(115);
        }
        QRadialGradient gradient(QPointF(0.5, 0.5), 0.71);
        gradient.setColorAt(0, color.lighter(145));
        gradient.setColorAt(1, color.darker(165));
        painter.save();
        painter.translate(tile.topLeft());
        painter.scale(tile.width(), tile.height());
        painter.fillRect(QRectF(0, 0, 1, 1), gradient);
        painter.restore();
        painter.setPen(palette().base().color());
        painter.drawRect(tile.adjusted(0, 0, -1, -1));
        if (tile.width() > 35 && tile.height() > fontMetrics().height() + 6) {
            painter.save();
            painter.setClipRect(tile);
            painter.setPen(palette().highlightedText().color());
            const QString label = fontMetrics().elidedText(m_items[i].name, Qt::ElideRight, qMax(0, int(tile.width()) - 8));
            painter.drawText(tile.adjusted(4, 3, -4, -3), Qt::AlignLeft | Qt::AlignTop, label);
            painter.restore();
        }
    }
    if (std::none_of(rectangles.cbegin(), rectangles.cend(), [](const QRectF &tile) {
            return !tile.isEmpty();
        })) {
        painter.setPen(palette().text().color());
        painter.drawText(rect(), Qt::AlignCenter, i18nc("@info", "No measured non-empty items to display."));
    }
}

int CushionTreemap::itemAt(const QPointF &pos) const
{
    const auto rectangles = tiles();
    for (qsizetype i = 0; i < rectangles.size(); ++i) {
        if (!rectangles[i].isEmpty() && rectangles[i].contains(pos)) {
            return int(i);
        }
    }
    return -1;
}

void CushionTreemap::mousePressEvent(QMouseEvent *event)
{
    const int index = itemAt(event->position());
    if (event->button() == Qt::LeftButton && index >= 0) {
        Q_EMIT activated(index);
    }
    QWidget::mousePressEvent(event);
}

bool CushionTreemap::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        const auto *help = static_cast<QHelpEvent *>(event);
        const int index = itemAt(help->pos());
        if (index >= 0) {
            const QString text = i18nc("@info:tooltip name and size", "%1: %2", m_items[index].name, KIO::convertSize(m_items[index].size));
            QToolTip::showText(help->globalPos(), QStringLiteral("<qt>%1</qt>").arg(text.toHtmlEscaped()), this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}
