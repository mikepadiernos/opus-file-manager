// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>

#include <QAbstractListModel>
#include <QList>
#include <QUrl>

/**
 * The user's favorite folders, in their chosen order, each with an
 * optional custom title. Every change is written to the KConfig group
 * given to the constructor.
 */
class FavoritesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UrlRole = Qt::UserRole + 1,
        /// The custom title, or an empty string when the default is used.
        CustomTitleRole,
    };

    /**
     * Loads the favorites stored in @p group.
     * @param group where favorites are read from and written to
     * @param parent the QObject parent
     */
    explicit FavoritesModel(const KConfigGroup &group, QObject *parent = nullptr);

    /// The favorite folders in display order.
    QList<QUrl> urls() const;

    /// Whether @p url is a favorite.
    bool contains(const QUrl &url) const;

    /**
     * Adds a favorite.
     * @param url the folder to add
     * @param row where to insert it; -1 appends
     * @returns false when @p url is invalid or already a favorite
     */
    bool add(const QUrl &url, int row = -1);

    /**
     * Removes a favorite.
     * @param url the folder to remove
     * @returns false when @p url is not a favorite
     */
    bool remove(const QUrl &url);

    /**
     * The title shown when a favorite has no custom title.
     * @param url the favorite folder
     * @returns the folder name, or the full path for the root folder
     */
    static QString defaultTitle(const QUrl &url);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    /// DisplayRole and EditRole give the shown title, DecorationRole the folder's icon.
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    /// Renames a favorite through EditRole; an empty title restores the default.
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild) override;

    QStringList mimeTypes() const override;
    /// Encodes the favorites at @p indexes as a URI list.
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    /**
     * Drops URLs before @p row: a URL that is already a favorite moves
     * there, any other is added there. Local files are skipped.
     */
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

private:
    struct Favorite {
        QUrl url;
        QString customTitle;
    };

    int rowOf(const QUrl &url) const;
    void save();

    KConfigGroup m_group;
    QList<Favorite> m_favorites;
};
