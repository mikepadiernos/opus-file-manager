// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QUrl>
#include <QWidget>

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

Q_SIGNALS:
    /// A folder was clicked.
    void urlActivated(const QUrl &url);
    /// A folder was middle-clicked.
    void tabRequested(const QUrl &url);

protected:
    /// Turns a middle-click on a folder into tabRequested().
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QList<QTreeView *> m_trees;
    QList<QUrl> m_roots;
};
