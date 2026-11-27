// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "draftstore.h"
#include "notevault.h"

#include <QWidget>

#include <atomic>
#include <memory>

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
class QTextBrowser;
class QTextEdit;
class QTimer;
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
     * Sets the folder of Markdown notes that wiki links resolve against,
     * remembers it for later sessions and scans it in the background. An
     * empty @p root closes the vault.
     */
    void setVault(const QString &root);
    QString vaultRoot() const;
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
    void updateVaultPanel();
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
    NoteVault::Index m_vaultIndex;
    bool m_vaultScanning = false;
    quint64 m_vaultGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_vaultCanceled;
    QWidget *m_vaultPanel = nullptr;
    QListWidget *m_backlinks = nullptr;
    QLabel *m_vaultStatus = nullptr;
    QAction *m_vaultRescan = nullptr;
    QAction *m_vaultClose = nullptr;
};
