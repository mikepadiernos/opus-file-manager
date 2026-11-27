// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "vaultsettings.h"
#include "workspacecatalog.h"

#include <QGroupBox>
#include <QHash>
#include <QMap>
#include <atomic>
#include <memory>
#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class WorkspaceLocations;

/**
 * The Workspace vault group of Preferences › Applications › Editor: whether
 * each workspace's editor vault is on, which of its assigned locations it
 * uses, and its own Markdown options. Changes wait for save(), like other
 * settings, and are stored with the workspace in the workspace catalog.
 */
class WorkspaceVaultSettings : public QGroupBox
{
    Q_OBJECT

public:
    /// @param locations the saved locations that workspaces are assigned
    explicit WorkspaceVaultSettings(WorkspaceLocations *locations, QWidget *parent = nullptr);
    ~WorkspaceVaultSettings() override;
    /// Reads the workspace catalog again, keeping the changes not yet saved.
    void reload();
    /// Forgets the changes not yet saved and reads the workspace catalog again.
    void discard();
    bool hasChanges() const;
    /**
     * Stores the changes with their workspaces. When the catalog can't be
     * saved, the group reports why and keeps the changes.
     * @returns whether the changes were saved
     */
    bool save();

Q_SIGNALS:
    void changed();
    void manageWorkspacesRequested();

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct Vault {
        bool enabled = true;
        QString locationId;
        std::optional<VaultSettings::Options> options;
        bool operator==(const Vault &other) const = default;
    };
    /// What a scan of a vault folder found.
    struct Summary {
        bool readable = false;
        qsizetype notes = 0;
        qsizetype skipped = 0;
        bool truncated = false;
    };

    Vault stored(const QString &workspaceId) const;
    Vault vault(const QString &workspaceId) const;
    QString selectedId() const;
    void showWorkspace();
    void edit();
    void showOptions(const VaultSettings::Options &options);
    VaultSettings::Options shownOptions() const;
    void refreshStatus();
    void scan(const QString &root);

    WorkspaceLocations *m_locations;
    WorkspaceCatalog::Catalog m_catalog;
    QString m_loadError;
    QMap<QString, Vault> m_edits;
    QComboBox *m_workspace;
    QCheckBox *m_enabled;
    QComboBox *m_location;
    QCheckBox *m_ownOptions;
    QWidget *m_options;
    QComboBox *m_mode;
    QComboBox *m_dialect;
    QCheckBox *m_preview;
    QCheckBox *m_wikiLinks;
    QCheckBox *m_backlinks;
    QCheckBox *m_links;
    QCheckBox *m_tags;
    QLabel *m_status;
    bool m_updating = false;
    QHash<QString, Summary> m_summaries;
    QString m_scanning;
    quint64 m_generation = 0;
    std::shared_ptr<std::atomic_bool> m_canceled;
};
