// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QUrl>
#include <QWidget>

class QDropEvent;
class QTreeView;

/**
 * Folder trees, one per root folder, stacked vertically. Each root is an
 * expandable item and folders load as they are expanded. The trees grow
 * with their contents; put the widget in a scroll area.
 */
class FolderTree : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param roots the folders shown as top-level items, in order
     * @param parent the parent widget
     */
    explicit FolderTree(const QList<QUrl> &roots, QWidget *parent = nullptr);

    QList<QUrl> roots() const;

    /// Folders currently expanded, in all trees.
    QList<QUrl> expandedUrls() const;

    /**
     * Expands each folder once its tree has listed it. Folders outside
     * every root are ignored.
     * @param urls the folders to expand
     */
    void setExpandedUrls(const QList<QUrl> &urls);

    /**
     * Expands the folders leading to @p url and selects it, in the tree of
     * the innermost root that holds it, then emits revealed(). Does nothing
     * for a URL outside every root.
     * @param url the folder to show
     */
    void reveal(const QUrl &url);

    /// The row of the last revealed folder, in this widget's coordinates; null when none is selected.
    QRect revealedArea() const;

    bool showHiddenFolders() const;
    /// @param show true to list folders whose names start with a dot
    void setShowHiddenFolders(bool show);

Q_SIGNALS:
    /// A folder was clicked.
    void urlActivated(const QUrl &url);
    /// A folder was middle-clicked.
    void tabRequested(const QUrl &url);
    /// A revealed folder is selected; revealedArea() gives its row.
    void revealed();
    /// A folder was right-clicked.
    void contextMenuRequested(const QUrl &url, const QPoint &globalPos);
    /// Files were dropped on a folder.
    void urlsDropped(const QUrl &destination, QDropEvent *event);

protected:
    /// Turns middle-clicks into tabRequested() and drops on folders into urlsDropped().
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QList<QTreeView *> m_trees;
    /// Selects the pending reveal in its tree once that tree has listed it.
    void tryReveal();
    /// Returns the tree whose viewport is @p viewport, or nullptr.
    QTreeView *treeForViewport(const QObject *viewport) const;

    QList<QUrl> m_roots;
    QUrl m_pendingReveal;
    int m_pendingTree = -1;
    QTreeView *m_revealedTree = nullptr;
    bool m_showHiddenFolders = false;
};
