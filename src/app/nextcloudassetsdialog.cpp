// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudassetsdialog.h"

#include "symbolicicon.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMimeDatabase>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
enum Column {
    NameColumn,
    FolderColumn,
    ModifiedColumn,
    FavoriteColumn,
    TagsColumn,
};
constexpr int UrlRole = Qt::UserRole;
constexpr int FileIdRole = Qt::UserRole + 1;
constexpr int FolderRole = Qt::UserRole + 2;
constexpr int FavoriteRole = Qt::UserRole + 3;

QString itemKey(const QUrl &url)
{
    return url.adjusted(QUrl::RemoveUserInfo | QUrl::StripTrailingSlash).toString();
}

QString itemName(const QUrl &url)
{
    const QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
    return name.isEmpty() ? url.toDisplayString(QUrl::RemoveUserInfo) : name;
}

KConfigGroup stateGroup()
{
    return KSharedConfig::openStateConfig()->group(QStringLiteral("NextcloudAssetsDialog"));
}

void setComboItemEnabled(QComboBox *combo, int index, bool enabled, const QString &toolTip)
{
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());
    if (QStandardItem *item = model ? model->item(index) : nullptr) {
        item->setEnabled(enabled);
        item->setToolTip(enabled ? QString() : toolTip);
    }
}

bool comboItemEnabled(const QComboBox *combo, int index)
{
    const auto *model = qobject_cast<const QStandardItemModel *>(combo->model());
    const QStandardItem *item = model ? model->item(index) : nullptr;
    return !item || item->isEnabled();
}
}

NextcloudAssetsDialog::NextcloudAssetsDialog(const NextcloudAccounts::Account &account, Nextcloud::CredentialStore *store, QWidget *parent)
    : QDialog(parent)
    , m_account(account)
    , m_store(store)
    , m_client(new NextcloudClient(this))
    , m_mode(new QComboBox(this))
    , m_form(new QFormLayout)
    , m_text(new QLineEdit(this))
    , m_type(new QComboBox(this))
    , m_favoritesOnly(new QCheckBox(i18nc("@option:check", "Only my favourites"), this))
    , m_tag(new QComboBox(this))
    , m_run(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("system-search")), i18nc("@action:button", "Search"), this))
    , m_results(new QTreeWidget(this))
    , m_status(new QLabel(this))
    , m_notice(new QLabel(this))
    , m_show(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:button", "Show in Pane"), this))
    , m_favorite(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("starred")), i18nc("@action:button", "Add to Favourites"), this))
    , m_tags(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("tag")), i18nc("@action:button", "Nextcloud Tags…"), this))
{
    setWindowTitle(i18nc("@title:window", "Search and Organize"));
    m_client->setCredentialStore(store);
    resize(760, 520);

    auto *intro = new QLabel(i18nc("@info %1 is an account name",
                                   "Search the files of %1 on the Nextcloud server, and list your favourites and the files with a collaborative "
                                   "tag there. The server's favourites and tags are separate from the Places panel and from Plasma Opus tags.",
                                   NextcloudAccounts::title(m_account).toHtmlEscaped()),
                             this);
    intro->setWordWrap(true);

    m_mode->setObjectName(QStringLiteral("nextcloudAssetsMode"));
    m_mode->addItem(SymbolicIcon::fromTheme(QStringLiteral("system-search")), i18nc("@item:inlistbox", "Search"), int(Mode::Search));
    m_mode->addItem(SymbolicIcon::fromTheme(QStringLiteral("starred")), i18nc("@item:inlistbox", "Favourites"), int(Mode::Favorites));
    m_mode->addItem(SymbolicIcon::fromTheme(QStringLiteral("tag")), i18nc("@item:inlistbox", "Nextcloud tag (server)"), int(Mode::Tag));
    m_text->setObjectName(QStringLiteral("nextcloudAssetsText"));
    m_text->setPlaceholderText(i18nc("@info:placeholder", "Any name"));
    m_text->setClearButtonEnabled(true);
    m_type->setObjectName(QStringLiteral("nextcloudAssetsType"));
    m_type->addItem(i18nc("@item:inlistbox kind of file", "Any kind"), int(Nextcloud::AssetType::Any));
    m_type->addItem(i18nc("@item:inlistbox kind of file", "Images"), int(Nextcloud::AssetType::Images));
    m_type->addItem(i18nc("@item:inlistbox kind of file", "Videos"), int(Nextcloud::AssetType::Videos));
    m_type->addItem(i18nc("@item:inlistbox kind of file", "Audio"), int(Nextcloud::AssetType::Audio));
    m_type->addItem(i18nc("@item:inlistbox kind of file", "Documents"), int(Nextcloud::AssetType::Documents));
    m_type->addItem(i18nc("@item:inlistbox kind of file", "Folders"), int(Nextcloud::AssetType::Folders));
    m_favoritesOnly->setObjectName(QStringLiteral("nextcloudAssetsFavoritesOnly"));
    m_tag->setObjectName(QStringLiteral("nextcloudAssetsTag"));
    m_tag->setToolTip(i18nc("@info:tooltip", "A collaborative tag of the Nextcloud server, not a Plasma Opus tag"));
    m_run->setObjectName(QStringLiteral("nextcloudAssetsRun"));
    m_run->setDefault(true);

    m_results->setObjectName(QStringLiteral("nextcloudAssetsResults"));
    m_results->setHeaderLabels({i18nc("@title:column", "Name"),
                                i18nc("@title:column", "Folder"),
                                i18nc("@title:column", "Modified"),
                                i18nc("@title:column", "Favourite"),
                                i18nc("@title:column", "Nextcloud tags (server)")});
    m_results->headerItem()->setToolTip(TagsColumn, i18nc("@info:tooltip", "Collaborative tags on the Nextcloud server, separate from Plasma Opus tags"));
    m_results->setRootIsDecorated(false);
    m_results->setUniformRowHeights(true);
    m_results->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_results->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_results->header()->setStretchLastSection(false);
    m_status->setObjectName(QStringLiteral("nextcloudAssetsStatus"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_notice->setObjectName(QStringLiteral("nextcloudAssetsUnavailable"));
    m_notice->setWordWrap(true);
    m_notice->hide();
    m_show->setObjectName(QStringLiteral("nextcloudAssetsShow"));
    m_favorite->setObjectName(QStringLiteral("nextcloudAssetsFavorite"));
    m_favorite->setToolTip(i18nc("@info:tooltip", "Mark as favourites on the Nextcloud server, separate from the Places panel"));
    m_tags->setObjectName(QStringLiteral("nextcloudAssetsTags"));
    m_tags->setToolTip(i18nc("@info:tooltip", "Assign the server's collaborative tags, which are separate from Plasma Opus tags"));

    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(m_mode, 1);
    modeRow->addWidget(m_run);
    m_form->addRow(i18nc("@label:listbox", "Show:"), modeRow);
    m_form->addRow(i18nc("@label:textbox", "Name contains:"), m_text);
    m_form->addRow(i18nc("@label:listbox", "Kind:"), m_type);
    m_form->addRow(QString(), m_favoritesOnly);
    m_form->addRow(i18nc("@label:listbox", "Nextcloud tag:"), m_tag);

    auto *actions = new QHBoxLayout;
    actions->addWidget(m_show);
    actions->addWidget(m_favorite);
    actions->addWidget(m_tags);
    actions->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(m_form);
    layout->addWidget(m_notice);
    layout->addWidget(m_results, 1);
    layout->addWidget(m_status);
    layout->addLayout(actions);
    layout->addWidget(buttons);

    const KConfigGroup state = stateGroup();
    m_mode->setCurrentIndex(std::max(0, m_mode->findData(state.readEntry("Mode", int(Mode::Search)))));
    m_type->setCurrentIndex(std::max(0, m_type->findData(state.readEntry("Type", int(Nextcloud::AssetType::Any)))));

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this] {
        stateGroup().writeEntry("Mode", m_mode->currentData().toInt());
        m_results->clear();
        m_status->clear();
        updateState();
        // Favourites need no criteria, so they are listed right away.
        if (mode() == Mode::Favorites) {
            run();
        }
    });
    connect(m_type, &QComboBox::currentIndexChanged, this, [this] {
        stateGroup().writeEntry("Type", m_type->currentData().toInt());
    });
    connect(m_tag, &QComboBox::activated, this, &NextcloudAssetsDialog::run);
    connect(m_text, &QLineEdit::returnPressed, this, &NextcloudAssetsDialog::run);
    connect(m_run, &QPushButton::clicked, this, &NextcloudAssetsDialog::run);
    connect(m_results, &QTreeWidget::itemSelectionChanged, this, &NextcloudAssetsDialog::updateState);
    connect(m_results, &QTreeWidget::itemDoubleClicked, this, &NextcloudAssetsDialog::showSelected);
    connect(m_show, &QPushButton::clicked, this, &NextcloudAssetsDialog::showSelected);
    connect(m_favorite, &QPushButton::clicked, this, &NextcloudAssetsDialog::toggleFavorite);
    connect(m_tags, &QPushButton::clicked, this, &NextcloudAssetsDialog::editTags);
    connect(m_client, &NextcloudClient::checked, this, &NextcloudAssetsDialog::checked);
    connect(m_client, &NextcloudClient::systemTagsFetched, this, &NextcloudAssetsDialog::systemTagsFetched);
    connect(m_client, &NextcloudClient::assetsListed, this, &NextcloudAssetsDialog::assetsListed);
    connect(m_client, &NextcloudClient::favoriteSet, this, &NextcloudAssetsDialog::favoriteSet);

    m_client->check(m_account);
    m_client->fetchSystemTags(m_account);
    updateModes();
    if (mode() == Mode::Favorites) {
        run();
    }
}

NextcloudAssetsDialog::~NextcloudAssetsDialog() = default;

NextcloudClient *NextcloudAssetsDialog::client() const
{
    return m_client;
}

void NextcloudAssetsDialog::setCapabilities(const Nextcloud::Capabilities &capabilities)
{
    m_capabilities = capabilities;
    updateModes();
}

void NextcloudAssetsDialog::setMode(Mode mode)
{
    m_mode->setCurrentIndex(m_mode->findData(int(mode)));
}

NextcloudAssetsDialog::Mode NextcloudAssetsDialog::mode() const
{
    return Mode(m_mode->currentData().toInt());
}

void NextcloudAssetsDialog::setResultLimit(int limit)
{
    m_resultLimit = std::max(1, limit);
}

int NextcloudAssetsDialog::resultLimit() const
{
    return m_resultLimit;
}

NextcloudTagsDialog *NextcloudAssetsDialog::tagsDialog() const
{
    return m_tagsDialog;
}

void NextcloudAssetsDialog::checked(const QString &accountId, const Nextcloud::Status &status)
{
    // Only a signed-in user sees the capabilities of the server's apps.
    if (accountId != m_account.id || m_capabilities || !status.capabilitiesKnown || !status.signedIn || !status.capabilities.authenticated) {
        return;
    }
    setCapabilities(status.capabilities);
}

void NextcloudAssetsDialog::systemTagsFetched(const QString &accountId, const QList<Nextcloud::SystemTag> &tags, const QString &error)
{
    if (accountId != m_account.id) {
        return;
    }
    m_tagsError = error;
    const QString current = m_tag->currentData().toString();
    m_tag->clear();
    for (const Nextcloud::SystemTag &tag : tags) {
        m_tag->addItem(SymbolicIcon::fromTheme(QStringLiteral("tag")), Nextcloud::describeTag(tag), tag.id);
    }
    if (const int index = m_tag->findData(current); index >= 0) {
        m_tag->setCurrentIndex(index);
    }
    if (m_tag->count() == 0 && m_tagsError.isEmpty()) {
        m_tag->addItem(i18nc("@item:inlistbox", "The server has no tags"));
    }
    updateModes();
}

void NextcloudAssetsDialog::updateModes()
{
    QStringList unavailable;
    const bool search = !m_capabilities || m_capabilities->davSearch;
    const QString searchReason = i18nc("@info", "Searching isn't available: the server doesn't support WebDAV search, which needs Nextcloud 15 or later.");
    setComboItemEnabled(m_mode, m_mode->findData(int(Mode::Search)), search, searchReason);
    if (!search) {
        unavailable.append(searchReason);
    }
    QString tagsReason;
    if (m_capabilities && !m_capabilities->systemTags) {
        tagsReason = i18nc("@info", "Nextcloud tags aren't available: the server's Collaborative tags app is disabled.");
    } else if (!m_tagsError.isEmpty()) {
        tagsReason = i18nc("@info %1 is why the tags couldn't be listed", "Nextcloud tags aren't available: %1", m_tagsError);
    }
    setComboItemEnabled(m_mode, m_mode->findData(int(Mode::Tag)), tagsReason.isEmpty(), tagsReason);
    if (!tagsReason.isEmpty()) {
        unavailable.append(tagsReason);
    }
    m_tags->setToolTip(tagsReason.isEmpty() ? i18nc("@info:tooltip", "Assign the server's collaborative tags, which are separate from Plasma Opus tags")
                                            : tagsReason);
    m_notice->setText(unavailable.join(QLatin1Char('\n')));
    m_notice->setVisible(!unavailable.isEmpty());
    if (!comboItemEnabled(m_mode, m_mode->currentIndex())) {
        setMode(Mode::Favorites);
    }
    updateState();
}

void NextcloudAssetsDialog::run()
{
    if (!comboItemEnabled(m_mode, m_mode->currentIndex())) {
        return;
    }
    m_results->clear();
    KConfigGroup state = stateGroup();
    state.writeEntry("Mode", m_mode->currentData().toInt());
    state.writeEntry("Type", m_type->currentData().toInt());
    switch (mode()) {
    case Mode::Search: {
        Nextcloud::SearchQuery query;
        query.text = m_text->text().trimmed();
        query.type = Nextcloud::AssetType(m_type->currentData().toInt());
        query.favoritesOnly = m_favoritesOnly->isChecked();
        query.limit = m_resultLimit;
        m_status->setText(i18nc("@info:status", "Searching the server…"));
        m_request = m_client->search(m_account, query);
        break;
    }
    case Mode::Favorites:
        m_status->setText(i18nc("@info:status", "Listing your favourites…"));
        m_request = m_client->listFavorites(m_account, m_resultLimit);
        break;
    case Mode::Tag:
        m_status->setText(i18nc("@info:status", "Listing the files with this tag…"));
        m_request = m_client->listTagged(m_account, m_tag->currentData().toString(), m_resultLimit);
        break;
    }
    m_listing = true;
    updateState();
}

void NextcloudAssetsDialog::assetsListed(int request, const Nextcloud::AssetResults &results)
{
    if (request != m_request) {
        return;
    }
    m_listing = false;
    if (!results.error.isEmpty()) {
        m_status->setText(results.error);
        updateState();
        return;
    }
    const QString root = NextcloudAccounts::filesUrl(m_account).path();
    const QMimeDatabase mimeTypes;
    QList<QTreeWidgetItem *> items;
    for (const Nextcloud::FileMetadata &metadata : results.items) {
        const QUrl url = Nextcloud::itemUrl(m_account, metadata.path);
        const QString parent = url.adjusted(QUrl::StripTrailingSlash).adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path();
        auto *item = new QTreeWidgetItem;
        item->setText(NameColumn, itemName(url));
        item->setToolTip(NameColumn, url.toDisplayString(QUrl::RemoveUserInfo));
        QString icon = QStringLiteral("folder");
        if (!metadata.folder) {
            const QMimeType type = metadata.contentType.isEmpty() ? mimeTypes.mimeTypeForFile(itemName(url), QMimeDatabase::MatchExtension)
                                                                  : mimeTypes.mimeTypeForName(metadata.contentType);
            icon = type.isValid() ? type.iconName() : QStringLiteral("unknown");
        }
        item->setIcon(NameColumn, SymbolicIcon::fromTheme(icon));
        item->setText(FolderColumn, parent.size() < root.size() ? QStringLiteral("/") : parent.mid(root.size() - 1));
        if (metadata.modified.isValid()) {
            item->setText(ModifiedColumn, QLocale().toString(metadata.modified.toLocalTime(), QLocale::ShortFormat));
        }
        item->setText(TagsColumn, metadata.serverTags.join(i18nc("@item:intable separator of tag names", ", ")));
        item->setData(NameColumn, UrlRole, url);
        item->setData(NameColumn, FileIdRole, metadata.fileId);
        item->setData(NameColumn, FolderRole, metadata.folder);
        setFavoriteColumn(item, metadata.favorite.value_or(false));
        items.append(item);
    }
    m_results->addTopLevelItems(items);
    m_results->resizeColumnToContents(FolderColumn);
    m_results->resizeColumnToContents(ModifiedColumn);

    QStringList status;
    status.append(results.items.isEmpty() ? i18nc("@info", "Nothing found.") : i18ncp("@info", "%1 result.", "%1 results.", results.items.size()));
    if (results.truncated) {
        status.append(i18ncp("@info",
                             "Only the first result is listed; narrow the search to find the rest.",
                             "Only the first %1 results are listed; narrow the search to find the rest.",
                             results.items.size()));
    }
    if (results.incomplete > 0) {
        status.append(i18ncp("@info",
                             "%1 result is missing or incomplete because the server withheld some of its details.",
                             "%1 results are missing or incomplete because the server withheld some of their details.",
                             results.incomplete));
    }
    m_status->setText(status.join(QLatin1Char(' ')));
    updateState();
}

void NextcloudAssetsDialog::setFavoriteColumn(QTreeWidgetItem *item, bool favorite)
{
    item->setData(NameColumn, FavoriteRole, favorite);
    item->setText(FavoriteColumn, favorite ? i18nc("@item:intable is a favourite", "Yes") : QString());
    item->setIcon(FavoriteColumn, favorite ? SymbolicIcon::fromTheme(QStringLiteral("starred")) : QIcon());
}

QList<QTreeWidgetItem *> NextcloudAssetsDialog::selectedItems() const
{
    QList<QTreeWidgetItem *> items = m_results->selectedItems();
    std::sort(items.begin(), items.end(), [this](QTreeWidgetItem *a, QTreeWidgetItem *b) {
        return m_results->indexOfTopLevelItem(a) < m_results->indexOfTopLevelItem(b);
    });
    return items;
}

QTreeWidgetItem *NextcloudAssetsDialog::itemFor(const QUrl &url) const
{
    const QString key = itemKey(url);
    for (int row = 0; row < m_results->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = m_results->topLevelItem(row);
        if (itemKey(item->data(NameColumn, UrlRole).toUrl()) == key) {
            return item;
        }
    }
    return nullptr;
}

void NextcloudAssetsDialog::showSelected()
{
    const QList<QTreeWidgetItem *> selected = selectedItems();
    QTreeWidgetItem *item = m_results->currentItem() && m_results->currentItem()->isSelected() ? m_results->currentItem() : selected.value(0);
    if (!item) {
        return;
    }
    Q_EMIT showRequested(item->data(NameColumn, UrlRole).toUrl(), item->data(NameColumn, FolderRole).toBool());
}

void NextcloudAssetsDialog::toggleFavorite()
{
    const QList<QTreeWidgetItem *> selected = selectedItems();
    if (selected.isEmpty() || !m_favoritesPending.isEmpty()) {
        return;
    }
    const bool favorite = !std::all_of(selected.cbegin(), selected.cend(), [](const QTreeWidgetItem *item) {
        return item->data(NameColumn, FavoriteRole).toBool();
    });
    m_favoriteErrors.clear();
    m_favoritesChanged.clear();
    QList<QUrl> urls;
    for (const QTreeWidgetItem *item : selected) {
        const QUrl url = item->data(NameColumn, UrlRole).toUrl();
        m_favoritesPending.insert(itemKey(url), favorite);
        urls.append(url);
    }
    m_status->setText(i18nc("@info:status", "Saving favourites on the server…"));
    updateState();
    for (const QUrl &url : std::as_const(urls)) {
        m_client->setFavorite(m_account, url, favorite);
    }
}

void NextcloudAssetsDialog::favoriteSet(const QUrl &url, bool favorite, const QString &error)
{
    const QString key = itemKey(url);
    if (!m_favoritesPending.contains(key)) {
        return;
    }
    m_favoritesPending.remove(key);
    if (error.isEmpty()) {
        if (QTreeWidgetItem *item = itemFor(url)) {
            setFavoriteColumn(item, favorite);
        }
        m_favoritesChanged.append(url);
    } else {
        m_favoriteErrors.append(i18nc("@info %1 is a file name, %2 why it failed", "%1: %2", itemName(url), error));
    }
    if (!m_favoritesPending.isEmpty()) {
        return;
    }
    if (!m_favoritesChanged.isEmpty()) {
        Q_EMIT itemsChanged(m_favoritesChanged);
    }
    if (m_favoriteErrors.isEmpty()) {
        m_status->setText(favorite
                              ? i18ncp("@info", "Added %1 item to your favourites.", "Added %1 items to your favourites.", m_favoritesChanged.size())
                              : i18ncp("@info", "Removed %1 item from your favourites.", "Removed %1 items from your favourites.", m_favoritesChanged.size()));
    } else {
        m_status->setText(i18nc("@info followed by a list of problems", "Some favourites couldn't be changed:") + QLatin1Char('\n')
                          + m_favoriteErrors.join(QLatin1Char('\n')));
    }
    updateState();
}

void NextcloudAssetsDialog::editTags()
{
    const QList<QTreeWidgetItem *> selected = selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    QList<NextcloudTagsDialog::Item> items;
    for (const QTreeWidgetItem *item : selected) {
        items.append({item->data(NameColumn, UrlRole).toUrl(), item->data(NameColumn, FileIdRole).toString()});
    }
    if (m_tagsDialog) {
        m_tagsDialog->close();
    }
    m_tagsDialog = new NextcloudTagsDialog(m_account, items, m_store, this);
    m_tagsDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_tagsDialog, &NextcloudTagsDialog::tagsChanged, this, [this](const QList<QUrl> &changed) {
        Q_EMIT itemsChanged(changed);
        // The results show the tags as they were listed, so list them again.
        run();
    });
    m_tagsDialog->show();
}

void NextcloudAssetsDialog::updateState()
{
    const Mode current = mode();
    for (QWidget *criterion : {static_cast<QWidget *>(m_text), static_cast<QWidget *>(m_type), static_cast<QWidget *>(m_favoritesOnly)}) {
        m_form->setRowVisible(criterion, current == Mode::Search);
    }
    m_form->setRowVisible(m_tag, current == Mode::Tag);
    m_run->setText(current == Mode::Search ? i18nc("@action:button", "Search") : i18nc("@action:button", "List"));
    m_run->setIcon(SymbolicIcon::fromTheme(current == Mode::Search ? QStringLiteral("system-search") : QStringLiteral("view-refresh")));
    const bool modeAvailable = comboItemEnabled(m_mode, m_mode->currentIndex());
    const bool tagChosen = current != Mode::Tag || !m_tag->currentData().toString().isEmpty();
    m_run->setEnabled(modeAvailable && tagChosen && !m_listing);

    const QList<QTreeWidgetItem *> selected = selectedItems();
    const bool idle = !m_listing && m_favoritesPending.isEmpty();
    m_show->setEnabled(idle && !selected.isEmpty());
    const bool allFavorites = !selected.isEmpty() && std::all_of(selected.cbegin(), selected.cend(), [](const QTreeWidgetItem *item) {
        return item->data(NameColumn, FavoriteRole).toBool();
    });
    m_favorite->setText(allFavorites ? i18nc("@action:button", "Remove from Favourites") : i18nc("@action:button", "Add to Favourites"));
    m_favorite->setIcon(SymbolicIcon::fromTheme(allFavorites ? QStringLiteral("non-starred") : QStringLiteral("starred")));
    m_favorite->setEnabled(idle && !selected.isEmpty());
    const bool tagsAvailable = (!m_capabilities || m_capabilities->systemTags) && m_tagsError.isEmpty();
    m_tags->setEnabled(idle && !selected.isEmpty() && tagsAvailable);
}
