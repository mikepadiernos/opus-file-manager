// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacevaultsettings.h"

#include "notevault.h"
#include "workspacelocations.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>

namespace
{
QString catalogGroup()
{
    return QStringLiteral("Workspaces");
}

// A fresh view of the configuration file, so other windows' changes are read.
std::unique_ptr<KConfig> openCatalogConfig()
{
    const KSharedConfigPtr shared = KSharedConfig::openConfig();
    return std::make_unique<KConfig>(shared->name(), shared->openFlags(), shared->locationType());
}
}

WorkspaceVaultSettings::WorkspaceVaultSettings(WorkspaceLocations *locations, QWidget *parent)
    : QGroupBox(i18nc("@title:group", "Workspace Vault"), parent)
    , m_locations(locations)
{
    setObjectName(QStringLiteral("editorWorkspaceVaultGroup"));
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_workspace = new QComboBox;
    m_workspace->setObjectName(QStringLiteral("editorWorkspaceVaultWorkspace"));
    m_workspace->setToolTip(i18nc("@info:tooltip", "The workspace whose vault these settings change"));
    form->addRow(i18nc("@label:listbox", "Workspace:"), m_workspace);
    m_location = new QComboBox;
    m_location->setObjectName(QStringLiteral("editorWorkspaceVaultLocation"));
    m_location->setToolTip(i18nc("@info:tooltip", "One of the workspace's assigned local folders, used as the vault while the workspace is active"));
    form->addRow(i18nc("@label:listbox", "Vault location:"), m_location);
    layout->addLayout(form);
    m_enabled = new QCheckBox(i18nc("@option:check", "Use the workspace's vault in the editor"));
    m_enabled->setObjectName(QStringLiteral("editorWorkspaceVaultEnabled"));
    m_enabled->setToolTip(
        i18nc("@info:tooltip", "When this is off, the editor uses the vault folder chosen under Markdown, and the workspace keeps its location and options"));
    layout->addWidget(m_enabled);
    m_ownOptions = new QCheckBox(i18nc("@option:check", "Use the workspace's own Markdown options"));
    m_ownOptions->setObjectName(QStringLiteral("editorWorkspaceVaultOwnOptions"));
    m_ownOptions->setToolTip(
        i18nc("@info:tooltip", "When this is off, the workspace's vault uses the Markdown settings above and the Markdown mode under Editor › Editor"));
    layout->addWidget(m_ownOptions);

    m_options = new QWidget;
    auto *options = new QVBoxLayout(m_options);
    options->setContentsMargins(0, 0, 0, 0);
    auto *optionsForm = new QFormLayout;
    m_mode = new QComboBox;
    m_mode->setObjectName(QStringLiteral("editorWorkspaceVaultMode"));
    m_mode->addItems({i18nc("@item:inlistbox Markdown editing mode", "Source"), i18nc("@item:inlistbox Markdown editing mode", "Visual")});
    optionsForm->addRow(i18nc("@label:listbox", "Markdown opens in:"), m_mode);
    m_dialect = new QComboBox;
    m_dialect->setObjectName(QStringLiteral("editorWorkspaceVaultDialect"));
    m_dialect->addItems({i18nc("@item:inlistbox", "GitHub Markdown"), i18nc("@item:inlistbox", "CommonMark")});
    optionsForm->addRow(i18nc("@label:listbox", "Preview dialect:"), m_dialect);
    options->addLayout(optionsForm);
    m_preview = new QCheckBox(i18nc("@option:check", "Show the preview when a Markdown file opens"));
    m_preview->setObjectName(QStringLiteral("editorWorkspaceVaultPreview"));
    options->addWidget(m_preview);
    m_wikiLinks = new QCheckBox(i18nc("@option:check", "Follow wiki links to the vault notes they name"));
    m_wikiLinks->setObjectName(QStringLiteral("editorWorkspaceVaultWikiLinks"));
    options->addWidget(m_wikiLinks);
    options->addWidget(new QLabel(i18nc("@label", "The vault panel lists:")));
    auto *lists = new QHBoxLayout;
    m_backlinks = new QCheckBox(i18nc("@option:check", "Backlinks"));
    m_backlinks->setObjectName(QStringLiteral("editorWorkspaceVaultBacklinks"));
    m_links = new QCheckBox(i18nc("@option:check", "Links"));
    m_links->setObjectName(QStringLiteral("editorWorkspaceVaultLinks"));
    m_tags = new QCheckBox(i18nc("@option:check", "Tags"));
    m_tags->setObjectName(QStringLiteral("editorWorkspaceVaultTags"));
    for (QCheckBox *list : {m_backlinks, m_links, m_tags}) {
        lists->addWidget(list);
    }
    lists->addStretch();
    options->addLayout(lists);
    layout->addWidget(m_options);

    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("editorWorkspaceVaultStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    auto *note = new QLabel(i18nc("@info",
                                  "The workspace keeps its vault location and options. Turning its vault off, or removing its location, "
                                  "changes no notes or drafts. The Backlinks, Links and Tags lists show how notes relate; there is no graph view."));
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *manage = new QPushButton(QIcon::fromTheme(QStringLiteral("view-grid")), i18nc("@action:button", "Manage Workspaces…"));
    manage->setObjectName(QStringLiteral("editorWorkspaceVaultManage"));
    manage->setToolTip(i18nc("@info:tooltip", "Assign locations to workspaces, or use one as a workspace's editor vault"));
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(manage);
    buttons->addStretch();
    layout->addLayout(buttons);

    connect(manage, &QPushButton::clicked, this, &WorkspaceVaultSettings::manageWorkspacesRequested);
    connect(m_workspace, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_updating) {
            showWorkspace();
        }
    });
    connect(m_location, &QComboBox::currentIndexChanged, this, [this] {
        if (m_updating) {
            return;
        }
        // Choosing a vault location turns the workspace's vault on, as Use as Editor Vault does.
        if (vault(selectedId()).locationId.isEmpty() && !m_location->currentData().toString().isEmpty()) {
            const QSignalBlocker blocker(m_enabled);
            m_enabled->setChecked(true);
        }
        edit();
    });
    connect(m_ownOptions, &QCheckBox::toggled, this, [this](bool own) {
        if (m_updating) {
            return;
        }
        if (!own) {
            showOptions(VaultSettings::general());
        }
        edit();
    });
    for (QCheckBox *check : {m_enabled, m_preview, m_wikiLinks, m_backlinks, m_links, m_tags}) {
        connect(check, &QCheckBox::toggled, this, &WorkspaceVaultSettings::edit);
    }
    for (QComboBox *combo : {m_mode, m_dialect}) {
        connect(combo, &QComboBox::currentIndexChanged, this, &WorkspaceVaultSettings::edit);
    }
    if (m_locations) {
        connect(m_locations, &WorkspaceLocations::changed, this, [this] {
            if (!m_updating) {
                showWorkspace();
            }
        });
    }
    reload();
}

WorkspaceVaultSettings::~WorkspaceVaultSettings()
{
    if (m_canceled) {
        m_canceled->store(true);
    }
}

void WorkspaceVaultSettings::reload()
{
    const QString selected = selectedId();
    m_catalog = {};
    m_loadError.clear();
    const std::unique_ptr<KConfig> config = openCatalogConfig();
    if (!WorkspaceCatalog::load(config->group(catalogGroup()), m_catalog, m_loadError)) {
        m_catalog = {};
    }
    // Forget changes that were saved elsewhere or whose workspace is gone.
    for (auto edit = m_edits.begin(); edit != m_edits.end();) {
        const bool exists = std::any_of(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [&edit](const auto &workspace) {
            return workspace.id == edit.key();
        });
        edit = !exists || *edit == stored(edit.key()) ? m_edits.erase(edit) : std::next(edit);
    }
    // The notes may have changed since the last scan.
    m_summaries.clear();
    {
        m_updating = true;
        m_workspace->clear();
        for (const auto &workspace : std::as_const(m_catalog.workspaces)) {
            const QString name = i18nc("@item:inlistbox workspace name and short label", "%1 (%2)", workspace.name, workspace.shortLabel);
            m_workspace->addItem(workspace.id == m_catalog.activeId ? i18nc("@item:inlistbox active workspace", "%1 — active", name) : name, workspace.id);
        }
        int index = m_workspace->findData(selected);
        if (index < 0) {
            index = std::max(0, m_workspace->findData(m_catalog.activeId));
        }
        m_workspace->setCurrentIndex(m_workspace->count() ? index : -1);
        m_updating = false;
    }
    showWorkspace();
}

void WorkspaceVaultSettings::discard()
{
    m_edits.clear();
    reload();
}

bool WorkspaceVaultSettings::hasChanges() const
{
    return !m_edits.isEmpty();
}

bool WorkspaceVaultSettings::save()
{
    if (m_edits.isEmpty()) {
        return true;
    }
    const std::unique_ptr<KConfig> config = openCatalogConfig();
    KConfigGroup group = config->group(catalogGroup());
    WorkspaceCatalog::Catalog current;
    QString error;
    bool saved = WorkspaceCatalog::load(group, current, error);
    WorkspaceCatalog::Catalog updated = current;
    for (auto edit = m_edits.cbegin(); saved && edit != m_edits.cend(); ++edit) {
        const auto workspace = std::find_if(updated.workspaces.cbegin(), updated.workspaces.cend(), [&edit](const auto &entry) {
            return entry.id == edit.key();
        });
        if (workspace == updated.workspaces.cend()) {
            continue;
        }
        if (workspace->vaultLocationId != edit->locationId) {
            saved = WorkspaceCatalog::setVault(updated, edit.key(), edit->locationId, error);
        }
        saved = saved && WorkspaceCatalog::setVaultOptions(updated, edit.key(), edit->enabled, edit->options, error);
    }
    saved = saved && (updated == current || WorkspaceCatalog::saveIfUnchanged(group, current, updated, error));
    if (!saved) {
        m_status->setText(i18nc("@info %1 is the reason", "The workspace vault settings could not be saved: %1", error));
        return false;
    }
    m_edits.clear();
    reload();
    return true;
}

void WorkspaceVaultSettings::showEvent(QShowEvent *event)
{
    QGroupBox::showEvent(event);
    refreshStatus();
}

WorkspaceVaultSettings::Vault WorkspaceVaultSettings::stored(const QString &workspaceId) const
{
    const auto workspace = std::find_if(m_catalog.workspaces.cbegin(), m_catalog.workspaces.cend(), [&workspaceId](const auto &entry) {
        return entry.id == workspaceId;
    });
    if (workspace == m_catalog.workspaces.cend()) {
        return {};
    }
    return {workspace->vaultEnabled, workspace->vaultLocationId, workspace->vaultOptions};
}

WorkspaceVaultSettings::Vault WorkspaceVaultSettings::vault(const QString &workspaceId) const
{
    return m_edits.value(workspaceId, stored(workspaceId));
}

QString WorkspaceVaultSettings::selectedId() const
{
    return m_workspace->currentData().toString();
}

void WorkspaceVaultSettings::showWorkspace()
{
    const QString id = selectedId();
    const Vault shown = vault(id);
    m_updating = true;
    m_location->clear();
    m_location->addItem(i18nc("@item:inlistbox no workspace vault", "None"), QString());
    QStringList ids;
    for (auto assignment = m_catalog.locationAssignments.cbegin(); assignment != m_catalog.locationAssignments.cend(); ++assignment) {
        if (assignment.value() == id) {
            ids.append(assignment.key());
        }
    }
    // The workspace's vault is always shown, even when its location is gone.
    if (!shown.locationId.isEmpty() && !ids.contains(shown.locationId)) {
        ids.append(shown.locationId);
    }
    const QList<WorkspaceLocations::Location> saved = m_locations ? m_locations->locations() : QList<WorkspaceLocations::Location>();
    auto *model = qobject_cast<QStandardItemModel *>(m_location->model());
    for (const QString &locationId : std::as_const(ids)) {
        const auto location = std::find_if(saved.cbegin(), saved.cend(), [&locationId](const auto &entry) {
            return entry.id == locationId;
        });
        const QString title = location == saved.cend() ? i18nc("@item:inlistbox", "Removed location") : location->title;
        QString reason = i18nc("@info", "The saved locations can't be read here.");
        const QString folder = m_locations ? m_locations->vaultFolder(locationId, &reason) : QString();
        m_location->addItem(reason.isEmpty() ? title : i18nc("@item:inlistbox %1 is a location name", "%1 (unavailable)", title), locationId);
        const int row = m_location->count() - 1;
        m_location->setItemData(row, reason.isEmpty() ? folder : reason, Qt::ToolTipRole);
        if (!reason.isEmpty() && model && locationId != shown.locationId) {
            model->item(row)->setEnabled(false);
        }
    }
    m_location->setCurrentIndex(std::max(0, m_location->findData(shown.locationId)));
    m_enabled->setChecked(shown.enabled);
    m_ownOptions->setChecked(shown.options.has_value());
    showOptions(shown.options.value_or(VaultSettings::general()));
    m_updating = false;

    const bool hasWorkspace = !id.isEmpty();
    for (QWidget *widget : {static_cast<QWidget *>(m_workspace), static_cast<QWidget *>(m_location), static_cast<QWidget *>(m_ownOptions)}) {
        widget->setEnabled(hasWorkspace);
    }
    m_enabled->setEnabled(hasWorkspace && !shown.locationId.isEmpty());
    m_options->setEnabled(hasWorkspace && shown.options.has_value());
    refreshStatus();
}

void WorkspaceVaultSettings::edit()
{
    if (m_updating) {
        return;
    }
    const QString id = selectedId();
    if (id.isEmpty()) {
        return;
    }
    Vault edited;
    edited.locationId = m_location->currentData().toString();
    edited.enabled = m_enabled->isChecked();
    if (m_ownOptions->isChecked()) {
        edited.options = shownOptions();
    }
    if (edited == stored(id)) {
        m_edits.remove(id);
    } else {
        m_edits.insert(id, edited);
    }
    m_enabled->setEnabled(!edited.locationId.isEmpty());
    m_options->setEnabled(edited.options.has_value());
    refreshStatus();
    Q_EMIT changed();
}

void WorkspaceVaultSettings::showOptions(const VaultSettings::Options &options)
{
    const bool updating = m_updating;
    m_updating = true;
    m_mode->setCurrentIndex(options.visual ? 1 : 0);
    m_dialect->setCurrentIndex(options.dialect == VaultSettings::Dialect::CommonMark ? 1 : 0);
    m_preview->setChecked(options.preview);
    m_wikiLinks->setChecked(options.wikiLinks);
    m_backlinks->setChecked(options.backlinks);
    m_links->setChecked(options.links);
    m_tags->setChecked(options.tags);
    m_updating = updating;
}

VaultSettings::Options WorkspaceVaultSettings::shownOptions() const
{
    VaultSettings::Options options;
    options.visual = m_mode->currentIndex() == 1;
    options.dialect = m_dialect->currentIndex() == 1 ? VaultSettings::Dialect::CommonMark : VaultSettings::Dialect::GitHub;
    options.preview = m_preview->isChecked();
    options.wikiLinks = m_wikiLinks->isChecked();
    options.backlinks = m_backlinks->isChecked();
    options.links = m_links->isChecked();
    options.tags = m_tags->isChecked();
    return options;
}

void WorkspaceVaultSettings::refreshStatus()
{
    if (!m_loadError.isEmpty()) {
        m_status->setText(i18nc("@info %1 is the reason", "The workspaces can't be read: %1", m_loadError));
        return;
    }
    const QString id = selectedId();
    if (id.isEmpty()) {
        m_status->setText(i18nc("@info", "There are no workspaces yet. Create one with Manage Workspaces to give it its own vault."));
        return;
    }
    const Vault shown = vault(id);
    if (shown.locationId.isEmpty()) {
        m_status->setText(i18nc("@info",
                                "This workspace has no vault, so the editor uses the vault folder chosen under Markdown. Choose one of the "
                                "workspace's assigned local folders, or assign one with Manage Workspaces."));
        return;
    }
    if (!shown.enabled) {
        m_status->setText(i18nc("@info",
                                "The workspace's vault is turned off, so the editor uses the vault folder chosen under Markdown. The "
                                "workspace keeps its vault location and options for when you turn it on."));
        return;
    }
    const QString active = id == m_catalog.activeId ? QString() : QLatin1Char(' ') + i18nc("@info", "The editor uses it while the workspace is active.");
    QString reason = i18nc("@info", "The saved locations can't be read here.");
    const QString folder = m_locations ? m_locations->vaultFolder(shown.locationId, &reason) : QString();
    if (!reason.isEmpty()) {
        m_status->setText(
            reason + QLatin1Char(' ')
            + i18nc("@info", "While the workspace is active, the editor reports this instead of using another vault, and keeps its notes and drafts."));
        return;
    }
    const QString root = QDir::cleanPath(folder);
    if (const QString unavailable = VaultSettings::unavailableReason(root); !unavailable.isEmpty()) {
        m_status->setText(unavailable + QLatin1Char(' ') + i18nc("@info", "The editor reports this and reads the folder again when it can."));
        return;
    }
    const auto summary = m_summaries.constFind(root);
    if (summary == m_summaries.cend()) {
        m_status->setText(i18nc("@info %1 is a folder path", "Indexing the notes in %1…", root));
        scan(root);
        return;
    }
    if (!summary->readable) {
        m_status->setText(i18nc("@info %1 is a folder path", "The vault folder %1 can't be read.", root));
        return;
    }
    QString text = i18ncp("@info %2 is a folder path",
                          "The workspace vault %2 has %1 Markdown note.",
                          "The workspace vault %2 has %1 Markdown notes.",
                          summary->notes,
                          root);
    if (summary->truncated) {
        text += QLatin1Char(' ')
            + i18nc("@info %1 is a number of files",
                    "Indexing stopped at %1 files, so links, backlinks and tags leave out some notes.",
                    NoteVault::DefaultFileLimit);
    }
    if (summary->skipped) {
        text += QLatin1Char(' ')
            + i18ncp("@info",
                     "%1 note was skipped because it is too large, unreadable or not valid UTF-8.",
                     "%1 notes were skipped because they are too large, unreadable or not valid UTF-8.",
                     summary->skipped);
    }
    m_status->setText(text + active);
}

void WorkspaceVaultSettings::scan(const QString &root)
{
    if (m_scanning == root) {
        return;
    }
    if (m_canceled) {
        m_canceled->store(true);
    }
    m_scanning = root;
    const quint64 generation = ++m_generation;
    const auto canceled = std::make_shared<std::atomic_bool>(false);
    m_canceled = canceled;
    auto *watcher = new QFutureWatcher<Summary>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, root] {
        watcher->deleteLater();
        if (generation != m_generation) {
            return;
        }
        m_scanning.clear();
        m_canceled.reset();
        m_summaries.insert(root, watcher->result());
        refreshStatus();
    });
    watcher->setFuture(QtConcurrent::run([root, canceled] {
        const NoteVault::Index index = NoteVault::scan(root, NoteVault::DefaultFileLimit, [canceled] {
            return canceled->load();
        });
        return Summary{!index.root.isEmpty(), index.notes.size(), index.skipped.size(), index.truncated};
    }));
}
