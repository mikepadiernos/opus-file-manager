// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QUrl>
#include <QWidget>

class KDirLister;
class KDirModel;
class KDirSortFilterProxyModel;
class KFileItem;
class QLabel;
class QListWidget;
class QTimer;

/**
 * A read-only list of a folder's contents for previews: icons and names,
 * folders first and sorted by name as Yazi mode's parent column sorts them,
 * listed asynchronously with a KDirLister. Only the first EntryLimit entries
 * are shown, with a note that counts the rest. Listing another folder, or
 * clearing, stops the previous listing; a listing that takes longer than
 * ListingTimeout is stopped and keeps the entries it has found.
 */
class FolderPreview : public QWidget
{
    Q_OBJECT

public:
    /// The most entries the list shows.
    static constexpr int EntryLimit = 500;
    /// How long, in milliseconds, a folder may take to list before listing stops.
    static constexpr int ListingTimeout = 10000;

    explicit FolderPreview(QWidget *parent = nullptr);
    ~FolderPreview() override;

    /// Lists the contents of the folder @p item; a null item clears the list.
    void setFolder(const KFileItem &item);
    QUrl url() const;
    /// Stops listing and empties the list.
    void clear();
    void setShowHiddenFiles(bool show);
    bool showHiddenFiles() const;
    /// Whether the folder is still being listed.
    bool isListing() const;
    /// How many entries the folder has so far, including those the list leaves out.
    int entryCount() const;
    QListWidget *list() const;
    /// The note below the list, such as how many entries it leaves out or why listing failed.
    QLabel *note() const;

Q_SIGNALS:
    void listingChanged(bool listing);

private:
    void resetLister();
    void scheduleRefresh();
    void refresh();
    void setListing(bool listing);

    KDirLister *m_lister = nullptr;
    KDirModel *m_model = nullptr;
    KDirSortFilterProxyModel *m_proxy = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_note = nullptr;
    QTimer *m_refresh = nullptr;
    QTimer *m_timeout = nullptr;
    QUrl m_url;
    QString m_error;
    bool m_showHiddenFiles = false;
    bool m_listing = false;
    bool m_timedOut = false;
};
