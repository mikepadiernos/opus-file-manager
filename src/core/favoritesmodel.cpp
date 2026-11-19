// SPDX-License-Identifier: GPL-3.0-or-later

#include "favoritesmodel.h"

#include <KIO/Global>

#include <QFileInfo>
#include <QIcon>
#include <QMimeData>

namespace
{
constexpr auto UrlsKey = "Urls";
constexpr auto TitlesKey = "Titles";
}

FavoritesModel::FavoritesModel(const KConfigGroup &group, QObject *parent)
    : QAbstractListModel(parent)
    , m_group(group)
{
    const QList<QUrl> urls = m_group.readEntry(UrlsKey, QList<QUrl>());
    const QStringList titles = m_group.readEntry(TitlesKey, QStringList());
    for (int i = 0; i < urls.size(); ++i) {
        m_favorites.append({urls.at(i), titles.value(i)});
    }
}

QList<QUrl> FavoritesModel::urls() const
{
    QList<QUrl> urls;
    urls.reserve(m_favorites.size());
    for (const Favorite &favorite : m_favorites) {
        urls.append(favorite.url);
    }
    return urls;
}

bool FavoritesModel::contains(const QUrl &url) const
{
    return rowOf(url) >= 0;
}

bool FavoritesModel::add(const QUrl &url, int row)
{
    if (!url.isValid() || contains(url)) {
        return false;
    }
    const int at = row < 0 || row > m_favorites.size() ? m_favorites.size() : row;
    beginInsertRows(QModelIndex(), at, at);
    m_favorites.insert(at, {url, QString()});
    endInsertRows();
    save();
    return true;
}

bool FavoritesModel::remove(const QUrl &url)
{
    const int row = rowOf(url);
    return row >= 0 && removeRows(row, 1);
}

QString FavoritesModel::defaultTitle(const QUrl &url)
{
    const QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
    return name.isEmpty() ? url.toDisplayString(QUrl::PreferLocalFile) : name;
}

int FavoritesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_favorites.size();
}

QVariant FavoritesModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Favorite &favorite = m_favorites.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return favorite.customTitle.isEmpty() ? defaultTitle(favorite.url) : favorite.customTitle;
    case Qt::DecorationRole:
        return QIcon::fromTheme(KIO::iconNameForUrl(favorite.url));
    case Qt::ToolTipRole:
        return favorite.url.toDisplayString(QUrl::PreferLocalFile);
    case UrlRole:
        return favorite.url;
    case CustomTitleRole:
        return favorite.customTitle;
    }
    return {};
}

bool FavoritesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }
    m_favorites[index.row()].customTitle = value.toString().trimmed();
    Q_EMIT dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, CustomTitleRole});
    save();
    return true;
}

Qt::ItemFlags FavoritesModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;
    }
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}

bool FavoritesModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row < 0 || count < 1 || row + count > m_favorites.size()) {
        return false;
    }
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    m_favorites.remove(row, count);
    endRemoveRows();
    save();
    return true;
}

bool FavoritesModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild)
{
    if (sourceParent.isValid() || destinationParent.isValid() || sourceRow < 0 || count < 1 || sourceRow + count > m_favorites.size() || destinationChild < 0
        || destinationChild > m_favorites.size()) {
        return false;
    }
    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow + count - 1, QModelIndex(), destinationChild)) {
        return false;
    }
    const QList<Favorite> moved = m_favorites.mid(sourceRow, count);
    m_favorites.remove(sourceRow, count);
    const int insertAt = destinationChild > sourceRow ? destinationChild - count : destinationChild;
    for (int i = 0; i < count; ++i) {
        m_favorites.insert(insertAt + i, moved.at(i));
    }
    endMoveRows();
    save();
    return true;
}

QStringList FavoritesModel::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

QMimeData *FavoritesModel::mimeData(const QModelIndexList &indexes) const
{
    QList<QUrl> urls;
    for (const QModelIndex &index : indexes) {
        urls.append(m_favorites.at(index.row()).url);
    }
    auto *mime = new QMimeData;
    mime->setUrls(urls);
    return mime;
}

Qt::DropActions FavoritesModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::DropActions FavoritesModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::LinkAction;
}

bool FavoritesModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(column)
    if (action == Qt::IgnoreAction || !data->hasUrls()) {
        return false;
    }
    int at = row < 0 || parent.isValid() ? m_favorites.size() : row;
    const QList<QUrl> urls = data->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile()) {
            continue;
        }
        const int existing = rowOf(url);
        if (existing < 0) {
            add(url, at);
            ++at;
        } else {
            if (existing != at && existing + 1 != at) {
                moveRows(QModelIndex(), existing, 1, QModelIndex(), at);
            }
            at = rowOf(url) + 1;
        }
    }
    return true;
}

int FavoritesModel::rowOf(const QUrl &url) const
{
    const QUrl wanted = url.adjusted(QUrl::StripTrailingSlash);
    for (int row = 0; row < m_favorites.size(); ++row) {
        if (m_favorites.at(row).url.adjusted(QUrl::StripTrailingSlash) == wanted) {
            return row;
        }
    }
    return -1;
}

void FavoritesModel::save()
{
    QStringList titles;
    for (const Favorite &favorite : std::as_const(m_favorites)) {
        titles.append(favorite.customTitle);
    }
    m_group.writeEntry(UrlsKey, urls());
    m_group.writeEntry(TitlesKey, titles);
    m_group.sync();
}
