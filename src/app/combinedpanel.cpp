// SPDX-License-Identifier: GPL-3.0-or-later

#include "combinedpanel.h"

#include "contentheight.h"
#include "favoritesmodel.h"
#include "foldertree.h"

#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KLocalizedString>

#include <QDir>
#include <QGridLayout>
#include <QListView>
#include <QMenu>
#include <QMetaEnum>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace
{
class SectionHeader : public QToolButton
{
public:
    using QToolButton::QToolButton;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter painter(this);
        QStyleOptionToolButton option;
        initStyleOption(&option);
        option.text.clear();
        option.arrowType = Qt::NoArrow;
        option.features &= ~QStyleOptionToolButton::Arrow;
        option.state &= ~QStyle::State_On;
        painter.drawComplexControl(QStyle::CC_ToolButton, option);

        const int margin = 6;
        const int arrowSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
        const QRect arrowRect(width() - margin - arrowSize, (height() - arrowSize) / 2, arrowSize, arrowSize);
        const QRect textRect(margin, 0, qMax(0, arrowRect.left() - 2 * margin), height());
        painter.drawItemText(textRect,
                             Qt::AlignLeft | Qt::AlignVCenter,
                             palette(),
                             isEnabled(),
                             fontMetrics().elidedText(text(), Qt::ElideRight, textRect.width()),
                             QPalette::ButtonText);
        QStyleOption arrow;
        arrow.initFrom(this);
        arrow.rect = arrowRect;
        painter.drawPrimitive(arrowType() == Qt::DownArrow ? QStyle::PE_IndicatorArrowDown : QStyle::PE_IndicatorArrowRight, arrow);
    }
};

void setSectionExpanded(QToolButton *header, QWidget *content, bool expanded)
{
    header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    header->setChecked(expanded);
    content->setVisible(expanded);
}

/// A flat section title, dimmed like the Places section titles, that shows or hides @p content.
QToolButton *sectionHeader(const QString &title, QWidget *content)
{
    auto *header = new SectionHeader;
    header->setText(title);
    header->setAutoRaise(true);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setCheckable(true);
    header->setChecked(true);
    header->setArrowType(Qt::DownArrow);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPalette palette = header->palette();
    palette.setColor(QPalette::ButtonText, palette.color(QPalette::Disabled, QPalette::WindowText));
    header->setPalette(palette);
    QObject::connect(header, &QToolButton::clicked, content, [header, content] {
        setSectionExpanded(header, content, content->isHidden());
    });
    return header;
}
}

class PlacesSection : public QWidget
{
public:
    PlacesSection(KFilePlacesModel *model, KFilePlacesModel::GroupType type, QWidget *parent)
        : QWidget(parent)
        , m_model(model)
        , m_type(type)
        , m_view(new KFilePlacesView(this))
        , m_header(sectionHeader(QString(), m_view))
    {
        m_view->setModel(model);
        m_view->setDropOnPlaceEnabled(true);
        m_view->setAutoResizeItemsEnabled(false);
        m_view->setFrameShape(QFrame::NoFrame);
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(m_view, 0, 0);
        m_header->setObjectName(key());
        m_header->setAutoFillBackground(true);
        m_header->setBackgroundRole(QPalette::Window);
        layout->addWidget(m_header, 0, 0, Qt::AlignTop);

        const auto schedule = [this] {
            if (!m_refreshPending) {
                m_refreshPending = true;
                QTimer::singleShot(0, this, [this] {
                    m_refreshPending = false;
                    refresh();
                });
            }
        };
        connect(model, &QAbstractItemModel::rowsInserted, this, schedule);
        connect(model, &QAbstractItemModel::rowsRemoved, this, schedule);
        connect(model, &QAbstractItemModel::rowsMoved, this, schedule);
        connect(model, &QAbstractItemModel::modelReset, this, schedule);
        connect(model, &QAbstractItemModel::layoutChanged, this, schedule);
        connect(model, &QAbstractItemModel::dataChanged, this, schedule);
        connect(m_view, &KFilePlacesView::allPlacesShownChanged, this, schedule);
        connect(m_view->verticalScrollBar(), &QScrollBar::rangeChanged, this, schedule);
        refresh();
    }

    KFilePlacesView *view() const
    {
        return m_view;
    }

    QString key() const
    {
        return QStringLiteral("places:") + QString::fromLatin1(QMetaEnum::fromType<KFilePlacesModel::GroupType>().valueToKey(m_type));
    }

    bool collapsed() const
    {
        return m_header->arrowType() == Qt::RightArrow;
    }

    void setCollapsed(bool collapsed)
    {
        setSectionExpanded(m_header, m_view, !collapsed);
        m_view->setVisible(m_enabled && !collapsed);
    }

    void setPanelVisible(bool visible)
    {
        m_enabled = visible;
        m_view->setVisible(visible && !collapsed());
    }

    void refresh()
    {
        int height = m_view->spacing();
        QString title;
        bool hasEntries = false;
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const QModelIndex index = m_model->index(row, 0);
            const bool show = m_model->groupType(index) == m_type && (m_view->allPlacesShown() || !m_model->isHidden(index));
            if (m_view->isRowHidden(row) == show) {
                m_view->setRowHidden(row, !show);
            }
            if (show) {
                if (!hasEntries) {
                    title = index.data(KFilePlacesModel::GroupRole).toString();
                }
                hasEntries = true;
                height += m_view->sizeHintForIndex(index).height() + m_view->spacing();
            }
        }
        m_header->setText(title.isEmpty() ? i18nc("@title:group places section", "Other") : title);
        // KFilePlacesView paints its own group header above the first row.
        // Cover that header while leaving its entries and native delegate intact.
        m_header->setFixedHeight(m_view->fontMetrics().height() + 3 * (4 + m_view->spacing()));
        m_view->setFixedHeight(height);
        setVisible(hasEntries);
    }

private:
    KFilePlacesModel *m_model;
    KFilePlacesModel::GroupType m_type;
    KFilePlacesView *m_view;
    QToolButton *m_header;
    bool m_enabled = true;
    bool m_refreshPending = false;
};

CombinedPanel::CombinedPanel(KFilePlacesModel *places, FavoritesModel *favorites, QWidget *parent)
    : QWidget(parent)
    , m_favorites(favorites)
{
    auto *content = new QWidget;
    auto *column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    m_favoritesView = new QListView(content);
    m_favoritesView->setModel(favorites);
    m_favoritesView->setDragDropMode(QAbstractItemView::DragDrop);
    m_favoritesView->setDefaultDropAction(Qt::CopyAction);
    m_favoritesView->setDropIndicatorShown(true);
    m_favoritesView->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_favoritesView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_favoritesView->viewport()->installEventFilter(this);
    fitHeightToContents(m_favoritesView);
    connect(m_favoritesView, &QListView::clicked, this, [this](const QModelIndex &index) {
        Q_EMIT urlActivated(index.data(FavoritesModel::UrlRole).toUrl());
    });
    connect(m_favoritesView, &QListView::customContextMenuRequested, this, &CombinedPanel::showFavoriteMenu);
    m_favoritesHeader = sectionHeader(i18nc("@title:group", "Favorites"), m_favoritesView);
    m_favoritesHeader->setObjectName(QStringLiteral("favorites"));
    column->addWidget(m_favoritesHeader);
    column->addWidget(m_favoritesView);

    m_placesContainer = new QWidget(content);
    auto *placesLayout = new QVBoxLayout(m_placesContainer);
    placesLayout->setContentsMargins(0, 0, 0, 0);
    placesLayout->setSpacing(0);
    for (const auto type : {KFilePlacesModel::PlacesType,
                            KFilePlacesModel::RemoteType,
                            KFilePlacesModel::RecentlySavedType,
                            KFilePlacesModel::SearchForType,
                            KFilePlacesModel::DevicesType,
                            KFilePlacesModel::RemovableDevicesType,
                            KFilePlacesModel::TagsType,
                            KFilePlacesModel::UnknownType}) {
        auto *section = new PlacesSection(places, type, m_placesContainer);
        m_placesSections.append(section);
        placesLayout->addWidget(section);
        KFilePlacesView *view = section->view();
        connect(view, &KFilePlacesView::urlChanged, this, &CombinedPanel::urlActivated);
        connect(view, &KFilePlacesView::tabRequested, this, &CombinedPanel::tabRequested);
        connect(view, &KFilePlacesView::urlsDropped, this, [this](const QUrl &destination, QDropEvent *event, QWidget *) {
            Q_EMIT urlsDropped(destination, event);
        });
        connect(view, &KFilePlacesView::allPlacesShownChanged, this, [this, view](bool show) {
            for (PlacesSection *other : std::as_const(m_placesSections)) {
                if (other->view() != view && other->view()->allPlacesShown() != show) {
                    other->view()->setShowAll(show);
                }
            }
        });
    }
    column->addWidget(m_placesContainer);

    m_folderTree = new FolderTree({QUrl::fromLocalFile(QDir::homePath()), QUrl::fromLocalFile(QStringLiteral("/"))}, content);
    connect(m_folderTree, &FolderTree::urlActivated, this, &CombinedPanel::urlActivated);
    connect(m_folderTree, &FolderTree::tabRequested, this, &CombinedPanel::tabRequested);
    connect(m_folderTree, &FolderTree::contextMenuRequested, this, &CombinedPanel::folderContextMenuRequested);
    connect(m_folderTree, &FolderTree::urlsDropped, this, &CombinedPanel::urlsDropped);
    connect(m_folderTree, &FolderTree::revealed, this, [this] {
        m_followRevealed = true;
        scrollToRevealed();
    });
    // Match the Places view, which draws on the window colour.
    const auto trees = m_folderTree->findChildren<QTreeView *>();
    for (QAbstractItemView *view : trees) {
        view->viewport()->setBackgroundRole(QPalette::Window);
    }
    m_favoritesView->viewport()->setBackgroundRole(QPalette::Window);
    m_treeHeader = sectionHeader(i18nc("@title:group", "Tree"), m_folderTree);
    m_treeHeader->setObjectName(QStringLiteral("tree"));
    column->addWidget(m_treeHeader);
    column->addWidget(m_folderTree);
    column->addStretch();

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setWidget(content);
    connect(m_scroll->verticalScrollBar(), &QScrollBar::rangeChanged, this, &CombinedPanel::scrollToRevealed);
    connect(m_scroll->verticalScrollBar(), &QScrollBar::actionTriggered, this, [this] {
        m_followRevealed = false;
    });
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scroll);
}

FolderTree *CombinedPanel::folderTree() const
{
    return m_folderTree;
}

QStringList CombinedPanel::collapsedSections() const
{
    QStringList sections;
    if (m_favoritesHeader->arrowType() == Qt::RightArrow) {
        sections.append(QStringLiteral("favorites"));
    }
    if (m_treeHeader->arrowType() == Qt::RightArrow) {
        sections.append(QStringLiteral("tree"));
    }
    for (const PlacesSection *section : m_placesSections) {
        if (section->collapsed()) {
            sections.append(section->key());
        }
    }
    return sections;
}

void CombinedPanel::setCollapsedSections(const QStringList &sections)
{
    const bool favoritesExpanded = !sections.contains(QStringLiteral("favorites"));
    const bool treeExpanded = !sections.contains(QStringLiteral("tree"));
    setSectionExpanded(m_favoritesHeader, m_favoritesView, favoritesExpanded);
    setSectionExpanded(m_treeHeader, m_folderTree, treeExpanded);
    for (PlacesSection *section : std::as_const(m_placesSections)) {
        section->setCollapsed(sections.contains(section->key()));
    }
    m_favoritesView->setVisible(m_favoritesVisible && favoritesExpanded);
    m_folderTree->setVisible(m_foldersVisible && treeExpanded);
}

void CombinedPanel::setSectionVisibility(bool favorites, bool places, bool folders)
{
    const QStringList collapsed = collapsedSections();
    m_favoritesVisible = favorites;
    m_foldersVisible = folders;
    m_favoritesHeader->setVisible(favorites);
    m_placesContainer->setVisible(places);
    for (PlacesSection *section : std::as_const(m_placesSections)) {
        section->setPanelVisible(places);
    }
    m_treeHeader->setVisible(folders);
    setCollapsedSections(collapsed);
}

void CombinedPanel::scrollToRevealed()
{
    const QRect revealed = m_folderTree->revealedArea();
    if (!m_followRevealed || revealed.isNull()) {
        return;
    }
    const QPoint center = m_folderTree->mapTo(m_scroll->widget(), revealed.center());
    m_scroll->ensureVisible(center.x(), center.y(), 0, revealed.height());
}

void CombinedPanel::showFavoriteMenu(const QPoint &pos)
{
    const QModelIndex index = m_favoritesView->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    const QUrl url = index.data(FavoritesModel::UrlRole).toUrl();
    QMenu menu(this);
    QAction *openInTab = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open in New Tab"));
    QAction *rename = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), i18nc("@action:inmenu", "Rename…"));
    menu.addSeparator();
    QAction *remove = menu.addAction(QIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:inmenu", "Remove from Favorites"));

    QAction *chosen = menu.exec(m_favoritesView->viewport()->mapToGlobal(pos));
    if (chosen == openInTab) {
        Q_EMIT tabRequested(url);
    } else if (chosen == rename) {
        m_favoritesView->edit(index);
    } else if (chosen == remove) {
        m_favorites->remove(url);
    }
}

bool CombinedPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_favoritesView->viewport() && event->type() == QEvent::MouseButtonRelease) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const QModelIndex index = m_favoritesView->indexAt(mouse->position().toPoint());
        if (mouse->button() == Qt::MiddleButton && index.isValid()) {
            Q_EMIT tabRequested(index.data(FavoritesModel::UrlRole).toUrl());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
