// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "draftstore.h"
#include "notevault.h"
#include "vaultsettings.h"

#include <QWidget>

#include <atomic>
#include <memory>
#include <optional>

namespace KTextEditor
{
class Document;
class View;
}
class QLabel;
class QComboBox;
class QAction;
class QHBoxLayout;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QTextBrowser;
class QTextEdit;
class QTimer;
class QToolButton;
class QTreeWidget;
class QUrl;

class EditorPanel : public QWidget
{
    Q_OBJECT

public:
    enum class Placement {
        Panel,
        Below,
        Right,
        Maximized,
        Window
    };
    /// The active workspace's vault.
    struct WorkspaceVault {
        /// The vault folder, or empty when the workspace has no vault or its folder can't be found.
        QString root;
        /// Why the workspace's vault folder can't be found, such as a removed location, or empty.
        QString unavailable;
        /// The workspace's own Markdown options, or empty to use the Editor settings.
        std::optional<VaultSettings::Options> options;
        /// Whether the workspace decides the vault, even one it can't find.
        bool isActive() const
        {
            return !root.isEmpty() || !unavailable.isEmpty();
        }
        bool operator==(const WorkspaceVault &other) const = default;
    };
    explicit EditorPanel(QWidget *parent = nullptr, const QString &draftDirectory = {});
    ~EditorPanel() override;
    bool openUrl(const QUrl &url);
    bool queryClose();
    /**
     * Closes the document after queryClose() lets the user save or discard
     * unsaved changes. Returns false, keeping the document, if the user keeps
     * editing or a draft can't be written.
     */
    bool closeDocument();
    /// Closes the document without asking, discarding its changes and draft.
    void discardDocument();
    /// Writes pending visual edits into the document; false if they can't be.
    bool syncVisual();
    void applySettings();
    void setPanelTitleVisible(bool visible);
    /**
     * Sets the presentation for hosting the editor in another panel: it hides
     * the Open, Placement and Hide buttons, whose signals the host does not
     * handle, shows the document name below the editor and drops the minimum
     * width.
     */
    void setEmbedded(bool embedded);
    KTextEditor::Document *document() const;
    /**
     * Sets the folder of Markdown notes that wiki links resolve against as
     * the EditorVaultFolder setting, which every editor panel uses, and scans
     * it in the background. An empty @p root closes the vault.
     */
    void setVault(const QString &root);
    /// The vault in use: the workspace vault while one is set, or the panel's own.
    QString vaultRoot() const;
    /**
     * Uses @p root as the vault in every editor panel, including ones created
     * later, while the active workspace has a vault, without remembering it
     * or changing the open documents. An empty @p root restores each panel's
     * own vault.
     */
    static void setWorkspaceVault(const QString &root);
    static QString workspaceVault();
    /**
     * Uses the active workspace's @p vault in every editor panel, as
     * setWorkspaceVault() does. While the workspace's vault folder can't be
     * found, the panels report why instead of using their own vault, and the
     * workspace's options still apply. An inactive @p vault restores each
     * panel's own vault and the Editor settings.
     */
    static void setWorkspaceVaultState(const WorkspaceVault &vault);
    static WorkspaceVault workspaceVaultState();
    /// The last completed scan of the vault.
    const NoteVault::Index &vaultIndex() const;

Q_SIGNALS:
    void placementRequested(EditorPanel::Placement placement);
    void hideRequested();
    /// Emitted when a vault scan completes and its results are shown.
    void vaultScanned();

private:
    void updateTitle();
    bool saveDraft();
    void showDrafts();
    void setDraftError(const QString &error);
    void setVisualMode(bool visual);
    void refreshVisual();
    void updateVisualControls();
    void refreshPreview();
    void rescanVault();
    /// Applies the Markdown and vault settings: the preview dialect and the vault folder.
    void applyMarkdownSettings();
    /// The Markdown and vault options in effect.
    VaultSettings::Options vaultOptions() const;
    /// Switches to the workspace vault or the panel's own, rescanning when it changes.
    void applyVault();
    void updateVaultPanel();
    void updateVaultLinks(const QString &note);
    void updateVaultTags(const QString &note);
    /// The open document's canonical path when it's a local file in the vault, or empty.
    QString currentNotePath() const;
    void openWikiLink(const QUrl &url);
    KTextEditor::Document *m_document = nullptr;
    KTextEditor::View *m_view = nullptr;
    QLabel *m_title = nullptr;
    QHBoxLayout *m_header = nullptr;
    QWidget *m_documentStatus = nullptr;
    QHBoxLayout *m_documentStatusLayout = nullptr;
    bool m_panelTitleVisible = true;
    QTextBrowser *m_preview = nullptr;
    QPushButton *m_previewToggle = nullptr;
    QComboBox *m_dialect = nullptr;
    QTextEdit *m_visual = nullptr;
    QStackedWidget *m_editorStack = nullptr;
    QPushButton *m_openButton = nullptr;
    QWidget *m_placementButton = nullptr;
    QPushButton *m_hideButton = nullptr;
    QPushButton *m_sourceMode = nullptr;
    QPushButton *m_visualMode = nullptr;
    QPushButton *m_visualEnable = nullptr;
    QPushButton *m_visualRetry = nullptr;
    QPushButton *m_visualDiscard = nullptr;
    QWidget *m_visualToolbar = nullptr;
    QLabel *m_visualStatus = nullptr;
    DraftStore m_drafts;
    QString m_draftId;
    QTimer *m_draftTimer = nullptr;
    QLabel *m_draftStatus = nullptr;
    QWidget *m_recovery = nullptr;
    QComboBox *m_draftChoice = nullptr;
    QList<DraftStore::Draft> m_availableDrafts;
    quint64 m_editRevision = 0;
    quint64 m_modeRevision = 0;
    quint64 m_openRevision = 0;
    bool m_opening = false;
    bool m_syncingVisual = false;
    bool m_visualPending = false;
    bool m_visualEditing = false;
    QString m_vaultRoot;
    QString m_ownVaultRoot;
    NoteVault::Index m_vaultIndex;
    bool m_vaultScanning = false;
    quint64 m_vaultGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_vaultCanceled;
    QWidget *m_vaultPanel = nullptr;
    QToolButton *m_vaultButton = nullptr;
    QLabel *m_vaultTitle = nullptr;
    QTabWidget *m_vaultTabs = nullptr;
    VaultSettings::Dialect m_appliedDialect = VaultSettings::Dialect::GitHub;
    QListWidget *m_backlinks = nullptr;
    QListWidget *m_outgoingLinks = nullptr;
    QTreeWidget *m_tags = nullptr;
    QAction *m_vaultChoose = nullptr;
    QLabel *m_vaultStatus = nullptr;
    QAction *m_vaultRescan = nullptr;
    QAction *m_vaultClose = nullptr;
};
