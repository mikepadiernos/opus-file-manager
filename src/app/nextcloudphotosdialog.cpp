// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudphotosdialog.h"

#include "symbolicicon.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace
{
enum Column {
    NameColumn,
    TakenColumn,
    PlaceColumn,
};
constexpr int IndexRole = Qt::UserRole;
constexpr int PreviewSize = 256;

KConfigGroup stateGroup()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("NextcloudPhotosDialog"));
}

bool askConfirmation(QWidget *parent, const QString &title, const QString &text, const QString &action)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Cancel, parent);
    QPushButton *accept = box.addButton(action, QMessageBox::AcceptRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(box.button(QMessageBox::Cancel)));
    box.exec();
    return box.clickedButton() == accept;
}

std::optional<QString> askName(QWidget *parent)
{
    bool ok = false;
    const QString name =
        QInputDialog::getText(parent, i18nc("@title:window", "New Album"), i18nc("@label:textbox", "Album name:"), QLineEdit::Normal, QString(), &ok);
    return ok ? std::optional(name) : std::nullopt;
}

bool isShared(const Nextcloud::PhotoCollection &album)
{
    return album.kind == Nextcloud::PhotoCollectionKind::SharedAlbums || !album.collaborators.isEmpty();
}

QString collectionToolTip(const Nextcloud::PhotoCollection &collection)
{
    QStringList lines;
    if (collection.itemCount) {
        lines.append(i18ncp("@info:tooltip", "%1 photo", "%1 photos", *collection.itemCount));
    }
    if (!collection.location.isEmpty()) {
        lines.append(i18nc("@info:tooltip %1 is a place", "Location: %1", collection.location));
    }
    if (collection.start.isValid() && collection.end.isValid()) {
        const QLocale locale;
        lines.append(i18nc("@info:tooltip %1 and %2 are dates",
                           "Taken: %1 – %2",
                           locale.toString(collection.start.toLocalTime().date(), QLocale::ShortFormat),
                           locale.toString(collection.end.toLocalTime().date(), QLocale::ShortFormat)));
    }
    if (!collection.collaborators.isEmpty()) {
        lines.append(i18nc("@info:tooltip %1 is a list of people", "Shared with: %1", collection.collaborators.join(QStringLiteral(", "))));
    }
    if (collection.kind != Nextcloud::PhotoCollectionKind::Places && !Nextcloud::canAddPhotos(collection)) {
        lines.append(i18nc("@info:tooltip", "The album's owner doesn't allow you to add photos."));
    }
    return lines.join(QLatin1Char('\n'));
}

QString collectionText(const Nextcloud::PhotoCollection &collection)
{
    return collection.itemCount ? i18nc("@item %1 is an album name, %2 how many photos it holds", "%1 (%2)", collection.name, *collection.itemCount)
                                : collection.name;
}
}

QString NextcloudPhotos::removeQuestion(const Nextcloud::PhotoCollection &album, int count)
{
    QString text = i18ncp("@info %2 is an album name", "Remove %1 photo from the album “%2”?", "Remove %1 photos from the album “%2”?", count, album.name);
    text += QLatin1Char('\n')
        + i18nc("@info", "The files are not deleted: they stay in your Nextcloud files and in other albums, and only this album no longer lists them.");
    if (isShared(album)) {
        text += QLatin1Char('\n') + i18nc("@info", "Everyone the album is shared with will no longer see them in it.");
    }
    return text;
}

QString NextcloudPhotos::addQuestion(const Nextcloud::PhotoCollection &album, int count)
{
    QString text = i18ncp("@info %2 is an album name", "Add %1 file to the album “%2”?", "Add %1 files to the album “%2”?", count, album.name);
    text += QLatin1Char('\n') + i18nc("@info", "The files stay where they are; the album only lists them.");
    if (!album.collaborators.isEmpty()) {
        text += QLatin1Char('\n')
            + i18nc("@info %1 is a list of people",
                    "Everyone the album is shared with (%1) will be able to see and download them.",
                    album.collaborators.join(QStringLiteral(", ")));
    } else if (isShared(album)) {
        text += QLatin1Char('\n') + i18nc("@info", "Everyone the album is shared with will be able to see and download them.");
    }
    return text;
}

NextcloudPhotosDialog::NextcloudPhotosDialog(const NextcloudAccounts::Account &account, Nextcloud::CredentialStore *store, QWidget *parent)
    : QDialog(parent)
    , m_account(account)
    , m_client(new NextcloudClient(this))
    , m_confirm(askConfirmation)
    , m_prompt(askName)
    , m_kind(new QComboBox(this))
    , m_reload(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action:button", "Reload"), this))
    , m_newAlbum(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "New Album…"), this))
    , m_collections(new QListWidget(this))
    , m_sort(new QComboBox(this))
    , m_group(new QComboBox(this))
    , m_photos(new QTreeWidget(this))
    , m_preview(new QLabel(this))
    , m_status(new QLabel(this))
    , m_show(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:button", "Show in Pane"), this))
    , m_remove(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove from Album…"), this))
{
    setWindowTitle(i18nc("@title:window %1 is an account name", "Nextcloud Photos — %1", NextcloudAccounts::title(account)));
    m_client->setCredentialStore(store);
    resize(900, 560);

    auto *intro = new QLabel(i18nc("@info",
                                   "Albums, shared albums and places of the Nextcloud Photos app. Photos are sorted and grouped by the capture date "
                                   "and place the server read from them, without downloading the files. Removing a photo from an album never "
                                   "deletes the file."),
                             this);
    intro->setWordWrap(true);

    m_kind->setObjectName(QStringLiteral("photosKind"));
    m_kind->addItem(SymbolicIcon::fromTheme(QStringLiteral("folder-pictures")),
                    i18nc("@item:inlistbox", "Albums"),
                    int(Nextcloud::PhotoCollectionKind::Albums));
    m_kind->addItem(SymbolicIcon::fromTheme(QStringLiteral("folder-publicshare")),
                    i18nc("@item:inlistbox", "Shared albums"),
                    int(Nextcloud::PhotoCollectionKind::SharedAlbums));
    m_kind->addItem(SymbolicIcon::fromTheme(QStringLiteral("mark-location")), i18nc("@item:inlistbox", "Places"), int(Nextcloud::PhotoCollectionKind::Places));
    m_reload->setObjectName(QStringLiteral("photosReload"));
    m_newAlbum->setObjectName(QStringLiteral("photosNewAlbum"));
    m_collections->setObjectName(QStringLiteral("photosCollections"));

    m_sort->setObjectName(QStringLiteral("photosSort"));
    m_sort->addItem(i18nc("@item:inlistbox sort order", "Newest first"), int(Nextcloud::PhotoSort::NewestFirst));
    m_sort->addItem(i18nc("@item:inlistbox sort order", "Oldest first"), int(Nextcloud::PhotoSort::OldestFirst));
    m_sort->addItem(i18nc("@item:inlistbox sort order", "Name"), int(Nextcloud::PhotoSort::Name));
    m_sort->addItem(i18nc("@item:inlistbox sort order", "Place"), int(Nextcloud::PhotoSort::Place));
    m_group->setObjectName(QStringLiteral("photosGroup"));
    m_group->addItem(i18nc("@item:inlistbox grouping", "No groups"), int(Nextcloud::PhotoGrouping::None));
    m_group->addItem(i18nc("@item:inlistbox grouping", "Month taken"), int(Nextcloud::PhotoGrouping::Month));
    m_group->addItem(i18nc("@item:inlistbox grouping", "Place"), int(Nextcloud::PhotoGrouping::Place));

    m_photos->setObjectName(QStringLiteral("photosList"));
    m_photos->setHeaderLabels({i18nc("@title:column", "Name"), i18nc("@title:column", "Taken"), i18nc("@title:column", "Place")});
    m_photos->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_photos->setUniformRowHeights(true);
    m_photos->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_photos->header()->setStretchLastSection(false);
    m_preview->setObjectName(QStringLiteral("photosPreview"));
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFixedWidth(PreviewSize);
    m_preview->setWordWrap(true);
    m_status->setObjectName(QStringLiteral("photosStatus"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_show->setObjectName(QStringLiteral("photosShow"));
    m_remove->setObjectName(QStringLiteral("photosRemove"));

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(i18nc("@label:listbox", "Show:"), this));
    top->addWidget(m_kind, 1);
    top->addWidget(m_reload);
    top->addWidget(m_newAlbum);

    auto *arrange = new QHBoxLayout;
    arrange->addWidget(new QLabel(i18nc("@label:listbox", "Sort:"), this));
    arrange->addWidget(m_sort);
    arrange->addWidget(new QLabel(i18nc("@label:listbox", "Group by:"), this));
    arrange->addWidget(m_group);
    arrange->addStretch();

    auto *photosPane = new QWidget(this);
    auto *photosLayout = new QVBoxLayout(photosPane);
    photosLayout->setContentsMargins(0, 0, 0, 0);
    photosLayout->addLayout(arrange);
    auto *photosRow = new QHBoxLayout;
    photosRow->addWidget(m_photos, 1);
    photosRow->addWidget(m_preview);
    photosLayout->addLayout(photosRow, 1);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(m_collections);
    splitter->addWidget(photosPane);
    splitter->setStretchFactor(1, 3);

    auto *actions = new QHBoxLayout;
    actions->addWidget(m_show);
    actions->addWidget(m_remove);
    actions->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(top);
    layout->addWidget(splitter, 1);
    layout->addWidget(m_status);
    layout->addLayout(actions);
    layout->addWidget(buttons);

    const KConfigGroup state = stateGroup();
    m_kind->setCurrentIndex(std::max(0, m_kind->findData(state.readEntry("Kind", int(Nextcloud::PhotoCollectionKind::Albums)))));
    m_sort->setCurrentIndex(std::max(0, m_sort->findData(state.readEntry("Sort", int(Nextcloud::PhotoSort::NewestFirst)))));
    m_group->setCurrentIndex(std::max(0, m_group->findData(state.readEntry("Grouping", int(Nextcloud::PhotoGrouping::Month)))));

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_kind, &QComboBox::currentIndexChanged, this, [this] {
        stateGroup().writeEntry("Kind", m_kind->currentData().toInt());
        reload();
    });
    connect(m_sort, &QComboBox::currentIndexChanged, this, [this] {
        stateGroup().writeEntry("Sort", m_sort->currentData().toInt());
        fillPhotos();
    });
    connect(m_group, &QComboBox::currentIndexChanged, this, [this] {
        stateGroup().writeEntry("Grouping", m_group->currentData().toInt());
        fillPhotos();
    });
    connect(m_reload, &QPushButton::clicked, this, &NextcloudPhotosDialog::reload);
    connect(m_newAlbum, &QPushButton::clicked, this, &NextcloudPhotosDialog::newAlbum);
    connect(m_collections, &QListWidget::currentRowChanged, this, &NextcloudPhotosDialog::collectionChanged);
    connect(m_photos, &QTreeWidget::itemSelectionChanged, this, [this] {
        updateState();
        previewSelected();
    });
    connect(m_photos, &QTreeWidget::itemDoubleClicked, this, &NextcloudPhotosDialog::showSelected);
    connect(m_show, &QPushButton::clicked, this, &NextcloudPhotosDialog::showSelected);
    connect(m_remove, &QPushButton::clicked, this, &NextcloudPhotosDialog::removeSelected);
    connect(m_client, &NextcloudClient::photoCollectionsListed, this, &NextcloudPhotosDialog::collectionsListed);
    connect(m_client, &NextcloudClient::photosListed, this, &NextcloudPhotosDialog::photosListed);
    connect(m_client, &NextcloudClient::previewFetched, this, [this](const QUrl &item, const QImage &image, const QString &error) {
        if (item != m_previewKey) {
            return;
        }
        m_preview->setPixmap(image.isNull() ? QPixmap()
                                            : QPixmap::fromImage(image).scaled(PreviewSize, PreviewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        if (image.isNull()) {
            m_preview->setText(error);
        }
    });
    connect(m_client, &NextcloudClient::photoRemoved, this, [this](const QString &entryPath, const QString &error) {
        if (!m_removing.removeOne(entryPath)) {
            return;
        }
        if (!error.isEmpty() && !m_removeErrors.contains(error)) {
            m_removeErrors.append(error);
        }
        if (!m_removing.isEmpty()) {
            return;
        }
        m_message = m_removeErrors.isEmpty()
            ? i18nc("@info:status", "Removed from the album. The files were not deleted.")
            : i18nc("@info:status %1 is a list of problems", "Some photos couldn't be removed from the album: %1", m_removeErrors.join(QLatin1Char(' ')));
        m_removeErrors.clear();
        collectionChanged();
    });
    connect(m_client, &NextcloudClient::albumCreated, this, [this](const QString &, const QString &name, const QString &error) {
        if (!error.isEmpty()) {
            m_message = error;
            updateState();
            return;
        }
        m_message = i18nc("@info:status %1 is an album name", "Created the album “%1”.", name);
        m_selectAfterReload = name;
        if (kind() != Nextcloud::PhotoCollectionKind::Albums) {
            setKind(Nextcloud::PhotoCollectionKind::Albums);
        } else {
            reload();
        }
    });

    reload();
}

NextcloudPhotosDialog::~NextcloudPhotosDialog() = default;

NextcloudClient *NextcloudPhotosDialog::client() const
{
    return m_client;
}

void NextcloudPhotosDialog::setConfirmation(const NextcloudPhotos::Confirmation &confirmation)
{
    m_confirm = confirmation;
}

void NextcloudPhotosDialog::setNamePrompt(const NextcloudPhotos::NamePrompt &prompt)
{
    m_prompt = prompt;
}

void NextcloudPhotosDialog::setKind(Nextcloud::PhotoCollectionKind kind)
{
    m_kind->setCurrentIndex(std::max(0, m_kind->findData(int(kind))));
}

Nextcloud::PhotoCollectionKind NextcloudPhotosDialog::kind() const
{
    return Nextcloud::PhotoCollectionKind(m_kind->currentData().toInt());
}

void NextcloudPhotosDialog::reload()
{
    m_collectionList.clear();
    {
        const QSignalBlocker blocker(m_collections);
        m_collections->clear();
    }
    m_photoList.clear();
    m_photos->clear();
    m_photosRequest = 0;
    m_listingError.clear();
    m_collectionsRequest = m_client->listPhotoCollections(m_account, kind());
    updateState();
}

void NextcloudPhotosDialog::collectionsListed(int request, const Nextcloud::PhotoCollectionListing &listing)
{
    if (request != m_collectionsRequest) {
        return;
    }
    m_collectionsRequest = 0;
    m_collectionList = listing.collections;
    m_listingError = listing.error;
    m_photosUnavailable = listing.unavailable && kind() != Nextcloud::PhotoCollectionKind::Places;
    int select = 0;
    {
        const QSignalBlocker blocker(m_collections);
        for (int i = 0; i < m_collectionList.size(); ++i) {
            const Nextcloud::PhotoCollection &collection = m_collectionList.at(i);
            auto *item = new QListWidgetItem(collectionText(collection), m_collections);
            item->setToolTip(collectionToolTip(collection));
            if (collection.name == m_selectAfterReload) {
                select = i;
            }
        }
    }
    m_selectAfterReload.clear();
    if (!m_collectionList.isEmpty()) {
        m_collections->setCurrentRow(select);
    }
    updateState();
}

void NextcloudPhotosDialog::collectionChanged()
{
    m_photoList.clear();
    m_photos->clear();
    const std::optional<Nextcloud::PhotoCollection> collection = currentCollection();
    m_photosRequest = collection ? m_client->listPhotos(m_account, *collection) : 0;
    updateState();
}

void NextcloudPhotosDialog::photosListed(int request, const Nextcloud::PhotoListing &listing)
{
    if (request != m_photosRequest) {
        return;
    }
    m_photosRequest = 0;
    m_photoList = listing.photos;
    m_listingError = listing.error;
    fillPhotos();
}

void NextcloudPhotosDialog::fillPhotos()
{
    m_photos->clear();
    const auto sort = Nextcloud::PhotoSort(m_sort->currentData().toInt());
    const auto grouping = Nextcloud::PhotoGrouping(m_group->currentData().toInt());
    // Arrange indexes, so that items can refer to m_photoList.
    QList<Nextcloud::Photo> indexed = m_photoList;
    for (int i = 0; i < indexed.size(); ++i) {
        indexed[i].file.etag = QString::number(i);
    }
    const QList<Nextcloud::PhotoGroup> groups = Nextcloud::arrangePhotos(indexed, sort, grouping);
    const QLocale locale;
    for (const Nextcloud::PhotoGroup &group : groups) {
        QTreeWidgetItem *parent = nullptr;
        if (grouping != Nextcloud::PhotoGrouping::None) {
            parent = new QTreeWidgetItem(m_photos, {i18nc("@item %1 is a month or place, %2 how many photos", "%1 (%2)", group.title, group.photos.size())});
            parent->setFlags(Qt::ItemIsEnabled);
            parent->setFirstColumnSpanned(true);
            QFont font = parent->font(NameColumn);
            font.setBold(true);
            parent->setFont(NameColumn, font);
        }
        for (const Nextcloud::Photo &photo : group.photos) {
            const QString taken = photo.taken.isValid() ? locale.toString(photo.taken.toLocalTime(), QLocale::ShortFormat)
                                                        : i18nc("@item photo without a capture date", "Unknown");
            auto *item =
                parent ? new QTreeWidgetItem(parent, {photo.name, taken, photo.place}) : new QTreeWidgetItem(m_photos, {photo.name, taken, photo.place});
            item->setData(NameColumn, IndexRole, photo.file.etag.toInt());
            if (!photo.taken.isValid() && photo.file.modified.isValid()) {
                item->setToolTip(TakenColumn,
                                 i18nc("@info:tooltip %1 is a date",
                                       "The server has no capture date for this photo. It was last modified on %1.",
                                       locale.toString(photo.file.modified.toLocalTime(), QLocale::ShortFormat)));
            }
            if (photo.gps) {
                item->setToolTip(PlaceColumn,
                                 i18nc("@info:tooltip %1 and %2 are coordinates",
                                       "Latitude %1, longitude %2",
                                       locale.toString(photo.gps->x(), 'f', 5),
                                       locale.toString(photo.gps->y(), 'f', 5)));
            }
        }
    }
    m_photos->expandAll();
    m_photos->setRootIsDecorated(grouping != Nextcloud::PhotoGrouping::None);
    updateState();
}

QList<Nextcloud::Photo> NextcloudPhotosDialog::shownPhotos() const
{
    QList<Nextcloud::Photo> photos;
    for (QTreeWidgetItemIterator it(m_photos); *it; ++it) {
        const QVariant index = (*it)->data(NameColumn, IndexRole);
        if (index.isValid()) {
            photos.append(m_photoList.value(index.toInt()));
        }
    }
    return photos;
}

QList<Nextcloud::Photo> NextcloudPhotosDialog::selectedPhotos() const
{
    QList<Nextcloud::Photo> photos;
    for (QTreeWidgetItemIterator it(m_photos, QTreeWidgetItemIterator::Selected); *it; ++it) {
        const QVariant index = (*it)->data(NameColumn, IndexRole);
        if (index.isValid()) {
            photos.append(m_photoList.value(index.toInt()));
        }
    }
    return photos;
}

std::optional<Nextcloud::PhotoCollection> NextcloudPhotosDialog::currentCollection() const
{
    const int row = m_collections->currentRow();
    if (row < 0 || row >= m_collectionList.size()) {
        return std::nullopt;
    }
    return m_collectionList.at(row);
}

void NextcloudPhotosDialog::showSelected()
{
    const QList<Nextcloud::Photo> photos = selectedPhotos();
    if (photos.isEmpty()) {
        return;
    }
    const QUrl url = Nextcloud::photoFileUrl(m_account, photos.first());
    if (url.isEmpty()) {
        m_message = i18nc("@info:status", "This photo is in someone else's files, so it can't be shown in a pane.");
        updateState();
        return;
    }
    Q_EMIT showRequested(url);
}

void NextcloudPhotosDialog::removeSelected()
{
    const std::optional<Nextcloud::PhotoCollection> album = currentCollection();
    const QList<Nextcloud::Photo> photos = selectedPhotos();
    if (!album || photos.isEmpty() || !Nextcloud::canRemovePhotos(*album) || !m_removing.isEmpty()) {
        return;
    }
    if (!m_confirm(this,
                   i18nc("@title:window", "Remove from Album"),
                   NextcloudPhotos::removeQuestion(*album, photos.size()),
                   i18nc("@action:button", "Remove from Album"))) {
        return;
    }
    m_removeErrors.clear();
    for (const Nextcloud::Photo &photo : photos) {
        m_removing.append(photo.file.path);
    }
    for (const Nextcloud::Photo &photo : photos) {
        m_client->removeFromAlbum(m_account, photo);
    }
    updateState();
}

void NextcloudPhotosDialog::newAlbum()
{
    const std::optional<QString> name = m_prompt(this);
    if (!name) {
        return;
    }
    if (const QString error = Nextcloud::albumNameError(*name); !error.isEmpty()) {
        m_message = error;
        updateState();
        return;
    }
    m_client->createAlbum(m_account, *name);
}

void NextcloudPhotosDialog::previewSelected()
{
    const QList<Nextcloud::Photo> photos = selectedPhotos();
    m_preview->setPixmap({});
    m_preview->clear();
    m_previewKey.clear();
    if (photos.size() != 1) {
        return;
    }
    const Nextcloud::Photo &photo = photos.first();
    if (photo.file.fileId.isEmpty() || photo.file.hasPreview == false) {
        m_preview->setText(i18nc("@info", "The server has no preview of this photo."));
        return;
    }
    m_previewKey = QUrl::fromLocalFile(photo.file.path);
    m_preview->setText(i18nc("@info:status", "Loading preview…"));
    m_client->fetchPreview(m_account, photo.file.fileId, QSize(PreviewSize, PreviewSize), m_previewKey);
}

void NextcloudPhotosDialog::updateState()
{
    const bool busy = m_collectionsRequest != 0 || m_photosRequest != 0;
    const std::optional<Nextcloud::PhotoCollection> collection = currentCollection();
    const QList<Nextcloud::Photo> photos = selectedPhotos();

    QStringList status;
    if (busy) {
        status.append(i18nc("@info:status", "Loading…"));
    } else if (!m_listingError.isEmpty()) {
        status.append(m_listingError);
    } else if (m_collectionList.isEmpty()) {
        switch (kind()) {
        case Nextcloud::PhotoCollectionKind::Albums:
            status.append(i18nc("@info:status", "You have no albums yet. Create one with New Album."));
            break;
        case Nextcloud::PhotoCollectionKind::SharedAlbums:
            status.append(i18nc("@info:status", "No albums are shared with you."));
            break;
        case Nextcloud::PhotoCollectionKind::Places:
            status.append(i18nc("@info:status", "The server hasn't grouped any photos by place."));
            break;
        }
    } else if (collection) {
        status.append(i18ncp("@info:status", "%1 photo.", "%1 photos.", m_photoList.size()));
        if (const int undated = Nextcloud::photosWithoutCaptureDate(m_photoList); undated > 0) {
            status.append(i18ncp("@info:status",
                                 "The server has no capture date for %1 photo; it is listed last.",
                                 "The server has no capture date for %1 photos; they are listed last.",
                                 undated));
        }
    }
    if (!m_message.isEmpty()) {
        status.prepend(m_message);
    }
    m_status->setText(status.join(QLatin1Char(' ')));

    m_newAlbum->setEnabled(!m_photosUnavailable && m_collectionsRequest == 0);
    m_newAlbum->setToolTip(m_photosUnavailable ? m_listingError : i18nc("@info:tooltip", "Create an album on the server"));

    const bool inOwnFiles = !photos.isEmpty() && !Nextcloud::photoFileUrl(m_account, photos.first()).isEmpty();
    m_show->setEnabled(inOwnFiles);
    m_show->setToolTip(photos.isEmpty() || inOwnFiles ? i18nc("@info:tooltip", "Show the photo's file in the active pane")
                                                      : i18nc("@info:tooltip", "This photo is in someone else's files."));

    QString removeReason;
    if (!collection || collection->kind == Nextcloud::PhotoCollectionKind::Places) {
        removeReason = i18nc("@info:tooltip", "Places are grouped by the server; photos can only be removed from albums.");
    } else if (!Nextcloud::canRemovePhotos(*collection)) {
        removeReason = i18nc("@info:tooltip", "The album's owner doesn't allow you to remove photos.");
    }
    m_remove->setEnabled(removeReason.isEmpty() && !photos.isEmpty() && m_removing.isEmpty());
    m_remove->setToolTip(removeReason.isEmpty() ? i18nc("@info:tooltip", "Remove from this album; the files themselves are not deleted") : removeReason);
}

NextcloudAddToAlbumDialog::NextcloudAddToAlbumDialog(const NextcloudAccounts::Account &account,
                                                     const QList<QUrl> &files,
                                                     Nextcloud::CredentialStore *store,
                                                     QWidget *parent)
    : QDialog(parent)
    , m_account(account)
    , m_files(files)
    , m_client(new NextcloudClient(this))
    , m_confirm(askConfirmation)
    , m_prompt(askName)
    , m_albums(new QListWidget(this))
    , m_status(new QLabel(this))
    , m_add(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Add to Album"), this))
    , m_newAlbum(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("folder-new")), i18nc("@action:button", "New Album…"), this))
{
    setWindowTitle(i18nc("@title:window", "Add to Nextcloud Album"));
    m_client->setCredentialStore(store);
    resize(420, 400);

    auto *intro = new QLabel(i18ncp("@info",
                                    "Choose the album to list %1 file in. The file stays where it is; the album only lists it.",
                                    "Choose the album to list %1 files in. The files stay where they are; the album only lists them.",
                                    files.size()),
                             this);
    intro->setWordWrap(true);
    m_albums->setObjectName(QStringLiteral("addToAlbumList"));
    m_status->setObjectName(QStringLiteral("addToAlbumStatus"));
    m_status->setWordWrap(true);
    m_add->setObjectName(QStringLiteral("addToAlbumAdd"));
    m_add->setDefault(true);
    m_newAlbum->setObjectName(QStringLiteral("addToAlbumNew"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->addButton(m_newAlbum, QDialogButtonBox::ActionRole);
    buttons->addButton(m_add, QDialogButtonBox::ActionRole);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_albums, 1);
    layout->addWidget(m_status);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_add, &QPushButton::clicked, this, &NextcloudAddToAlbumDialog::addToChosen);
    connect(m_newAlbum, &QPushButton::clicked, this, &NextcloudAddToAlbumDialog::newAlbum);
    connect(m_albums, &QListWidget::currentRowChanged, this, &NextcloudAddToAlbumDialog::updateState);
    connect(m_albums, &QListWidget::itemDoubleClicked, this, &NextcloudAddToAlbumDialog::addToChosen);
    connect(m_client, &NextcloudClient::photoCollectionsListed, this, &NextcloudAddToAlbumDialog::collectionsListed);
    connect(m_client, &NextcloudClient::albumCreated, this, [this](const QString &, const QString &name, const QString &error) {
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        m_selectAfterReload = name;
        reload();
    });
    connect(m_client, &NextcloudClient::photoAdded, this, [this](const QUrl &file, const QString &albumPath, const QString &error) {
        if (m_pending == 0 || albumPath != m_pendingAlbum || !m_files.contains(file)) {
            return;
        }
        if (error.isEmpty()) {
            ++m_added;
        } else {
            m_errors.append(i18nc("@info %1 is a file name, %2 a problem", "%1: %2", file.fileName(), error));
        }
        if (--m_pending > 0) {
            return;
        }
        QString name;
        for (const Nextcloud::PhotoCollection &album : std::as_const(m_albumList)) {
            if (album.path == m_pendingAlbum) {
                name = album.name;
            }
        }
        Q_EMIT finished(name, m_added, m_errors);
        accept();
    });

    reload();
}

NextcloudAddToAlbumDialog::~NextcloudAddToAlbumDialog() = default;

NextcloudClient *NextcloudAddToAlbumDialog::client() const
{
    return m_client;
}

void NextcloudAddToAlbumDialog::setConfirmation(const NextcloudPhotos::Confirmation &confirmation)
{
    m_confirm = confirmation;
}

void NextcloudAddToAlbumDialog::setNamePrompt(const NextcloudPhotos::NamePrompt &prompt)
{
    m_prompt = prompt;
}

void NextcloudAddToAlbumDialog::reload()
{
    m_albumList.clear();
    m_listingErrors.clear();
    m_albums->clear();
    m_requests.clear();
    for (const auto kind : {Nextcloud::PhotoCollectionKind::Albums, Nextcloud::PhotoCollectionKind::SharedAlbums}) {
        m_requests.insert(m_client->listPhotoCollections(m_account, kind), kind);
    }
    updateState();
}

void NextcloudAddToAlbumDialog::collectionsListed(int request, const Nextcloud::PhotoCollectionListing &listing)
{
    if (!m_requests.remove(request)) {
        return;
    }
    if (!listing.error.isEmpty() && !m_listingErrors.contains(listing.error)) {
        m_listingErrors.append(listing.error);
    }
    if (listing.kind == Nextcloud::PhotoCollectionKind::Albums) {
        m_photosUnavailable = listing.unavailable;
    }
    for (const Nextcloud::PhotoCollection &album : listing.collections) {
        if (Nextcloud::canAddPhotos(album)) {
            m_albumList.append(album);
        }
    }
    if (!m_requests.isEmpty()) {
        updateState();
        return;
    }
    // Own albums first, then those shared with the user.
    std::stable_sort(m_albumList.begin(), m_albumList.end(), [](const Nextcloud::PhotoCollection &a, const Nextcloud::PhotoCollection &b) {
        return a.kind == Nextcloud::PhotoCollectionKind::Albums && b.kind != Nextcloud::PhotoCollectionKind::Albums;
    });
    int select = 0;
    for (int i = 0; i < m_albumList.size(); ++i) {
        const Nextcloud::PhotoCollection &album = m_albumList.at(i);
        auto *item = new QListWidgetItem(SymbolicIcon::fromTheme(isShared(album) ? QStringLiteral("folder-publicshare") : QStringLiteral("folder-pictures")),
                                         collectionText(album),
                                         m_albums);
        item->setToolTip(collectionToolTip(album));
        if (album.name == m_selectAfterReload) {
            select = i;
        }
    }
    m_selectAfterReload.clear();
    if (!m_albumList.isEmpty()) {
        m_albums->setCurrentRow(select);
    }
    updateState();
}

void NextcloudAddToAlbumDialog::newAlbum()
{
    const std::optional<QString> name = m_prompt(this);
    if (!name) {
        return;
    }
    if (const QString error = Nextcloud::albumNameError(*name); !error.isEmpty()) {
        m_status->setText(error);
        return;
    }
    m_client->createAlbum(m_account, *name);
}

void NextcloudAddToAlbumDialog::addToChosen()
{
    const int row = m_albums->currentRow();
    if (row < 0 || row >= m_albumList.size() || m_pending > 0 || m_files.isEmpty()) {
        return;
    }
    const Nextcloud::PhotoCollection album = m_albumList.at(row);
    if (!m_confirm(this,
                   i18nc("@title:window", "Add to Album"),
                   NextcloudPhotos::addQuestion(album, m_files.size()),
                   i18nc("@action:button", "Add to Album"))) {
        return;
    }
    m_pendingAlbum = album.path;
    m_pending = m_files.size();
    m_added = 0;
    m_errors.clear();
    m_status->setText(i18nc("@info:status", "Adding…"));
    updateState();
    const QList<QUrl> files = m_files;
    for (const QUrl &file : files) {
        m_client->addToAlbum(m_account, file, album.path);
    }
}

void NextcloudAddToAlbumDialog::updateState()
{
    const bool busy = !m_requests.isEmpty() || m_pending > 0;
    m_add->setEnabled(!busy && m_albums->currentRow() >= 0);
    m_newAlbum->setEnabled(!busy && !m_photosUnavailable);
    m_albums->setEnabled(m_pending == 0);
    if (m_pending > 0) {
        return;
    }
    if (!m_requests.isEmpty()) {
        m_status->setText(i18nc("@info:status", "Loading albums…"));
    } else if (!m_listingErrors.isEmpty()) {
        m_status->setText(m_listingErrors.join(QLatin1Char(' ')));
    } else if (m_albumList.isEmpty()) {
        m_status->setText(i18nc("@info:status", "You have no albums that accept photos. Create one with New Album."));
    } else {
        m_status->clear();
    }
}
