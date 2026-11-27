// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "nextcloudclient.h"
#include "nextcloudtagsdialog.h"

#include <QDialog>
#include <QHash>
#include <QPointer>

#include <optional>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * Searches a Nextcloud server's files and lists the user's favourites there
 * and the files with one of its collaborative tags, which can then be shown
 * in a pane, marked as favourites or tagged. Everything here is the server's
 * own data: neither its favourites nor its tags are Plasma Opus's.
 */
class NextcloudAssetsDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Search,
        Favorites,
        Tag,
    };
    /// The most results listed at once.
    static constexpr int ResultLimit = 500;

    /// @param store where the account's app password is kept; not owned
    NextcloudAssetsDialog(const NextcloudAccounts::Account &account, Nextcloud::CredentialStore *store, QWidget *parent = nullptr);
    ~NextcloudAssetsDialog() override;

    NextcloudClient *client() const;
    /// Uses @p capabilities instead of asking the server, for tests.
    void setCapabilities(const Nextcloud::Capabilities &capabilities);
    void setMode(Mode mode);
    Mode mode() const;
    /// Lists the results of the current mode.
    void run();
    /// The tags dialog the Nextcloud Tags button opened, if it is still open.
    NextcloudTagsDialog *tagsDialog() const;

Q_SIGNALS:
    /// The user asked to show @p url, a folder when @p folder is true, in the active pane.
    void showRequested(const QUrl &url, bool folder);
    /// The favourite state or tags of @p items changed on the server.
    void itemsChanged(const QList<QUrl> &items);

private:
    void checked(const QString &accountId, const Nextcloud::Status &status);
    void systemTagsFetched(const QString &accountId, const QList<Nextcloud::SystemTag> &tags, const QString &error);
    void assetsListed(int request, const Nextcloud::AssetResults &results);
    void favoriteSet(const QUrl &url, bool favorite, const QString &error);
    void showSelected();
    void toggleFavorite();
    void editTags();
    QList<QTreeWidgetItem *> selectedItems() const;
    QTreeWidgetItem *itemFor(const QUrl &url) const;
    void setFavoriteColumn(QTreeWidgetItem *item, bool favorite);
    void updateModes();
    void updateState();

    NextcloudAccounts::Account m_account;
    Nextcloud::CredentialStore *m_store;
    NextcloudClient *m_client;
    QComboBox *m_mode;
    QFormLayout *m_form;
    QLineEdit *m_text;
    QComboBox *m_type;
    QCheckBox *m_favoritesOnly;
    QComboBox *m_tag;
    QPushButton *m_run;
    QTreeWidget *m_results;
    QLabel *m_status;
    /// Which features the server doesn't offer, and why.
    QLabel *m_notice;
    QPushButton *m_show;
    QPushButton *m_favorite;
    QPushButton *m_tags;
    std::optional<Nextcloud::Capabilities> m_capabilities;
    /// Why the server's tags can't be listed, or an empty string.
    QString m_tagsError;
    int m_request = 0;
    bool m_listing = false;
    /// The favourites being changed, by item.
    QHash<QString, bool> m_favoritesPending;
    QStringList m_favoriteErrors;
    QList<QUrl> m_favoritesChanged;
    QPointer<NextcloudTagsDialog> m_tagsDialog;
};
