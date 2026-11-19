// SPDX-License-Identifier: GPL-3.0-or-later

#include "combinedpanel.h"

#include "contentheight.h"
#include "favoritesmodel.h"
#include "foldertree.h"

#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KLocalizedString>

#include <QDir>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace
{
void setSectionExpanded(QToolButton *header, QWidget *content, bool expanded)
{
    header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    content->setVisible(expanded);
}

/// A flat section title, dimmed like the Places section titles, that shows or hides @p content.
QToolButton *sectionHeader(const QString &title, QWidget *content)
{
    auto *header = new QToolButton;
    header->setText(title);
    header->setAutoRaise(true);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
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
    column->addWidget(m_favoritesHeader);
    column->addWidget(m_favoritesView);

    auto *placesView = new KFilePlacesView(content);
    m_placesView = placesView;
    placesView->setModel(places);
    placesView->setDropOnPlaceEnabled(true);
    placesView->setAutoResizeItemsEnabled(false);
    fitHeightToContents(placesView);
    connect(placesView, &KFilePlacesView::urlChanged, this, &CombinedPanel::urlActivated);
    connect(placesView, &KFilePlacesView::tabRequested, this, &CombinedPanel::tabRequested);
    connect(placesView, &KFilePlacesView::urlsDropped, this, [this](const QUrl &destination, QDropEvent *event, QWidget *) {
        Q_EMIT urlsDropped(destination, event);
    });
    column->addWidget(placesView);

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
    if (m_favoritesView->isHidden()) {
        sections.append(QStringLiteral("favorites"));
    }
    if (m_folderTree->isHidden()) {
        sections.append(QStringLiteral("tree"));
    }
    return sections;
}

void CombinedPanel::setCollapsedSections(const QStringList &sections)
{
    setSectionExpanded(m_favoritesHeader, m_favoritesView, m_favoritesVisible && !sections.contains(QStringLiteral("favorites")));
    setSectionExpanded(m_treeHeader, m_folderTree, m_foldersVisible && !sections.contains(QStringLiteral("tree")));
}

void CombinedPanel::setSectionVisibility(bool favorites, bool places, bool folders)
{
    const QStringList collapsed = collapsedSections();
    m_favoritesVisible = favorites;
    m_placesVisible = places;
    m_foldersVisible = folders;
    m_favoritesHeader->setVisible(favorites);
    m_favoritesView->setVisible(favorites);
    m_placesView->setVisible(places);
    m_treeHeader->setVisible(folders);
    m_folderTree->setVisible(folders);
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
