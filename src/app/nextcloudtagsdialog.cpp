// SPDX-License-Identifier: GPL-3.0-or-later

#include "nextcloudtagsdialog.h"

#include "symbolicicon.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
constexpr int TagIdRole = Qt::UserRole;
constexpr int InitialStateRole = Qt::UserRole + 1;

QString itemKey(const QUrl &url)
{
    return url.adjusted(QUrl::RemoveUserInfo | QUrl::StripTrailingSlash).toString();
}

QString itemName(const QUrl &url)
{
    const QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
    return name.isEmpty() ? url.toDisplayString(QUrl::RemoveUserInfo) : name;
}
}

NextcloudTagsDialog::NextcloudTagsDialog(const NextcloudAccounts::Account &account,
                                         const QList<Item> &items,
                                         Nextcloud::CredentialStore *store,
                                         QWidget *parent)
    : QDialog(parent)
    , m_account(account)
    , m_items(items)
    , m_client(new NextcloudClient(this))
    , m_list(new QListWidget(this))
    , m_newTag(new QLineEdit(this))
    , m_create(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Create Tag…"), this))
    , m_status(new QLabel(this))
{
    setWindowTitle(i18nc("@title:window", "Nextcloud Tags (Server)"));
    m_client->setCredentialStore(store);

    const QString subject =
        m_items.size() == 1 ? i18nc("@info %1 is a file name", "“%1”", itemName(m_items.first().url)) : i18ncp("@info", "%1 item", "%1 items", m_items.size());
    auto *intro = new QLabel(i18nc("@info %1 is a file name or a number of items",
                                   "Collaborative tags of %1 on the Nextcloud server. Other users of the server can see these tags. "
                                   "They are separate from Plasma Opus tags, which stay on this computer.",
                                   subject),
                             this);
    intro->setWordWrap(true);

    m_list->setObjectName(QStringLiteral("nextcloudTagList"));
    m_newTag->setObjectName(QStringLiteral("nextcloudNewTag"));
    m_newTag->setPlaceholderText(i18nc("@info:placeholder", "New tag name"));
    m_newTag->setClearButtonEnabled(true);
    m_create->setObjectName(QStringLiteral("nextcloudCreateTag"));
    m_create->setToolTip(i18nc("@info:tooltip", "Create a collaborative tag on the server, after asking for confirmation"));
    m_status->setObjectName(QStringLiteral("nextcloudTagsStatus"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setText(i18nc("@info:status", "Loading the server's tags…"));

    auto *createRow = new QHBoxLayout;
    createRow->addWidget(m_newTag, 1);
    createRow->addWidget(m_create);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setObjectName(QStringLiteral("nextcloudTagsOk"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_list, 1);
    layout->addLayout(createRow);
    layout->addWidget(m_status);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &NextcloudTagsDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_newTag, &QLineEdit::textChanged, this, &NextcloudTagsDialog::updateState);
    connect(m_newTag, &QLineEdit::returnPressed, this, &NextcloudTagsDialog::createTag);
    connect(m_create, &QPushButton::clicked, this, &NextcloudTagsDialog::createTag);
    connect(m_client, &NextcloudClient::metadataFetched, this, &NextcloudTagsDialog::metadataFetched);
    connect(m_client, &NextcloudClient::systemTagsFetched, this, &NextcloudTagsDialog::systemTagsFetched);
    connect(m_client, &NextcloudClient::fileTagsFetched, this, &NextcloudTagsDialog::fileTagsFetched);
    connect(m_client, &NextcloudClient::fileTagSet, this, &NextcloudTagsDialog::fileTagSet);
    connect(m_client, &NextcloudClient::tagCreated, this, &NextcloudTagsDialog::tagCreated);

    // Count every request before sending any, so that no answer finishes loading early.
    QStringList fileIds;
    for (const Item &item : std::as_const(m_items)) {
        if (item.fileId.isEmpty()) {
            ++m_idsPending;
        } else if (!m_fileTags.contains(item.fileId)) {
            m_fileTags.insert(item.fileId, {});
            fileIds.append(item.fileId);
        }
    }
    m_fileTagsPending = int(fileIds.size());
    for (const Item &item : std::as_const(m_items)) {
        if (item.fileId.isEmpty()) {
            m_client->fetchMetadata(m_account, item.url);
        }
    }
    for (const QString &fileId : std::as_const(fileIds)) {
        m_client->fetchFileTags(m_account, fileId);
    }
    m_client->fetchSystemTags(m_account);
    updateState();
}

void NextcloudTagsDialog::setCreationConfirmation(CreationConfirmation confirmation)
{
    m_confirmCreation = std::move(confirmation);
}

NextcloudClient *NextcloudTagsDialog::client() const
{
    return m_client;
}

void NextcloudTagsDialog::metadataFetched(const QUrl &url, const Nextcloud::FileMetadata &metadata, const QString &error)
{
    const QString key = itemKey(url);
    const auto item = std::find_if(m_items.begin(), m_items.end(), [&key](const Item &candidate) {
        return candidate.fileId.isEmpty() && itemKey(candidate.url) == key;
    });
    if (item == m_items.end() || m_idsPending == 0) {
        return;
    }
    --m_idsPending;
    if (!error.isEmpty() || metadata.fileId.isEmpty()) {
        m_loadErrors.append(i18nc("@info %1 is a file name, %2 why it failed",
                                  "%1: %2",
                                  itemName(url),
                                  error.isEmpty() ? i18nc("@info", "The server didn't report the file's ID, which its tags need.") : error));
        // Leave the item out without counting it again.
        item->fileId = QStringLiteral("-");
    } else {
        item->fileId = metadata.fileId;
        if (!m_fileTags.contains(metadata.fileId)) {
            m_fileTags.insert(metadata.fileId, {});
            ++m_fileTagsPending;
            m_client->fetchFileTags(m_account, metadata.fileId);
        }
    }
    finishLoading();
}

void NextcloudTagsDialog::systemTagsFetched(const QString &accountId, const QList<Nextcloud::SystemTag> &tags, const QString &error)
{
    // Creating an existing tag lists the tags again, which only the creation needs.
    if (accountId != m_account.id || m_tagsLoaded) {
        return;
    }
    m_tagsLoaded = true;
    m_tagsError = !error.isEmpty();
    if (m_tagsError) {
        m_loadErrors.prepend(error);
    }
    for (const Nextcloud::SystemTag &tag : tags) {
        if (!std::any_of(m_tags.cbegin(), m_tags.cend(), [&tag](const Nextcloud::SystemTag &known) {
                return known.id == tag.id;
            })) {
            m_tags.append(tag);
        }
    }
    finishLoading();
}

void NextcloudTagsDialog::fileTagsFetched(const QString &fileId, const QList<Nextcloud::SystemTag> &tags, const QString &error)
{
    if (!m_fileTags.contains(fileId) || m_loaded || m_fileTagsPending == 0) {
        return;
    }
    --m_fileTagsPending;
    if (!error.isEmpty()) {
        m_fileTags.remove(fileId);
        for (Item &item : m_items) {
            if (item.fileId == fileId) {
                m_loadErrors.append(i18nc("@info %1 is a file name, %2 why it failed", "%1: %2", itemName(item.url), error));
                item.fileId = QStringLiteral("-");
            }
        }
        finishLoading();
        return;
    }
    QSet<QString> ids;
    for (const Nextcloud::SystemTag &tag : tags) {
        ids.insert(tag.id);
        // A file can have tags that the list of tags leaves out, such as invisible ones.
        if (!std::any_of(m_tags.cbegin(), m_tags.cend(), [&tag](const Nextcloud::SystemTag &known) {
                return known.id == tag.id;
            })) {
            m_tags.append(tag);
        }
    }
    m_fileTags.insert(fileId, ids);
    finishLoading();
}

void NextcloudTagsDialog::finishLoading()
{
    if (!m_tagsLoaded || m_idsPending > 0 || m_fileTagsPending > 0) {
        updateState();
        return;
    }
    m_loaded = true;
    rebuildList();
    QString status;
    if (m_fileTags.isEmpty()) {
        status = i18nc("@info", "None of the items can be tagged on the server.");
    } else if (m_list->count() == 0 && !m_tagsError) {
        status = i18nc("@info", "The server has no collaborative tags yet. Create one below.");
    }
    if (!m_loadErrors.isEmpty()) {
        const QString problems =
            i18nc("@info followed by a list of problems", "Some tags couldn't be read:") + QLatin1Char('\n') + m_loadErrors.join(QLatin1Char('\n'));
        status = status.isEmpty() ? problems : status + QLatin1Char('\n') + problems;
    }
    m_status->setText(status);
    updateState();
}

void NextcloudTagsDialog::rebuildList()
{
    m_list->clear();
    std::stable_sort(m_tags.begin(), m_tags.end(), [](const Nextcloud::SystemTag &a, const Nextcloud::SystemTag &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    for (const Nextcloud::SystemTag &tag : std::as_const(m_tags)) {
        qsizetype count = 0;
        for (const QSet<QString> &ids : std::as_const(m_fileTags)) {
            count += ids.contains(tag.id) ? 1 : 0;
        }
        const Qt::CheckState state = count == 0 ? Qt::Unchecked : count == m_fileTags.size() ? Qt::Checked : Qt::PartiallyChecked;
        addTag(tag, state);
    }
}

QListWidgetItem *NextcloudTagsDialog::addTag(const Nextcloud::SystemTag &tag, Qt::CheckState state)
{
    auto *item = new QListWidgetItem(Nextcloud::describeTag(tag), m_list);
    item->setIcon(SymbolicIcon::fromTheme(QStringLiteral("tag")));
    item->setData(TagIdRole, tag.id);
    item->setData(InitialStateRole, int(state));
    Qt::ItemFlags flags = Qt::ItemIsUserCheckable | Qt::ItemIsSelectable;
    if (state == Qt::PartiallyChecked) {
        flags |= Qt::ItemIsUserTristate;
        item->setToolTip(i18nc("@info:tooltip", "Assigned to some of the items. Click to assign it to all of them or to remove it from all of them."));
    }
    if (Nextcloud::canAssign(tag) && !m_fileTags.isEmpty()) {
        flags |= Qt::ItemIsEnabled;
    } else if (!Nextcloud::canAssign(tag)) {
        item->setToolTip(i18nc("@info:tooltip", "The server doesn't allow you to assign this tag or to remove it."));
    }
    item->setFlags(flags);
    item->setCheckState(state);
    return item;
}

void NextcloudTagsDialog::createTag()
{
    if (!m_create->isEnabled()) {
        return;
    }
    const QString name = m_newTag->text().trimmed();
    if (const QString invalid = Nextcloud::tagNameError(name); !invalid.isEmpty()) {
        m_status->setText(invalid);
        return;
    }
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        const QString id = item->data(TagIdRole).toString();
        const auto tag = std::find_if(m_tags.cbegin(), m_tags.cend(), [&id](const Nextcloud::SystemTag &known) {
            return known.id == id;
        });
        if (tag != m_tags.cend() && tag->name.compare(name, Qt::CaseInsensitive) == 0) {
            if (item->flags().testFlag(Qt::ItemIsEnabled)) {
                item->setCheckState(Qt::Checked);
            }
            m_list->scrollToItem(item);
            m_list->setCurrentItem(item);
            m_newTag->clear();
            m_status->setText(i18nc("@info %1 is a tag name", "The server already has the tag “%1”.", tag->name));
            return;
        }
    }
    const bool confirmed = m_confirmCreation ? m_confirmCreation(name)
                                             : QMessageBox::question(this,
                                                                     i18nc("@title:window", "Create Nextcloud Tag"),
                                                                     i18nc("@info %1 is a tag name",
                                                                           "Create the collaborative tag “%1” on the Nextcloud server?\n\n"
                                                                           "Every user of the server can see the new tag and assign it to their files. "
                                                                           "It is created now, even if you then cancel this dialog.",
                                                                           name))
            == QMessageBox::Yes;
    if (!confirmed) {
        m_status->setText(i18nc("@info", "The tag wasn't created."));
        return;
    }
    m_creating = true;
    m_status->setText(i18nc("@info:status", "Creating the tag…"));
    updateState();
    m_client->createTag(m_account, name);
}

void NextcloudTagsDialog::tagCreated(const QString &accountId, const Nextcloud::SystemTag &tag, const QString &error)
{
    if (accountId != m_account.id || !m_creating) {
        return;
    }
    m_creating = false;
    if (!error.isEmpty()) {
        m_status->setText(error);
        updateState();
        return;
    }
    QListWidgetItem *item = nullptr;
    for (int row = 0; row < m_list->count() && !item; ++row) {
        if (m_list->item(row)->data(TagIdRole).toString() == tag.id) {
            item = m_list->item(row);
        }
    }
    if (!item) {
        m_tags.append(tag);
        item = addTag(tag, Qt::Unchecked);
    }
    if (item->flags().testFlag(Qt::ItemIsEnabled)) {
        item->setCheckState(Qt::Checked);
    }
    m_list->scrollToItem(item);
    m_list->setCurrentItem(item);
    m_newTag->clear();
    m_status->setText(i18nc("@info %1 is a tag name", "Created the tag “%1” on the server. Choose OK to assign it.", tag.name));
    updateState();
}

void NextcloudTagsDialog::apply()
{
    if (!m_loaded || m_creating || m_changesPending > 0) {
        return;
    }
    struct Change {
        QString fileId;
        QString tagId;
        bool assign;
    };
    QList<Change> changes;
    for (int row = 0; row < m_list->count(); ++row) {
        const QListWidgetItem *item = m_list->item(row);
        const auto initial = Qt::CheckState(item->data(InitialStateRole).toInt());
        const Qt::CheckState state = item->checkState();
        if (!item->flags().testFlag(Qt::ItemIsEnabled) || state == initial || state == Qt::PartiallyChecked) {
            continue;
        }
        const QString tagId = item->data(TagIdRole).toString();
        for (auto file = m_fileTags.cbegin(); file != m_fileTags.cend(); ++file) {
            if (file->contains(tagId) != (state == Qt::Checked)) {
                changes.append({file.key(), tagId, state == Qt::Checked});
            }
        }
    }
    if (changes.isEmpty()) {
        accept();
        return;
    }
    m_changesPending = int(changes.size());
    m_applyErrors.clear();
    m_status->setText(i18nc("@info:status", "Saving the tags on the server…"));
    updateState();
    for (const Change &change : std::as_const(changes)) {
        m_client->setFileTag(m_account, change.fileId, change.tagId, change.assign);
    }
}

void NextcloudTagsDialog::fileTagSet(const QString &fileId, const QString &tagId, bool assigned, const QString &error)
{
    if (m_changesPending == 0 || !m_fileTags.contains(fileId)) {
        return;
    }
    --m_changesPending;
    if (error.isEmpty()) {
        if (assigned) {
            m_fileTags[fileId].insert(tagId);
        } else {
            m_fileTags[fileId].remove(tagId);
        }
        m_changedFiles.insert(fileId);
    } else {
        QString name;
        for (const Item &item : std::as_const(m_items)) {
            if (item.fileId == fileId) {
                name = itemName(item.url);
            }
        }
        const auto tag = std::find_if(m_tags.cbegin(), m_tags.cend(), [&tagId](const Nextcloud::SystemTag &known) {
            return known.id == tagId;
        });
        m_applyErrors.append(
            i18nc("@info %1 is a file name, %2 a tag name, %3 why it failed", "%1, %2: %3", name, tag != m_tags.cend() ? tag->name : tagId, error));
    }
    if (m_changesPending > 0) {
        return;
    }
    QList<QUrl> changed;
    for (const Item &item : std::as_const(m_items)) {
        if (m_changedFiles.contains(item.fileId)) {
            changed.append(item.url);
        }
    }
    if (!changed.isEmpty()) {
        Q_EMIT tagsChanged(changed);
    }
    if (m_applyErrors.isEmpty()) {
        accept();
        return;
    }
    // Show what the server has now, so that OK only retries what failed.
    rebuildList();
    m_status->setText(i18nc("@info followed by a list of problems", "Some changes couldn't be saved:") + QLatin1Char('\n')
                      + m_applyErrors.join(QLatin1Char('\n')));
    updateState();
}

void NextcloudTagsDialog::updateState()
{
    const bool busy = !m_loaded || m_creating || m_changesPending > 0;
    m_list->setEnabled(!busy);
    m_ok->setEnabled(!busy);
    m_newTag->setEnabled(m_loaded && !m_tagsError && m_changesPending == 0);
    m_create->setEnabled(!busy && !m_tagsError && !m_newTag->text().trimmed().isEmpty());
}
