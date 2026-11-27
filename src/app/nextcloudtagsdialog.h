// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudclient.h"

#include <QDialog>
#include <QHash>
#include <QSet>

#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

/**
 * Assigns and removes the collaborative tags of a Nextcloud server on files
 * and folders there. These are the server's tags, which other users of the
 * server can see; they are never mixed with Plasma Opus's local tags.
 */
class NextcloudTagsDialog : public QDialog
{
    Q_OBJECT

public:
    /// A file or folder on the server, with its file ID when it is known.
    struct Item {
        QUrl url;
        QString fileId;
    };
    using CreationConfirmation = std::function<bool(const QString &name)>;

    /// @param store where the account's app password is kept; not owned
    NextcloudTagsDialog(const NextcloudAccounts::Account &account, const QList<Item> &items, Nextcloud::CredentialStore *store, QWidget *parent = nullptr);

    /// Replaces the question asked before creating a tag on the server, for tests.
    void setCreationConfirmation(CreationConfirmation confirmation);
    NextcloudClient *client() const;

Q_SIGNALS:
    /// Tags were assigned to or removed from some of @p items.
    void tagsChanged(const QList<QUrl> &items);

private:
    void metadataFetched(const QUrl &url, const Nextcloud::FileMetadata &metadata, const QString &error);
    void systemTagsFetched(const QString &accountId, const QList<Nextcloud::SystemTag> &tags, const QString &error);
    void fileTagsFetched(const QString &fileId, const QList<Nextcloud::SystemTag> &tags, const QString &error);
    void fileTagSet(const QString &fileId, const QString &tagId, bool assigned, const QString &error);
    void tagCreated(const QString &accountId, const Nextcloud::SystemTag &tag, const QString &error);
    void createTag();
    void apply();
    void finishLoading();
    /// Lists the tags, checked by how many of the items have them.
    void rebuildList();
    QListWidgetItem *addTag(const Nextcloud::SystemTag &tag, Qt::CheckState state);
    void updateState();

    NextcloudAccounts::Account m_account;
    QList<Item> m_items;
    NextcloudClient *m_client;
    CreationConfirmation m_confirmCreation;
    QListWidget *m_list;
    QLineEdit *m_newTag;
    QPushButton *m_create;
    QLabel *m_status;
    QPushButton *m_ok;
    QList<Nextcloud::SystemTag> m_tags;
    bool m_tagsLoaded = false;
    /// Whether the server's tags couldn't be listed.
    bool m_tagsError = false;
    /// The items whose file ID is being read.
    int m_idsPending = 0;
    /// The tag IDs of each file ID, once read.
    QHash<QString, QSet<QString>> m_fileTags;
    int m_fileTagsPending = 0;
    QStringList m_loadErrors;
    bool m_loaded = false;
    bool m_creating = false;
    int m_changesPending = 0;
    QStringList m_applyErrors;
    QSet<QString> m_changedFiles;
};
