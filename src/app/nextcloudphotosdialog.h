// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudclient.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QUrl>

#include <functional>
#include <optional>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace NextcloudPhotos
{
/// Asks the user to confirm @p text; returns whether they did. The default asks with a message box.
using Confirmation = std::function<bool(QWidget *parent, const QString &title, const QString &text, const QString &action)>;
/// Asks the user for a new album's name; nullopt when they cancel. The default asks with an input dialog.
using NamePrompt = std::function<std::optional<QString>(QWidget *parent)>;

/// Choices from Preferences › Remote and Cloud › Nextcloud › Photos. The defaults keep every kind and confirmation.
struct Options {
    bool albums = true;
    bool sharedAlbums = true;
    bool places = true;
    /// The grouping the Photos window opens with; nullopt for the one last chosen in the window.
    std::optional<Nextcloud::PhotoGrouping> grouping;
    /// Whether changes to albums that aren't shared are confirmed first. Changes to shared albums always are.
    bool confirmChanges = true;

    /// The kinds shown, in the order shown; the user's albums when none are.
    QList<Nextcloud::PhotoCollectionKind> kinds() const;
};

/// The question to ask before removing @p count photos from @p album.
QString removeQuestion(const Nextcloud::PhotoCollection &album, int count);
/// The question to ask before adding @p count files to @p album.
QString addQuestion(const Nextcloud::PhotoCollection &album, int count);
}

/**
 * Browses the albums, shared albums and places of the Nextcloud Photos app as
 * virtual folders: the photos are listed with the capture date and place the
 * server read from them, and sorted and grouped by them, without downloading
 * any file. Photos can be removed from albums, which leaves the files in place.
 */
class NextcloudPhotosDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param store where the account's app password is kept; not owned
    NextcloudPhotosDialog(const NextcloudAccounts::Account &account,
                          Nextcloud::CredentialStore *store,
                          const NextcloudPhotos::Options &options = {},
                          QWidget *parent = nullptr);
    ~NextcloudPhotosDialog() override;

    NextcloudClient *client() const;
    void setConfirmation(const NextcloudPhotos::Confirmation &confirmation);
    void setNamePrompt(const NextcloudPhotos::NamePrompt &prompt);
    void setKind(Nextcloud::PhotoCollectionKind kind);
    Nextcloud::PhotoCollectionKind kind() const;
    /// Lists the collections of the current kind again.
    void reload();
    /// The photos shown, in the order shown.
    QList<Nextcloud::Photo> shownPhotos() const;

Q_SIGNALS:
    /// The user asked to show the file @p url in the active pane.
    void showRequested(const QUrl &url);

private:
    void collectionsListed(int request, const Nextcloud::PhotoCollectionListing &listing);
    void photosListed(int request, const Nextcloud::PhotoListing &listing);
    void collectionChanged();
    void fillPhotos();
    void showSelected();
    void removeSelected();
    void newAlbum();
    void previewSelected();
    std::optional<Nextcloud::PhotoCollection> currentCollection() const;
    QList<Nextcloud::Photo> selectedPhotos() const;
    void updateState();

    NextcloudAccounts::Account m_account;
    NextcloudPhotos::Options m_options;
    NextcloudClient *m_client;
    NextcloudPhotos::Confirmation m_confirm;
    NextcloudPhotos::NamePrompt m_prompt;
    QComboBox *m_kind;
    QPushButton *m_reload;
    QPushButton *m_newAlbum;
    QListWidget *m_collections;
    QComboBox *m_sort;
    QComboBox *m_group;
    QTreeWidget *m_photos;
    QLabel *m_preview;
    QLabel *m_status;
    QPushButton *m_show;
    QPushButton *m_remove;
    QList<Nextcloud::PhotoCollection> m_collectionList;
    QList<Nextcloud::Photo> m_photoList;
    int m_collectionsRequest = 0;
    int m_photosRequest = 0;
    /// Album entries being removed.
    QStringList m_removing;
    QStringList m_removeErrors;
    QUrl m_previewKey;
    QString m_message;
    QString m_listingError;
    bool m_photosUnavailable = false;
    QString m_selectAfterReload;
};

/**
 * Chooses an album, or shared album that accepts photos, to add files from
 * the user's Nextcloud files to, and adds them after the user confirms.
 */
class NextcloudAddToAlbumDialog : public QDialog
{
    Q_OBJECT

public:
    /// Lists only the kinds of album that @p options show.
    NextcloudAddToAlbumDialog(const NextcloudAccounts::Account &account,
                              const QList<QUrl> &files,
                              Nextcloud::CredentialStore *store,
                              const NextcloudPhotos::Options &options = {},
                              QWidget *parent = nullptr);
    ~NextcloudAddToAlbumDialog() override;

    NextcloudClient *client() const;
    void setConfirmation(const NextcloudPhotos::Confirmation &confirmation);
    void setNamePrompt(const NextcloudPhotos::NamePrompt &prompt);
    /// Asks to confirm, then adds the files to the chosen album.
    void addToChosen();

Q_SIGNALS:
    /// The files were added to @p album; @p errors lists those that couldn't be.
    void finished(const QString &album, int added, const QStringList &errors);

private:
    void reload();
    void collectionsListed(int request, const Nextcloud::PhotoCollectionListing &listing);
    void newAlbum();
    void updateState();

    NextcloudAccounts::Account m_account;
    NextcloudPhotos::Options m_options;
    QList<QUrl> m_files;
    NextcloudClient *m_client;
    NextcloudPhotos::Confirmation m_confirm;
    NextcloudPhotos::NamePrompt m_prompt;
    QListWidget *m_albums;
    QLabel *m_status;
    QPushButton *m_add;
    QPushButton *m_newAlbum;
    QList<Nextcloud::PhotoCollection> m_albumList;
    QHash<int, Nextcloud::PhotoCollectionKind> m_requests;
    QString m_pendingAlbum;
    int m_pending = 0;
    int m_added = 0;
    QStringList m_errors;
    QStringList m_listingErrors;
    bool m_photosUnavailable = false;
    QString m_selectAfterReload;
};
