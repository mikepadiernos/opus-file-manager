// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"

#include "detailsview.h"
#include "filecolorrules.h"
#include "flatview.h"
#include "foldermetadata.h"
#include "ignorelist.h"
#include "labelcolor.h"
#include "symbolicicon.h"
#include "viewsettings.h"

#include <KCategorizedSortFilterProxyModel>
#include <KCategorizedView>
#include <KCategoryDrawer>
#include <KConfigGroup>
#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KFileItemDelegate>
#include <KFileItemListProperties>
#include <KFilePreviewGenerator>
#include <KIO/DropJob>
#include <KIO/Job>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QActionGroup>
#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
const QList<QPair<int, QString>> OptionalColumns{
    {KDirModel::Type, QStringLiteral("type")},
    {KDirModel::Size, QStringLiteral("size")},
    {KDirModel::ModifiedTime, QStringLiteral("date")},
    {KDirModel::Permissions, QStringLiteral("mode")},
    {KDirModel::Owner, QStringLiteral("owner")},
    {KDirModel::Group, QStringLiteral("group")},
};

int columnForSortKey(const QString &key)
{
    if (key == QLatin1String("size")) {
        return KDirModel::Size;
    }
    if (key == QLatin1String("date")) {
        return KDirModel::ModifiedTime;
    }
    if (key == QLatin1String("type")) {
        return KDirModel::Type;
    }
    if (key == QLatin1String("mode")) {
        return KDirModel::Permissions;
    }
    if (key == QLatin1String("owner")) {
        return KDirModel::Owner;
    }
    if (key == QLatin1String("group")) {
        return KDirModel::Group;
    }
    if (key == QLatin1String("rating")) {
        return KDirModel::Group;
    }
    return KDirModel::Name;
}

QString sortKeyForColumn(int column, bool ratingColumn)
{
    if (column == KDirModel::Group && ratingColumn) {
        return QStringLiteral("rating");
    }
    for (const auto &[candidate, key] : OptionalColumns) {
        if (candidate == column) {
            return key;
        }
    }
    return QStringLiteral("name");
}
}

/// Sorts like KDirSortFilterProxyModel and hides ignored items and items a NameFilter rejects.
class NameFilterProxyModel : public KDirSortFilterProxyModel
{
public:
    explicit NameFilterProxyModel(QObject *parent)
        : KDirSortFilterProxyModel(parent)
        , m_ignoreList(IgnoreList::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("Files"))))
    {
    }

    NameFilter nameFilter() const
    {
        return m_filter;
    }

    void setNameFilter(const NameFilter &filter)
    {
        beginFilterChange();
        m_filter = filter;
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    }

    /// Hides folders and shows relative paths in the Type column; an empty URL ends flat mode.
    void setFlatBase(const QUrl &base)
    {
        beginFilterChange();
        m_flatBase = base;
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
        Q_EMIT headerDataChanged(Qt::Horizontal, KDirModel::Type, KDirModel::Type);
    }

    void setMetadata(const QJsonObject &metadata, const QString &directory)
    {
        beginFilterChange();
        m_metadata = metadata;
        m_metadataDirectory = directory.isEmpty() ? QString() : QFileInfo(directory).absoluteFilePath();
        m_metadataCache.clear();
        FolderMetadata::Format format;
        const bool hasFormat = FolderMetadata::format(metadata, &format);
        m_groupBy = hasFormat ? format.group : m_settings.groupBy;
        m_ratingColumn = hasFormat ? format.columns.contains(QStringLiteral("rating")) : m_settings.visibleColumns.contains(QStringLiteral("rating"));
        setCategorizedModel(m_groupBy != QLatin1String("none"));
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
        sort(sortColumn(), sortOrder());
    }

    void setSettings(const ViewSettings &settings, const FileColorRules &fileColors, const IgnoreList &ignoreList)
    {
        beginFilterChange();
        m_settings = settings;
        m_fileColors = fileColors;
        m_ignoreList = ignoreList;
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
        if (rowCount() > 0) {
            Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::DisplayRole, Qt::ForegroundRole});
        }
    }

    QJsonObject metadata() const
    {
        return m_metadata;
    }

    QString groupBy() const
    {
        return m_groupBy;
    }

    bool ratingColumn() const
    {
        return m_ratingColumn;
    }

    void setRatingColumn(bool enabled)
    {
        if (enabled == m_ratingColumn) {
            return;
        }
        m_ratingColumn = enabled;
        Q_EMIT headerDataChanged(Qt::Horizontal, KDirModel::Group, KDirModel::Group);
        if (rowCount() > 0) {
            Q_EMIT dataChanged(index(0, KDirModel::Group), index(rowCount() - 1, KDirModel::Group));
        }
        sort(sortColumn(), sortOrder());
    }

    void setGroupBy(const QString &group)
    {
        if (group == m_groupBy) {
            return;
        }
        m_groupBy = group;
        setCategorizedModel(m_groupBy != QLatin1String("none"));
        sort(sortColumn(), sortOrder());
    }

    QString groupLabel(const QModelIndex &index) const
    {
        if (m_groupBy == QLatin1String("none")) {
            return {};
        }
        const KFileItem item = itemAt(index);
        if (m_groupBy == QLatin1String("name")) {
            const QString first = item.name().left(1).toUpper();
            return first.isEmpty() || !first.at(0).isLetter() ? QStringLiteral("#") : first;
        }
        if (m_groupBy == QLatin1String("type")) {
            return item.isDir() ? i18nc("@item:inlistbox file group", "Folders") : item.mimeComment();
        }
        if (m_groupBy == QLatin1String("ext")) {
            if (item.isDir()) {
                return i18nc("@item:inlistbox file group", "Folders");
            }
            const QString suffix = QFileInfo(item.name()).suffix().toUpper();
            return suffix.isEmpty() ? i18nc("@item:inlistbox file group", "No Extension") : suffix;
        }
        if (m_groupBy == QLatin1String("size")) {
            if (item.isDir()) {
                return i18nc("@item:inlistbox file group", "Folders");
            }
            const qint64 size = item.size();
            if (size < 1024 * 1024) {
                return i18nc("@item:inlistbox file group", "Under 1 MiB");
            }
            if (size < 10LL * 1024 * 1024) {
                return i18nc("@item:inlistbox file group", "1-10 MiB");
            }
            if (size < 100LL * 1024 * 1024) {
                return i18nc("@item:inlistbox file group", "10-100 MiB");
            }
            if (size < 1024LL * 1024 * 1024) {
                return i18nc("@item:inlistbox file group", "100 MiB-1 GiB");
            }
            return i18nc("@item:inlistbox file group", "Over 1 GiB");
        }
        if (m_groupBy == QLatin1String("date")) {
            const QDate modified = item.time(KFileItem::ModificationTime).date();
            const qint64 daysAgo = modified.daysTo(QDate::currentDate());
            if (daysAgo <= 0) {
                return i18nc("@item:inlistbox file group", "Today");
            }
            if (daysAgo == 1) {
                return i18nc("@item:inlistbox file group", "Yesterday");
            }
            if (daysAgo < 7) {
                return i18nc("@item:inlistbox file group", "Last 7 Days");
            }
            if (modified.month() == QDate::currentDate().month() && modified.year() == QDate::currentDate().year()) {
                return i18nc("@item:inlistbox file group", "This Month");
            }
            if (modified.year() == QDate::currentDate().year()) {
                return i18nc("@item:inlistbox file group", "This Year");
            }
            return i18nc("@item:inlistbox file group", "Older");
        }
        return {};
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role == Qt::DisplayRole && index.column() == KDirModel::Size && m_settings.sizeFormat == QLatin1String("bytes")) {
            const KFileItem item = itemAt(index);
            if (!item.isDir()) {
                return QString(QLocale().toString(item.size()) + i18nc("@unit byte in file size", " B"));
            }
        }
        if (role == Qt::DisplayRole && index.column() == KDirModel::ModifiedTime) {
            const QDateTime modified = itemAt(index).time(KFileItem::ModificationTime);
            if (modified.isValid()) {
                if (m_settings.dateFormat == QLatin1String("iso")) {
                    return modified.toString(m_settings.showSeconds ? QStringLiteral("yyyy-MM-dd HH:mm:ss") : QStringLiteral("yyyy-MM-dd HH:mm"));
                }
                if (m_settings.showSeconds) {
                    return QString(QLocale().toString(modified.date(), QLocale::ShortFormat) + u' ' + QLocale().toString(modified.time(), QLocale::LongFormat));
                }
                return QLocale().toString(modified, QLocale::ShortFormat);
            }
        }
        if (role == KCategorizedSortFilterProxyModel::CategorySortRole) {
            return m_groupBy == QLatin1String("none") ? QString() : groupSortKey(index);
        }
        if (role == KCategorizedSortFilterProxyModel::CategoryDisplayRole) {
            return groupLabel(index);
        }
        if (m_ratingColumn && index.column() == KDirModel::Group && role == Qt::DisplayRole) {
            const KFileItem item = itemAt(index);
            const int rating = metadataForItem(item).value(QStringLiteral("ratings")).toObject().value(item.name()).toInt();
            return rating > 0 ? QVariant(rating) : QVariant();
        }
        if (isRelativePath(index.column()) && role == Qt::DisplayRole) {
            return FlatView::relativeFolder(m_flatBase, itemAt(index).url());
        }
        if (role == Qt::ForegroundRole || role == Qt::ToolTipRole) {
            const KFileItem item = itemAt(index);
            const QJsonObject metadata = metadataForItem(item);
            const QString name = item.name();
            const QString color = metadata.value(QStringLiteral("colors")).toObject().value(name).toString();
            const QString label = metadata.value(QStringLiteral("labels")).toObject().value(name).toString();
            if (role == Qt::ForegroundRole && index.column() == KDirModel::Name) {
                if (!color.isEmpty()) {
                    return QColor(color);
                }
                const QColor extensionColor = m_fileColors.colorForFileName(name);
                if (extensionColor.isValid()) {
                    return extensionColor;
                }
                const QColor labelColor = LabelColors::rgb(LabelColors::fromKey(label));
                if (labelColor.isValid()) {
                    return labelColor;
                }
            }
            if (role == Qt::ToolTipRole) {
                const QString note = metadata.value(QStringLiteral("notes")).toObject().value(name).toString();
                if (!note.isEmpty()) {
                    const QString defaultTip = KDirSortFilterProxyModel::data(index, role).toString();
                    return defaultTip.isEmpty() ? note : defaultTip + u'\n' + note;
                }
            }
        }
        if (role == Qt::BackgroundRole && index.column() == KDirModel::Name && m_filter.appliesTo(itemAt(index).isDir())
            && m_filter.text().trimmed().startsWith(u'#') == false && m_filter.text().trimmed().startsWith(u'*') == false) {
            QColor highlight = QApplication::palette().color(QPalette::Highlight);
            highlight.setAlpha(45);
            return highlight;
        }
        if (role == Qt::FontRole && index.column() == KDirModel::Name && m_filter.appliesTo(itemAt(index).isDir()) && !m_filter.text().trimmed().isEmpty()
            && !m_filter.text().contains(u'*') && !m_filter.text().contains(u'?') && !m_filter.text().contains(u'[')
            && itemAt(index).name().contains(m_filter.text().trimmed(), Qt::CaseInsensitive)) {
            QFont font = QApplication::font();
            font.setBold(true);
            return font;
        }
        const QVariant value = KDirSortFilterProxyModel::data(index, role);
        if (role == Qt::DecorationRole && value.typeId() == QMetaType::QIcon) {
            return SymbolicIcon::wrap(value.value<QIcon>());
        }
        return value;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && section == KDirModel::Group && m_ratingColumn && role == Qt::DisplayRole) {
            return i18nc("@title:column", "Rating");
        }
        if (orientation == Qt::Horizontal && isRelativePath(section) && role == Qt::DisplayRole) {
            return i18nc("@title:column path below the flat folder", "Relative Path");
        }
        return KDirSortFilterProxyModel::headerData(section, orientation, role);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (!KDirSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent)) {
            return false;
        }
        const auto *model = static_cast<const KDirModel *>(sourceModel());
        const KFileItem item = model->itemForIndex(model->index(sourceRow, 0, sourceParent));
        if (m_flatBase.isValid() && item.isDir()) {
            return false;
        }
        const QString name = item.name();
        const QJsonObject metadata = metadataForItem(item);
        const QString label = metadata.value(QStringLiteral("labels")).toObject().value(name).toString();
        const int rating = metadata.value(QStringLiteral("ratings")).toObject().value(name).toInt();
        return !m_ignoreList.ignores(name) && m_filter.matches(name, item.isDir(), label, rating);
    }

    bool subSortLessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        QStringList pins;
        const QJsonArray pinArray = metadataForItem(itemAt(left)).value(QStringLiteral("pins")).toArray();
        QStringList rightPins;
        const QJsonArray rightPinArray = metadataForItem(itemAt(right)).value(QStringLiteral("pins")).toArray();
        for (const QJsonValue &pin : pinArray) {
            pins.append(pin.toString());
        }
        for (const QJsonValue &pin : rightPinArray) {
            rightPins.append(pin.toString());
        }
        const bool leftPinned = pins.contains(itemAt(left).name());
        const bool rightPinned = rightPins.contains(itemAt(right).name());
        if (leftPinned != rightPinned) {
            return sortOrder() == Qt::DescendingOrder ? !leftPinned : leftPinned;
        }
        if (m_groupBy != QLatin1String("none")) {
            const int byGroup = QString::localeAwareCompare(groupSortKey(left), groupSortKey(right));
            if (byGroup != 0) {
                return sortOrder() == Qt::DescendingOrder ? byGroup > 0 : byGroup < 0;
            }
        }
        if (m_ratingColumn && sortColumn() == KDirModel::Group) {
            const int leftRating = metadataForItem(itemAt(left)).value(QStringLiteral("ratings")).toObject().value(itemAt(left).name()).toInt();
            const int rightRating = metadataForItem(itemAt(right)).value(QStringLiteral("ratings")).toObject().value(itemAt(right).name()).toInt();
            if (leftRating != rightRating) {
                return leftRating < rightRating;
            }
        }
        if (isRelativePath(left.column())) {
            const int byPath = QString::localeAwareCompare(FlatView::relativeFolder(m_flatBase, itemAt(left).url()),
                                                           FlatView::relativeFolder(m_flatBase, itemAt(right).url()));
            if (byPath != 0) {
                return byPath < 0;
            }
        }
        return KDirSortFilterProxyModel::subSortLessThan(left, right);
    }

private:
    bool isRelativePath(int column) const
    {
        return m_flatBase.isValid() && column == KDirModel::Type;
    }

    /// The item at a proxy or source index of this model's KDirModel.
    KFileItem itemAt(const QModelIndex &index) const
    {
        const QModelIndex source = index.model() == this ? mapToSource(index) : index;
        return static_cast<const KDirModel *>(sourceModel())->itemForIndex(source);
    }

    QJsonObject metadataForItem(const KFileItem &item) const
    {
        const QString localPath = item.localPath();
        if (localPath.isEmpty()) {
            return m_metadata;
        }
        const QString directory = QFileInfo(localPath).absolutePath();
        if (directory == m_metadataDirectory) {
            return m_metadata;
        }
        const auto found = m_metadataCache.constFind(directory);
        if (found != m_metadataCache.cend()) {
            return found.value();
        }
        const QJsonObject metadata = FolderMetadata::read(directory);
        m_metadataCache.insert(directory, metadata);
        return metadata;
    }

    QString groupSortKey(const QModelIndex &index) const
    {
        const KFileItem item = itemAt(index);
        if ((m_groupBy == QLatin1String("type") || m_groupBy == QLatin1String("ext")) && item.isDir()) {
            return QStringLiteral("0");
        }
        if (m_groupBy == QLatin1String("size")) {
            if (item.isDir()) {
                return QStringLiteral("0");
            }
            const qint64 size = item.size();
            return size < 1024 * 1024         ? QStringLiteral("1")
                : size < 10LL * 1024 * 1024   ? QStringLiteral("2")
                : size < 100LL * 1024 * 1024  ? QStringLiteral("3")
                : size < 1024LL * 1024 * 1024 ? QStringLiteral("4")
                                              : QStringLiteral("5");
        }
        if (m_groupBy == QLatin1String("date")) {
            const QDate modified = item.time(KFileItem::ModificationTime).date();
            const qint64 daysAgo = modified.daysTo(QDate::currentDate());
            return daysAgo <= 0                                                                                      ? QStringLiteral("0")
                : daysAgo == 1                                                                                       ? QStringLiteral("1")
                : daysAgo < 7                                                                                        ? QStringLiteral("2")
                : modified.month() == QDate::currentDate().month() && modified.year() == QDate::currentDate().year() ? QStringLiteral("3")
                : modified.year() == QDate::currentDate().year()                                                     ? QStringLiteral("4")
                                                                                                                     : QStringLiteral("5");
        }
        return QStringLiteral("1") + groupLabel(index).toCaseFolded();
    }

    QUrl m_flatBase;
    NameFilter m_filter;
    IgnoreList m_ignoreList;
    FileColorRules m_fileColors;
    ViewSettings m_settings;
    QJsonObject m_metadata;
    QString m_metadataDirectory;
    mutable QHash<QString, QJsonObject> m_metadataCache;
    QString m_groupBy = QStringLiteral("none");
    bool m_ratingColumn = false;
};

namespace
{
QRect selectionMarkerRect(const QRect &itemRect, Qt::LayoutDirection direction)
{
    const int size = 16;
    return QRect(direction == Qt::RightToLeft ? itemRect.right() - size + 1 : itemRect.left(), itemRect.bottom() - size + 1, size, size);
}

void paintSelectionMarker(QPainter *painter, const QStyleOptionViewItem &option)
{
    QStyleOptionButton check;
    check.rect = selectionMarkerRect(option.rect, option.direction);
    check.state = QStyle::State_Enabled | ((option.state & QStyle::State_Selected) ? QStyle::State_On : QStyle::State_Off);
    QApplication::style()->drawControl(QStyle::CE_CheckBox, &check, painter);
}
}

class GroupHeaderDelegate : public KFileItemDelegate
{
public:
    explicit GroupHeaderDelegate(NameFilterProxyModel *model, FolderView *folder, QObject *parent)
        : KFileItemDelegate(parent)
        , m_model(model)
        , m_folder(folder)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = KFileItemDelegate::sizeHint(option, index);
        size.rheight() = qMax(1, size.height() + 2 * (m_itemSpace - 2));
        if (firstInGroup(index)) {
            size.rheight() += HeaderHeight;
        }
        return size;
    }

    void setItemSpace(int space)
    {
        m_itemSpace = space;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (!firstInGroup(index)) {
            KFileItemDelegate::paint(painter, option, index);
            if (index.column() == 0 && (m_folder->selectionMarkersVisible() || m_folder->selectionMode())) {
                paintSelectionMarker(painter, option);
            }
            return;
        }
        const QRect headerRect(option.rect.left(), option.rect.top(), option.rect.width(), HeaderHeight);
        if (index.column() == 0) {
            painter->fillRect(headerRect, option.palette.alternateBase());
            QFont font = option.font;
            font.setBold(true);
            painter->setFont(font);
            painter->setPen(option.palette.color(QPalette::Text));
            painter->drawText(headerRect.adjusted(8, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, m_model->groupLabel(index));
        }
        QStyleOptionViewItem itemOption(option);
        itemOption.rect.setTop(itemOption.rect.top() + HeaderHeight);
        KFileItemDelegate::paint(painter, itemOption, index);
        if (index.column() == 0 && (m_folder->selectionMarkersVisible() || m_folder->selectionMode())) {
            paintSelectionMarker(painter, itemOption);
        }
    }

private:
    bool firstInGroup(const QModelIndex &index) const
    {
        if (m_model->groupBy() == QLatin1String("none")) {
            return false;
        }
        return index.row() == 0 || m_model->groupLabel(index.sibling(index.row(), 0)) != m_model->groupLabel(index.sibling(index.row() - 1, 0));
    }

    static constexpr int HeaderHeight = 20;
    NameFilterProxyModel *m_model;
    FolderView *m_folder;
    int m_itemSpace = 2;
};

class SelectionMarkerDelegate : public KFileItemDelegate
{
public:
    explicit SelectionMarkerDelegate(FolderView *folder, QObject *parent)
        : KFileItemDelegate(parent)
        , m_folder(folder)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        KFileItemDelegate::paint(painter, option, index);
        if (m_folder->selectionMarkersVisible() || m_folder->selectionMode()) {
            paintSelectionMarker(painter, option);
        }
    }

private:
    FolderView *m_folder;
};

FolderView::FolderView(QWidget *parent)
    : QWidget(parent)
    , m_lister(new KDirLister(this))
    , m_model(new KDirModel(this))
    , m_proxy(new NameFilterProxyModel(this))
    , m_stack(new QStackedWidget(this))
    , m_details(new DetailsView(this))
    , m_icons(new KCategorizedView(this))
    , m_compact(new QListView(this))
{
    m_lister->setAutoErrorHandlingEnabled(false);
    m_lister->setMainWindow(window());
    m_model->setDirLister(m_lister);
    m_model->setJobTransfersVisible(true);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortFoldersFirst(true);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_details->setModel(m_proxy);
    m_details->setRootIsDecorated(false);
    m_details->setItemsExpandable(false);
    m_details->setUniformRowHeights(false);
    m_details->setSortingEnabled(true);
    m_details->sortByColumn(KDirModel::Name, Qt::AscendingOrder);
    m_details->setAllColumnsShowFocus(true);
    m_details->header()->setSectionsMovable(true);
    m_details->header()->setStretchLastSection(false);
    m_details->header()->setSectionResizeMode(KDirModel::Name, QHeaderView::Stretch);
    m_details->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    for (int column : {KDirModel::Permissions, KDirModel::Owner, KDirModel::Group}) {
        m_details->setColumnHidden(column, true);
    }
    setupView(m_details);

    m_icons->setModel(m_proxy);
    static_cast<KCategorizedView *>(m_icons)->setCategoryDrawer(new KCategoryDrawer(static_cast<KCategorizedView *>(m_icons)));
    m_icons->setSelectionModel(m_details->selectionModel());
    m_icons->setViewMode(QListView::IconMode);
    m_icons->setResizeMode(QListView::Adjust);
    m_icons->setFlow(QListView::LeftToRight);
    m_icons->setWrapping(true);
    m_icons->setMovement(QListView::Static);
    m_icons->setUniformItemSizes(true);
    m_icons->setWordWrap(true);
    m_icons->setIconSize(QSize(64, 64));
    m_icons->setGridSize(QSize(112, 112));
    m_icons->setSpacing(4);
    setupView(m_icons);

    m_compact->setModel(m_proxy);
    m_compact->setSelectionModel(m_details->selectionModel());
    m_compact->setViewMode(QListView::ListMode);
    m_compact->setFlow(QListView::TopToBottom);
    m_compact->setWrapping(true);
    m_compact->setResizeMode(QListView::Adjust);
    m_compact->setMovement(QListView::Static);
    m_compact->setIconSize(QSize(16, 16));
    setupView(m_compact);

    m_detailsPreviews = new KFilePreviewGenerator(m_details);
    m_iconPreviews = new KFilePreviewGenerator(m_icons);
    m_compactPreviews = new KFilePreviewGenerator(m_compact);
    setPreviewsEnabled(true);

    m_stack->addWidget(m_details);
    m_stack->addWidget(m_icons);
    m_stack->addWidget(m_compact);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);
    m_selectionBar = new QWidget(this);
    m_selectionBar->setObjectName(QStringLiteral("selectionBar"));
    auto *barLayout = new QHBoxLayout(m_selectionBar);
    barLayout->setContentsMargins(0, 0, 0, 0);
    const auto addButton = [this, barLayout](const QString &name, const QString &text, const QString &icon, auto slot) {
        auto *button = new QToolButton(m_selectionBar);
        button->setObjectName(name);
        button->setText(text);
        button->setIcon(SymbolicIcon::fromTheme(icon));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        connect(button, &QToolButton::clicked, this, slot);
        barLayout->addWidget(button);
        return button;
    };
    auto *copy = addButton(QStringLiteral("selectionCopy"), i18nc("@action", "Copy"), QStringLiteral("edit-copy"), &FolderView::copySelectionRequested);
    auto *cut = addButton(QStringLiteral("selectionCut"), i18nc("@action", "Cut"), QStringLiteral("edit-cut"), &FolderView::cutSelectionRequested);
    auto *trash =
        addButton(QStringLiteral("selectionTrash"), i18nc("@action", "Move to Trash"), QStringLiteral("user-trash"), &FolderView::trashSelectionRequested);
    barLayout->addStretch();
    addButton(QStringLiteral("selectionDone"), i18nc("@action", "Done"), QStringLiteral("dialog-ok"), [this] {
        setSelectionMode(false);
    });
    const auto updateSelectionButtons = [this, copy, cut, trash] {
        const KFileItemList items = selectedItems();
        const KFileItemListProperties properties(items);
        copy->setEnabled(!items.isEmpty());
        cut->setEnabled(!items.isEmpty() && properties.supportsMoving());
        trash->setEnabled(!items.isEmpty() && properties.supportsDeleting() && properties.isLocal());
    };
    connect(this, &FolderView::selectionChanged, this, updateSelectionButtons);
    updateSelectionButtons();
    m_selectionBar->hide();
    layout->addWidget(m_selectionBar);

    connect(m_lister, &KDirLister::started, this, [this] {
        Q_EMIT loadingChanged(true);
    });
    connect(m_lister, &KDirLister::completed, this, [this] {
        Q_EMIT loadingChanged(false);
        Q_EMIT itemCountChanged();
    });
    connect(m_lister, &KDirLister::canceled, this, [this] {
        Q_EMIT loadingChanged(false);
    });
    connect(m_lister, &KDirLister::redirection, this, [this](const QUrl &, const QUrl &newUrl) {
        if (!m_flat) {
            m_folder = newUrl;
        }
        Q_EMIT redirected(newUrl);
    });
    connect(m_lister, &KDirLister::jobError, this, [this](KIO::Job *job) {
        Q_EMIT loadingChanged(false);
        Q_EMIT listingFailed(job->errorString());
    });
    connect(m_lister, &KDirLister::itemsAdded, this, &FolderView::itemCountChanged);
    connect(m_lister, &KDirLister::itemsDeleted, this, &FolderView::itemCountChanged);
    connect(m_lister, &KDirLister::clear, this, &FolderView::itemCountChanged);

    connect(m_details->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FolderView::selectionChanged);
    connect(m_details->selectionModel(), &QItemSelectionModel::currentChanged, this, &FolderView::selectionChanged);
    connect(m_details->header(), &QHeaderView::customContextMenuRequested, this, &FolderView::showColumnMenu);
    connect(m_details->header(), &QHeaderView::sortIndicatorChanged, this, [this] {
        saveFolderFormat();
    });
    connect(m_details->header(), &QHeaderView::sectionMoved, this, [this] {
        saveFolderFormat();
    });
}

FolderView::~FolderView()
{
    // ~KCoreDirLister emits clear() while ~QWidget deletes children, after this type is gone.
    m_lister->disconnect(this);
}

void FolderView::setupView(QAbstractItemView *view)
{
    if (view == m_details) {
        view->setItemDelegate(new GroupHeaderDelegate(m_proxy, this, view));
    } else if (view == m_icons) {
        m_iconDelegate = new SelectionMarkerDelegate(this, view);
        view->setItemDelegate(m_iconDelegate);
    } else {
        view->setItemDelegate(new SelectionMarkerDelegate(this, view));
    }
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setDragEnabled(true);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);
    view->setDefaultDropAction(Qt::MoveAction);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setFocusPolicy(Qt::StrongFocus);
    view->viewport()->installEventFilter(this);
    view->installEventFilter(this);

    connect(view, &QAbstractItemView::activated, this, &FolderView::onActivated);
    connect(view, &QAbstractItemView::customContextMenuRequested, this, &FolderView::onContextMenu);
}

QUrl FolderView::url() const
{
    return m_folder;
}

void FolderView::setUrl(const QUrl &url)
{
    if (!url.isValid() || url == m_folder) {
        return;
    }
    m_folder = url;
    if (m_flat) {
        m_proxy->setFlatBase(url);
    }
    loadFolderFormat();
    openListing();
    updatePreviews();
    Q_EMIT urlChanged(url);
}

void FolderView::openListing()
{
    m_lister->openUrl(m_flat ? FlatView::listingUrl(m_folder) : m_folder);
}

void FolderView::reload()
{
    if (m_lister->url().isValid()) {
        m_lister->openUrl(m_lister->url(), KDirLister::Reload);
    }
}

FolderView::ViewMode FolderView::viewMode() const
{
    return m_viewMode;
}

bool FolderView::selectionMarkersVisible() const
{
    return m_selectionMarkers;
}

void FolderView::setSelectionMarkersVisible(bool visible)
{
    m_selectionMarkers = visible;
    currentView()->viewport()->update();
}

bool FolderView::selectionMode() const
{
    return m_selectionMode;
}

void FolderView::setSelectionMode(bool enabled)
{
    if (m_selectionMode == enabled) {
        return;
    }
    m_selectionMode = enabled;
    m_selectionBar->setVisible(enabled);
    currentView()->viewport()->update();
    Q_EMIT selectionModeChanged(enabled);
}

void FolderView::setViewMode(ViewMode mode)
{
    if (mode == m_viewMode) {
        return;
    }
    const bool hadFocus = currentView()->hasFocus();
    m_viewMode = mode;
    m_stack->setCurrentWidget(currentView());
    if (hadFocus) {
        currentView()->setFocus();
    }
    saveFolderFormat();
}

bool FolderView::showHiddenFiles() const
{
    return m_lister->showHiddenFiles();
}

void FolderView::setShowHiddenFiles(bool show)
{
    if (show == m_lister->showHiddenFiles()) {
        return;
    }
    m_lister->setShowHiddenFiles(show);
    m_lister->emitChanges();
}

bool FolderView::previewsEnabled() const
{
    return m_previewsEnabled;
}

void FolderView::setPreviewsEnabled(bool enabled)
{
    m_previewsEnabled = enabled;
    updatePreviews();
}

void FolderView::setRemotePreviewsEnabled(bool enabled)
{
    m_remotePreviewsEnabled = enabled;
    updatePreviews();
}

void FolderView::setPreviewPlugins(const QStringList &plugins)
{
    m_detailsPreviews->setEnabledPlugins(plugins);
    m_iconPreviews->setEnabledPlugins(plugins);
    m_compactPreviews->setEnabledPlugins(plugins);
}

void FolderView::setIconSizes(int details, int icons)
{
    m_details->setIconSize(QSize(details, details));
    m_icons->setIconSize(QSize(icons, icons));
    m_icons->setGridSize(QSize(icons + 48, icons + 48));
}

void FolderView::setItemSpacing(int detailsSpace, int iconsSpace)
{
    auto *delegate = static_cast<GroupHeaderDelegate *>(m_details->itemDelegate());
    delegate->setItemSpace(detailsSpace);
    m_details->doItemsLayout();
    m_icons->setSpacing(2 * iconsSpace);
}

void FolderView::setDragFromAnywhere(bool enabled)
{
    m_details->setDragFromAnywhere(enabled);
}

void FolderView::setColumnVisible(int column, bool visible)
{
    if (column != KDirModel::Name) {
        m_details->setColumnHidden(column, !visible);
    }
}

void FolderView::setSorting(int column, Qt::SortOrder order)
{
    m_details->sortByColumn(column, order);
}

void FolderView::setSortFoldersFirst(bool foldersFirst)
{
    m_proxy->setSortFoldersFirst(foldersFirst);
}

void FolderView::setShowToolTips(bool show)
{
    auto *detailsDelegate = static_cast<GroupHeaderDelegate *>(m_details->itemDelegate());
    detailsDelegate->setShowToolTipWhenElided(show);
    m_iconDelegate->setShowToolTipWhenElided(show);
    static_cast<KFileItemDelegate *>(m_compact->itemDelegate())->setShowToolTipWhenElided(show);
}

void FolderView::setDropAction(const QString &action)
{
    m_dropAction = action;
    const Qt::DropAction dropAction = action == QLatin1String("Copy") ? Qt::CopyAction : Qt::MoveAction;
    m_details->setDefaultDropAction(dropAction);
    m_icons->setDefaultDropAction(dropAction);
    m_compact->setDefaultDropAction(dropAction);
}

void FolderView::updatePreviews()
{
    const bool shown = m_previewsEnabled && (m_folder.isLocalFile() || m_remotePreviewsEnabled);
    m_detailsPreviews->setPreviewShown(shown);
    m_iconPreviews->setPreviewShown(shown);
    m_compactPreviews->setPreviewShown(shown);
}

KFileItem FolderView::rootItem() const
{
    return m_flat ? KFileItem(m_folder) : m_lister->rootItem();
}

KFileItemList FolderView::items() const
{
    return m_lister->items();
}

KFileItemList FolderView::selectedItems() const
{
    KFileItemList result;
    const QModelIndexList rows = currentView()->selectionModel()->selectedRows();
    result.reserve(rows.size());
    for (const QModelIndex &index : rows) {
        const KFileItem item = m_model->itemForIndex(m_proxy->mapToSource(index));
        if (!item.isNull()) {
            result.append(item);
        }
    }
    return result;
}

KFileItemList FolderView::selectedItemsOrRoot() const
{
    KFileItemList result = selectedItems();
    if (result.isEmpty()) {
        const KFileItem root = rootItem();
        if (!root.isNull()) {
            result.append(root);
        }
    }
    return result;
}

KFileItem FolderView::currentItem() const
{
    return m_model->itemForIndex(m_proxy->mapToSource(currentView()->currentIndex()));
}

int FolderView::itemCount() const
{
    return m_proxy->rowCount();
}

void FolderView::selectAll()
{
    currentView()->selectAll();
}

int FolderView::applySelectionPattern(const QString &pattern, bool select)
{
    const NameFilter filter(pattern, NameFilter::Scope::FilesAndFolders);
    QItemSelectionModel *selection = currentView()->selectionModel();
    const QItemSelectionModel::SelectionFlags operation = select ? QItemSelectionModel::Select : QItemSelectionModel::Deselect;
    int matched = 0;
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QModelIndex index = m_proxy->index(row, 0);
        const KFileItem item = m_model->itemForIndex(m_proxy->mapToSource(index));
        if (filter.matches(item.name(), item.isDir())) {
            selection->select(index, operation | QItemSelectionModel::Rows);
            ++matched;
        }
    }
    return matched;
}

void FolderView::clearSelection()
{
    currentView()->selectionModel()->clearSelection();
}

void FolderView::selectUrls(const QList<QUrl> &urls)
{
    QItemSelectionModel *selection = currentView()->selectionModel();
    selection->clearSelection();
    bool first = true;
    for (const QUrl &url : urls) {
        const QModelIndex index = m_proxy->mapFromSource(m_model->indexForUrl(url));
        if (!index.isValid()) {
            continue;
        }
        selection->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        if (first) {
            selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            currentView()->scrollTo(index);
            first = false;
        }
    }
}

QAbstractItemView *FolderView::currentView() const
{
    if (m_viewMode == ViewMode::Details) {
        return m_details;
    }
    return m_viewMode == ViewMode::Icons ? m_icons : m_compact;
}

NameFilter FolderView::nameFilter() const
{
    return m_proxy->nameFilter();
}

void FolderView::setNameFilter(const NameFilter &filter)
{
    if (filter == m_proxy->nameFilter()) {
        return;
    }
    m_proxy->setNameFilter(filter);
    Q_EMIT itemCountChanged();
}

bool FolderView::isFlat() const
{
    return m_flat;
}

void FolderView::setFlat(bool flat)
{
    if (flat == m_flat) {
        return;
    }
    m_flat = flat;
    m_proxy->setFlatBase(flat ? m_folder : QUrl());
    if (m_folder.isValid()) {
        openListing();
    }
}

void FolderView::reloadMetadata()
{
    QString error;
    const QJsonObject metadata = m_folder.isLocalFile() ? FolderMetadata::read(m_folder.toLocalFile(), &error) : QJsonObject();
    const ViewSettings settings = ViewSettings::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("View")));
    const KConfigGroup filesGroup = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
    m_proxy->setSettings(settings, FileColorRules::fromConfig(filesGroup), IgnoreList::fromConfig(filesGroup));
    m_proxy->setMetadata(metadata, m_folder.isLocalFile() ? m_folder.toLocalFile() : QString());
    if (!error.isEmpty()) {
        Q_EMIT listingFailed(error);
    }
}

QString FolderView::groupBy() const
{
    return m_proxy->groupBy();
}

void FolderView::setGroupBy(const QString &group)
{
    m_proxy->setGroupBy(group);
    saveFolderFormat();
}

void FolderView::reloadSettings()
{
    loadFolderFormat();
}

void FolderView::loadFolderFormat()
{
    m_applyingFolderFormat = true;
    reloadMetadata();
    const KConfigGroup defaults = KSharedConfig::openConfig()->group(QStringLiteral("Views"));
    const ViewSettings settings = ViewSettings::fromConfig(KSharedConfig::openConfig()->group(QStringLiteral("View")));
    for (const auto &[column, key] : OptionalColumns) {
        const bool visible = column == KDirModel::Group
            ? settings.visibleColumns.contains(QStringLiteral("group")) || settings.visibleColumns.contains(QStringLiteral("rating"))
            : settings.visibleColumns.contains(key);
        m_details->setColumnHidden(column, !visible);
    }

    FolderMetadata::Format format;
    if (m_folder.isLocalFile() && FolderMetadata::format(m_proxy->metadata(), &format)) {
        setViewMode(format.view == QLatin1String("grid") ? ViewMode::Icons : format.view == QLatin1String("compact") ? ViewMode::Compact : ViewMode::Details);
        m_proxy->setRatingColumn(format.columns.contains(QStringLiteral("rating")));
        for (const auto &[column, key] : OptionalColumns) {
            const bool visible = column == KDirModel::Group
                ? format.columns.contains(QStringLiteral("group")) || format.columns.contains(QStringLiteral("rating"))
                : format.columns.contains(key);
            m_details->setColumnHidden(column, !visible);
        }
        m_details->sortByColumn(columnForSortKey(format.sortKey), format.sortAscending ? Qt::AscendingOrder : Qt::DescendingOrder);
    } else {
        const QString defaultMode = defaults.readEntry("DefaultViewMode", QStringLiteral("Details"));
        setViewMode(defaultMode == QLatin1String("Icons") ? ViewMode::Icons : ViewMode::Details);
        const QString sortKey = defaults.readEntry("SortColumn", QStringLiteral("Name"));
        const QString normalizedSortKey = sortKey == QLatin1String("ModifiedTime") ? QStringLiteral("date")
            : sortKey == QLatin1String("Permissions")                              ? QStringLiteral("mode")
            : sortKey == QLatin1String("Owner")                                    ? QStringLiteral("owner")
            : sortKey == QLatin1String("Group")                                    ? QStringLiteral("group")
                                                                                   : sortKey.toLower();
        m_details->sortByColumn(columnForSortKey(normalizedSortKey), defaults.readEntry("SortDescending", false) ? Qt::DescendingOrder : Qt::AscendingOrder);
        setShowHiddenFiles(defaults.readEntry("ShowHiddenFiles", false));
    }
    m_proxy->setSortFoldersFirst(defaults.readEntry("FoldersFirst", true));
    setIconSizes(defaults.readEntry("DetailsIconSize", 16), defaults.readEntry("IconsIconSize", 64));
    const auto itemSpace = [&defaults](const QString &densityKey, const QString &customKey) {
        const QString density = defaults.readEntry(densityKey, QStringLiteral("Normal"));
        return density == QLatin1String("VeryCompact") ? 0
            : density == QLatin1String("Compact")      ? 1
            : density == QLatin1String("Custom")       ? qBound(0, defaults.readEntry(customKey, 2), 10)
                                                       : 2;
    };
    setItemSpacing(itemSpace(QStringLiteral("DetailsDensity"), QStringLiteral("DetailsCustomSpace")),
                   itemSpace(QStringLiteral("IconsDensity"), QStringLiteral("IconsCustomSpace")));
    setDragFromAnywhere(defaults.readEntry("DragFromAnywhere", false));
    setShowToolTips(defaults.readEntry("ShowToolTips", true));

    KFileItemDelegate::InformationList information;
    for (const QString &key : settings.iconInformation) {
        if (key == QLatin1String("size")) {
            information.append(KFileItemDelegate::Size);
        } else if (key == QLatin1String("permissions")) {
            information.append(KFileItemDelegate::Permissions);
        } else if (key == QLatin1String("octalPermissions")) {
            information.append(KFileItemDelegate::OctalPermissions);
        } else if (key == QLatin1String("owner")) {
            information.append(KFileItemDelegate::Owner);
        } else if (key == QLatin1String("ownerAndGroup")) {
            information.append(KFileItemDelegate::OwnerAndGroup);
        } else if (key == QLatin1String("creationTime")) {
            information.append(KFileItemDelegate::CreationTime);
        } else if (key == QLatin1String("modificationTime")) {
            information.append(KFileItemDelegate::ModificationTime);
        } else if (key == QLatin1String("accessTime")) {
            information.append(KFileItemDelegate::AccessTime);
        } else if (key == QLatin1String("mimeType")) {
            information.append(KFileItemDelegate::MimeType);
        } else if (key == QLatin1String("friendlyMimeType")) {
            information.append(KFileItemDelegate::FriendlyMimeType);
        } else if (key == QLatin1String("linkDest")) {
            information.append(KFileItemDelegate::LinkDest);
        } else if (key == QLatin1String("localPathOrUrl")) {
            information.append(KFileItemDelegate::LocalPathOrUrl);
        } else if (key == QLatin1String("comment")) {
            information.append(KFileItemDelegate::Comment);
        }
    }
    m_iconDelegate->setShowInformation(information);
    const int rowHeight = fontMetrics().height();
    const int gridHeight = qMax(112, 64 + rowHeight * (settings.iconNameLines + information.size()) + 16);
    m_icons->setGridSize(QSize(112, gridHeight));
    m_iconDelegate->setMaximumSize(QSize(112, gridHeight));
    m_icons->doItemsLayout();
    m_applyingFolderFormat = false;
}

void FolderView::saveFolderFormat()
{
    if (m_applyingFolderFormat || m_flat || !m_folder.isLocalFile()) {
        return;
    }
    FolderMetadata::Format format;
    format.view = m_viewMode == ViewMode::Icons ? QStringLiteral("grid") : m_viewMode == ViewMode::Compact ? QStringLiteral("compact") : QStringLiteral("list");
    for (int visual = 0; visual < m_details->header()->count(); ++visual) {
        const int column = m_details->header()->logicalIndex(visual);
        if (column == KDirModel::Name || m_details->isColumnHidden(column)) {
            continue;
        }
        if (column == KDirModel::Group) {
            format.columns.append(m_proxy->ratingColumn() ? QStringLiteral("rating") : QStringLiteral("group"));
            continue;
        }
        for (const auto &[candidate, key] : OptionalColumns) {
            if (candidate == column) {
                format.columns.append(key);
                break;
            }
        }
    }
    format.sortKey = sortKeyForColumn(m_details->header()->sortIndicatorSection(), m_proxy->ratingColumn());
    format.sortAscending = m_details->header()->sortIndicatorOrder() == Qt::AscendingOrder;
    format.group = m_proxy->groupBy();
    QString error;
    if (!FolderMetadata::setFormat(m_folder.toLocalFile(), format, &error)) {
        Q_EMIT listingFailed(error);
        return;
    }
    reloadMetadata();
}

void FolderView::showColumnMenu(const QPoint &pos)
{
    QMenu menu(this);
    for (const auto &[column, key] : OptionalColumns) {
        Q_UNUSED(key)
        if (column == KDirModel::Group) {
            continue;
        }
        QString text;
        switch (column) {
        case KDirModel::Type:
            text = i18nc("@option:check", "Type");
            break;
        case KDirModel::Size:
            text = i18nc("@option:check", "Size");
            break;
        case KDirModel::ModifiedTime:
            text = i18nc("@option:check", "Modified");
            break;
        case KDirModel::Permissions:
            text = i18nc("@option:check", "Permissions");
            break;
        case KDirModel::Owner:
            text = i18nc("@option:check", "Owner");
            break;
        case KDirModel::Group:
            text = i18nc("@option:check", "Group");
            break;
        default:
            continue;
        }
        QAction *action = menu.addAction(text);
        action->setCheckable(true);
        action->setChecked(!m_details->isColumnHidden(column));
        connect(action, &QAction::toggled, this, [this, column](bool shown) {
            m_details->setColumnHidden(column, !shown);
            saveFolderFormat();
        });
    }
    QMenu *groupColumn = menu.addMenu(i18nc("@menu", "Group Column"));
    const bool groupVisible = !m_details->isColumnHidden(KDirModel::Group);
    const QList<QPair<bool, QString>> groupColumnOptions{
        {false, i18nc("@option:check", "Group")},
        {true, i18nc("@option:check", "Rating")},
    };
    auto *groupColumnActions = new QActionGroup(groupColumn);
    groupColumnActions->setExclusive(true);
    for (const auto &[rating, label] : groupColumnOptions) {
        QAction *action = groupColumn->addAction(label);
        action->setCheckable(true);
        action->setChecked(groupVisible && m_proxy->ratingColumn() == rating);
        groupColumnActions->addAction(action);
        connect(action, &QAction::triggered, this, [this, rating] {
            m_proxy->setRatingColumn(rating);
            m_details->setColumnHidden(KDirModel::Group, false);
            saveFolderFormat();
        });
    }
    QAction *hideGroupColumn = groupColumn->addAction(i18nc("@option:check", "Hide Column"));
    hideGroupColumn->setCheckable(true);
    hideGroupColumn->setChecked(!groupVisible);
    groupColumnActions->addAction(hideGroupColumn);
    connect(hideGroupColumn, &QAction::triggered, this, [this] {
        m_details->setColumnHidden(KDirModel::Group, true);
        saveFolderFormat();
    });
    menu.addSeparator();
    QAction *fitColumn = menu.addAction(i18nc("@action:inmenu", "Fit Column to Contents"));
    const int column = m_details->header()->logicalIndexAt(pos);
    fitColumn->setEnabled(column >= 0);
    connect(fitColumn, &QAction::triggered, this, [this, column] {
        m_details->header()->setSectionResizeMode(column, QHeaderView::Interactive);
        m_details->resizeColumnToContents(column);
    });
    QAction *fitAll = menu.addAction(i18nc("@action:inmenu", "Fit All Columns to Contents"));
    connect(fitAll, &QAction::triggered, this, [this] {
        for (int section = 0; section < m_details->header()->count(); ++section) {
            if (!m_details->isColumnHidden(section)) {
                m_details->header()->setSectionResizeMode(section, QHeaderView::Interactive);
                m_details->resizeColumnToContents(section);
            }
        }
    });
    menu.addSeparator();
    QMenu *groups = menu.addMenu(i18nc("@menu", "Group By"));
    const QList<QPair<QString, QString>> options{
        {QStringLiteral("none"), i18nc("@option:check group files", "None")},
        {QStringLiteral("name"), i18nc("@option:check group files", "Name")},
        {QStringLiteral("type"), i18nc("@option:check group files", "Type")},
        {QStringLiteral("size"), i18nc("@option:check group files", "Size")},
        {QStringLiteral("date"), i18nc("@option:check group files", "Date")},
        {QStringLiteral("ext"), i18nc("@option:check group files", "Extension")},
    };
    auto *groupActions = new QActionGroup(groups);
    groupActions->setExclusive(true);
    for (const auto &[key, label] : options) {
        QAction *action = groups->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_proxy->groupBy() == key);
        groupActions->addAction(action);
        connect(action, &QAction::triggered, this, [this, key] {
            m_proxy->setGroupBy(key);
            saveFolderFormat();
        });
    }
    menu.exec(m_details->header()->mapToGlobal(pos));
}

void FolderView::activateFirstItem()
{
    const NameFilter filter = m_proxy->nameFilter();
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QModelIndex index = m_proxy->index(row, 0);
        if (filter.appliesTo(m_model->itemForIndex(m_proxy->mapToSource(index)).isDir())) {
            onActivated(index);
            return;
        }
    }
    if (m_proxy->rowCount() > 0) {
        onActivated(m_proxy->index(0, 0));
    }
}

void FolderView::onActivated(const QModelIndex &index)
{
    const KFileItem item = m_model->itemForIndex(m_proxy->mapToSource(index));
    if (!item.isNull()) {
        Q_EMIT itemActivated(item);
    }
}

void FolderView::onContextMenu(const QPoint &pos)
{
    auto *view = qobject_cast<QAbstractItemView *>(sender());
    if (!view) {
        return;
    }
    if (!view->indexAt(pos).isValid()) {
        clearSelection();
    }
    Q_EMIT contextMenuRequested(view->viewport()->mapToGlobal(pos), selectedItems());
}

KFileItem FolderView::itemAt(QAbstractItemView *view, const QPoint &viewportPos) const
{
    return m_model->itemForIndex(m_proxy->mapToSource(view->indexAt(viewportPos)));
}

bool FolderView::handleDrop(QAbstractItemView *view, QEvent *event)
{
    auto *dropEvent = static_cast<QDropEvent *>(event);
    if (m_flat) {
        // Items in a flat list come from many folders, so a drop has no single destination.
        dropEvent->ignore();
        return true;
    }
    const QMimeData *mime = dropEvent->mimeData();
    if (!mime || !mime->hasUrls()) {
        return false;
    }

    const KFileItem target = itemAt(view, dropEvent->position().toPoint());
    const bool onFolder = !target.isNull() && target.isDir();

    if (event->type() != QEvent::Drop) {
        // KIO::DropJob decides between copy, move and link; accept everything.
        if (m_dropAction == QLatin1String("Copy")) {
            dropEvent->setDropAction(Qt::CopyAction);
        } else if (m_dropAction == QLatin1String("Move")) {
            dropEvent->setDropAction(Qt::MoveAction);
        } else {
            dropEvent->setDropAction(Qt::CopyAction);
        }
        dropEvent->accept();
        return true;
    }

    QUrl destination = onFolder ? target.mostLocalUrl() : url();
    if (!destination.isValid()) {
        return false;
    }
    if (!onFolder) {
        // Dropping items onto their own folder is a no-op, not a copy.
        const QList<QUrl> urls = mime->urls();
        const bool allLocal = std::all_of(urls.cbegin(), urls.cend(), [&destination](const QUrl &source) {
            return source.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) == destination.adjusted(QUrl::StripTrailingSlash);
        });
        if (allLocal) {
            dropEvent->ignore();
            return true;
        }
    }

    if (m_dropAction == QLatin1String("Copy")) {
        dropEvent->setDropAction(Qt::CopyAction);
    } else if (m_dropAction == QLatin1String("Move")) {
        dropEvent->setDropAction(Qt::MoveAction);
    }
    KIO::DropJob *job = KIO::drop(dropEvent, destination);
    KJobWidgets::setWindow(job, window());
    dropEvent->acceptProposedAction();
    return true;
}

bool FolderView::eventFilter(QObject *watched, QEvent *event)
{
    auto *view = qobject_cast<QAbstractItemView *>(watched);
    if (!view) {
        view = qobject_cast<QAbstractItemView *>(watched->parent());
    }
    if (!view) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::Drop:
        if (watched == view->viewport() && handleDrop(view, event)) {
            return true;
        }
        break;
    case QEvent::FocusIn:
        Q_EMIT activated();
        break;
    case QEvent::MouseButtonPress:
        if (watched == view->viewport()) {
            Q_EMIT activated();
            auto *mouse = static_cast<QMouseEvent *>(event);
            const QModelIndex index = view->indexAt(mouse->position().toPoint());
            const QModelIndex nameIndex = index.siblingAtColumn(0);
            if (mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier && index.isValid()
                && (m_selectionMode
                    || (m_selectionMarkers
                        && selectionMarkerRect(view->visualRect(nameIndex), view->layoutDirection()).contains(mouse->position().toPoint())))) {
                view->selectionModel()->setCurrentIndex(nameIndex, QItemSelectionModel::NoUpdate);
                view->selectionModel()->select(nameIndex, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
                m_markerPress = true;
                return true;
            }
        }
        break;
    case QEvent::MouseButtonRelease:
        if (watched == view->viewport() && m_markerPress && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
            m_markerPress = false;
            return true;
        }
        break;
    case QEvent::MouseButtonDblClick:
        if (watched == view->viewport() && (m_selectionMode || m_selectionMarkers)) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            const QModelIndex index = view->indexAt(mouse->position().toPoint()).siblingAtColumn(0);
            if (index.isValid()
                && (m_selectionMode || selectionMarkerRect(view->visualRect(index), view->layoutDirection()).contains(mouse->position().toPoint()))) {
                m_markerPress = true;
                return true;
            }
        }
        break;
    case QEvent::KeyPress: {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (watched != view) {
            break;
        }
        const QString text = keyEvent->text();
        if ((keyEvent->modifiers() & ~Qt::ShiftModifier) == Qt::NoModifier && !text.isEmpty() && text.at(0).isPrint() && !text.trimmed().isEmpty()) {
            Q_EMIT typeAheadRequested(text);
            return true;
        }
        if (keyEvent->modifiers() != Qt::NoModifier) {
            break;
        }
        if (view == m_details && keyEvent->key() == Qt::Key_Left) {
            Q_EMIT goUpRequested();
            return true;
        }
        if (view == m_details && keyEvent->key() == Qt::Key_Right) {
            onActivated(view->currentIndex());
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            clearSelection();
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}
