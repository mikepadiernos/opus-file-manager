// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazipreview.h"

#include "folderview.h"
#include "previewpanel.h"

#include <KLocalizedString>

#include <QAbstractItemView>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionFocusRect>
#include <QTimer>
#include <QVBoxLayout>

YaziPreview::YaziPreview(QWidget *parent)
    : QFrame(parent)
    , m_panel(new PreviewPanel(this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("yaziPreview"));
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(i18nc("@info:accessibility", "Preview"));
    setAccessibleDescription(i18nc("@info:accessibility", "Nothing to preview"));
    m_panel->setCompact(true);
    m_timer->setSingleShot(true);
    m_timer->setInterval(UpdateDelay);
    connect(m_timer, &QTimer::timeout, this, &YaziPreview::updateNow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(m_panel);
}

YaziPreview::~YaziPreview() = default;

void YaziPreview::setView(FolderView *view)
{
    if (view == m_view) {
        return;
    }
    if (m_view) {
        m_view->disconnect(this);
    }
    m_view = view;
    reset();
    if (!m_view) {
        return;
    }
    connect(m_view, &FolderView::selectionChanged, this, &YaziPreview::scheduleUpdate);
    connect(m_view, &FolderView::urlChanged, this, &YaziPreview::scheduleUpdate);
    connect(m_view, &FolderView::listingReady, this, &YaziPreview::scheduleUpdate);
    connect(m_view, &FolderView::itemCountChanged, this, &YaziPreview::scheduleUpdate);
    updateNow();
}

FolderView *YaziPreview::view() const
{
    return m_view;
}

KFileItem YaziPreview::item() const
{
    return m_item;
}

PreviewPanel *YaziPreview::panel() const
{
    return m_panel;
}

bool YaziPreview::isUpdatePending() const
{
    return m_timer->isActive();
}

void YaziPreview::scheduleUpdate()
{
    m_timer->start();
}

void YaziPreview::reset()
{
    m_timer->stop();
    m_item = {};
    m_shown = false;
    m_panel->clear();
    setAccessibleDescription(i18nc("@info:accessibility", "Nothing to preview"));
}

KFileItem YaziPreview::focusedItem() const
{
    if (!m_view || !m_view->currentView()->currentIndex().isValid()) {
        return {};
    }
    return m_view->currentItem();
}

void YaziPreview::updateNow()
{
    m_timer->stop();
    if (!isVisible()) {
        return;
    }
    const KFileItem item = focusedItem();
    if (m_shown && item.url() == m_item.url()) {
        return;
    }
    m_item = item;
    m_shown = true;
    if (item.isNull()) {
        m_panel->clear();
        setAccessibleDescription(i18nc("@info:accessibility", "Nothing to preview"));
    } else {
        if (item.isDir()) {
            m_panel->showFolder(item);
        } else {
            m_panel->setItems({item});
        }
        setAccessibleDescription(i18nc("@info:accessibility", "Preview of %1", item.text()));
    }
    Q_EMIT itemChanged(item);
}

void YaziPreview::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    updateNow();
}

void YaziPreview::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
    // Hidden by the pane rather than by minimising the window: stop media
    // and pending work, and catch up when shown again.
    if (!event->spontaneous()) {
        reset();
    }
}

void YaziPreview::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier && m_view) {
        m_view->currentView()->setFocus(Qt::OtherFocusReason);
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void YaziPreview::focusInEvent(QFocusEvent *event)
{
    QFrame::focusInEvent(event);
    update();
}

void YaziPreview::focusOutEvent(QFocusEvent *event)
{
    QFrame::focusOutEvent(event);
    update();
}

void YaziPreview::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    if (!hasFocus()) {
        return;
    }
    QPainter painter(this);
    QStyleOptionFocusRect option;
    option.initFrom(this);
    option.backgroundColor = palette().color(QPalette::Window);
    style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
}
