// SPDX-License-Identifier: GPL-3.0-or-later

#include "yazipreview.h"

#include "folderview.h"
#include "informationpanel.h"
#include "previewpanel.h"

#include <KLocalizedString>

#include <QAbstractItemView>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyleOptionFocusRect>
#include <QTimer>
#include <QVBoxLayout>

YaziPreview::YaziPreview(QWidget *parent)
    : QFrame(parent)
    , m_busy(new QProgressBar(this))
    , m_pages(new QStackedWidget(this))
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

    m_busy->setObjectName(QStringLiteral("yaziPreviewBusy"));
    m_busy->setRange(0, 0);
    m_busy->setTextVisible(false);
    m_busy->setMaximumHeight(qMax(4, fontMetrics().height() / 4));
    m_busy->setAccessibleName(i18nc("@info:accessibility", "Loading preview…"));
    m_busy->setToolTip(i18nc("@info:tooltip", "Loading preview…"));
    // Keep the room so the preview doesn't jump when loading starts or ends.
    QSizePolicy busyPolicy = m_busy->sizePolicy();
    busyPolicy.setRetainSizeWhenHidden(true);
    m_busy->setSizePolicy(busyPolicy);
    m_busy->hide();
    connect(m_panel, &PreviewPanel::busyChanged, this, &YaziPreview::updateBusyIndicator);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("yaziInformation"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFocusPolicy(Qt::NoFocus);
    scroll->setAccessibleName(i18nc("@info:accessibility", "File information"));
    auto *contents = new QWidget(scroll);
    auto *contentsLayout = new QVBoxLayout(contents);
    m_informationTitle = new QLabel(contents);
    m_informationTitle->setObjectName(QStringLiteral("yaziInformationTitle"));
    m_informationTitle->setTextFormat(Qt::PlainText);
    m_informationTitle->setWordWrap(true);
    QFont titleFont = m_informationTitle->font();
    titleFont.setBold(true);
    m_informationTitle->setFont(titleFont);
    m_information = new InformationPanel(contents);
    m_information->setFields(informationFields());
    m_information->setEmptyFieldsHidden(true);
    if (auto *form = qobject_cast<QFormLayout *>(m_information->layout())) {
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setContentsMargins(0, 0, 0, 0);
    }
    contentsLayout->addWidget(m_informationTitle);
    contentsLayout->addWidget(m_information);
    contentsLayout->addStretch(1);
    scroll->setWidget(contents);
    m_informationPage = scroll;

    m_pages->addWidget(m_panel);
    m_pages->addWidget(m_informationPage);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);
    layout->addWidget(m_busy);
    layout->addWidget(m_pages, 1);
}

YaziPreview::~YaziPreview() = default;

QStringList YaziPreview::informationFields()
{
    return {QStringLiteral("friendlyMimeType"),
            QStringLiteral("size"),
            QStringLiteral("modificationTime"),
            QStringLiteral("permissions"),
            QStringLiteral("ownerAndGroup"),
            QStringLiteral("linkDest")};
}

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

InformationPanel *YaziPreview::information() const
{
    return m_information;
}

void YaziPreview::setContent(YaziMode::PreviewContent content)
{
    if (content == m_content) {
        return;
    }
    m_content = content;
    m_pages->setCurrentWidget(content == YaziMode::PreviewContent::Information ? m_informationPage : m_panel);
    reset();
    updateNow();
}

YaziMode::PreviewContent YaziPreview::content() const
{
    return m_content;
}

void YaziPreview::setShowHiddenFiles(bool show)
{
    m_panel->setShowHiddenFiles(show);
}

bool YaziPreview::isUpdatePending() const
{
    return m_timer->isActive();
}

QProgressBar *YaziPreview::busyIndicator() const
{
    return m_busy;
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
    m_information->setItem({});
    m_informationTitle->clear();
    setAccessibleDescription(i18nc("@info:accessibility", "Nothing to preview"));
}

void YaziPreview::updateBusyIndicator()
{
    m_busy->setVisible(m_content == YaziMode::PreviewContent::Preview && m_panel->isBusy());
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
    if (m_content == YaziMode::PreviewContent::Information) {
        m_information->setItem(item);
        m_informationTitle->setText(item.isNull() ? i18nc("@info", "Nothing to show.") : item.text());
        setAccessibleDescription(item.isNull() ? i18nc("@info:accessibility", "Nothing to preview")
                                               : i18nc("@info:accessibility", "Information about %1", item.text()));
    } else if (item.isNull()) {
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
