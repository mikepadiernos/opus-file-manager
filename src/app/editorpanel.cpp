// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorpanel.h"
#include "localizeddates.h"
#include "markdownexport.h"
#include "safetextdocument.h"
#include "settings.h"
#include "vaultsettings.h"
#include "wikilinks.h"

#include <KConfigGroup>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMimeDatabase>
#include <QPageSize>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <utility>

namespace
{
QString defaultDraftDirectory()
{
    const QString data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return data.isEmpty() ? QString() : QDir(data).filePath(QStringLiteral("drafts"));
}

constexpr QLatin1StringView WikiLinkScheme("opus-note");

QString cleanVaultRoot(const QString &root)
{
    return root.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(root).absoluteFilePath());
}

// Only one main window shows editors, so the active workspace's vault applies process-wide.
EditorPanel::WorkspaceVault s_workspaceVault;
QList<EditorPanel *> s_editorPanels;
}

EditorPanel::EditorPanel(QWidget *parent, const QString &draftDirectory)
    : QWidget(parent)
    , m_document(KTextEditor::Editor::instance()->createDocument(this))
    , m_title(new QLabel(this))
    , m_drafts(draftDirectory.isEmpty() ? defaultDraftDirectory() : draftDirectory)
    , m_draftId(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_draftTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("editorPanel"));
    m_document->setModifiedOnDiskWarning(true);
    m_title->setObjectName(QStringLiteral("editorTitle"));
    m_title->setTextFormat(Qt::PlainText);
    auto *open = new QPushButton(i18nc("@action", "Open…"), this);
    open->setObjectName(QStringLiteral("editorOpen"));
    m_openButton = open;
    auto *save = new QPushButton(i18nc("@action", "Save"), this);
    save->setShortcut(QKeySequence::Save);
    auto *saveAs = new QPushButton(i18nc("@action", "Save As…"), this);
    m_sourceMode = new QPushButton(i18nc("@action", "Source"), this);
    m_sourceMode->setObjectName(QStringLiteral("editorSourceMode"));
    m_sourceMode->setCheckable(true);
    m_visualMode = new QPushButton(i18nc("@action", "Visual"), this);
    m_visualMode->setObjectName(QStringLiteral("editorVisualMode"));
    m_visualMode->setCheckable(true);
    auto *preview = new QPushButton(i18nc("@action", "Markdown Preview"), this);
    m_previewToggle = preview;
    preview->setObjectName(QStringLiteral("editorMarkdownPreview"));
    preview->setCheckable(true);
    auto *dialect = new QComboBox(this);
    dialect->setObjectName(QStringLiteral("editorMarkdownDialect"));
    dialect->setAccessibleName(i18nc("@label", "Markdown preview dialect"));
    dialect->setToolTip(i18nc("@info:tooltip", "Choose the preview dialect. Visual editing and PDF export use their existing Markdown behavior."));
    dialect->addItem(i18nc("@item:inlistbox", "GitHub Markdown"));
    dialect->addItem(i18nc("@item:inlistbox", "CommonMark"));
    dialect->hide();
    m_dialect = dialect;
    auto *pdf = new QPushButton(i18nc("@action", "Export PDF…"), this);
    pdf->setObjectName(QStringLiteral("editorExportPdf"));
    auto *vault = new QToolButton(this);
    m_vaultButton = vault;
    vault->setObjectName(QStringLiteral("editorVault"));
    vault->setText(i18nc("@action", "Vault"));
    vault->setToolTip(i18nc("@info:tooltip", "Choose a folder of Markdown notes for wiki links and backlinks"));
    vault->setPopupMode(QToolButton::InstantPopup);
    auto *vaultMenu = new QMenu(vault);
    vault->setMenu(vaultMenu);
    QAction *chooseVault = vaultMenu->addAction(i18nc("@action", "Choose Vault Folder…"));
    m_vaultChoose = chooseVault;
    chooseVault->setObjectName(QStringLiteral("editorVaultChoose"));
    m_vaultRescan = vaultMenu->addAction(i18nc("@action", "Rescan Vault"));
    m_vaultRescan->setObjectName(QStringLiteral("editorVaultRescan"));
    m_vaultClose = vaultMenu->addAction(i18nc("@action", "Close Vault"));
    m_vaultClose->setObjectName(QStringLiteral("editorVaultClose"));
    auto *placement = new QToolButton(this);
    placement->setObjectName(QStringLiteral("editorPlacement"));
    placement->setText(i18nc("@action", "Placement"));
    placement->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(placement);
    placement->setMenu(menu);
    const auto addPlacement = [this, menu](const QString &text, const QString &name, Placement value) {
        QAction *action = menu->addAction(text);
        action->setObjectName(name);
        connect(action, &QAction::triggered, this, [this, value] {
            Q_EMIT placementRequested(value);
        });
    };
    addPlacement(i18nc("@action", "Side Panel"), QStringLiteral("editorPlacePanel"), Placement::Panel);
    addPlacement(i18nc("@action", "Below Panes"), QStringLiteral("editorPlaceBelow"), Placement::Below);
    addPlacement(i18nc("@action", "Right of Panes"), QStringLiteral("editorPlaceRight"), Placement::Right);
    addPlacement(i18nc("@action", "Maximized"), QStringLiteral("editorPlaceMaximized"), Placement::Maximized);
    addPlacement(i18nc("@action", "Separate Window"), QStringLiteral("editorPlaceWindow"), Placement::Window);
    auto *hide = new QPushButton(i18nc("@action", "Hide"), this);
    hide->setObjectName(QStringLiteral("editorHide"));
    m_placementButton = placement;
    m_hideButton = hide;
    m_header = new QHBoxLayout;
    m_header->addWidget(m_title, 1);
    m_header->addWidget(open);
    m_header->addWidget(save);
    m_header->addWidget(saveAs);
    m_header->addWidget(m_sourceMode);
    m_header->addWidget(m_visualMode);
    m_header->addWidget(preview);
    m_header->addWidget(pdf);
    m_header->addWidget(vault);
    m_header->addWidget(placement);
    m_header->addWidget(hide);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(qMax(8, layout->contentsMargins().left()), -1, qMax(8, layout->contentsMargins().right()), -1);
    layout->addLayout(m_header);
    m_recovery = new QWidget(this);
    m_recovery->setObjectName(QStringLiteral("editorDraftRecovery"));
    auto *recoveryLayout = new QHBoxLayout(m_recovery);
    recoveryLayout->setContentsMargins(0, 0, 0, 0);
    recoveryLayout->addWidget(new QLabel(i18nc("@info", "Recover draft:"), m_recovery));
    m_draftChoice = new QComboBox(m_recovery);
    recoveryLayout->addWidget(m_draftChoice, 1);
    auto *review = new QPushButton(i18nc("@action", "Review…"), m_recovery);
    auto *restore = new QPushButton(i18nc("@action", "Restore"), m_recovery);
    restore->setObjectName(QStringLiteral("editorRestoreDraft"));
    auto *discard = new QPushButton(i18nc("@action", "Discard"), m_recovery);
    discard->setObjectName(QStringLiteral("editorDiscardDraft"));
    recoveryLayout->addWidget(review);
    recoveryLayout->addWidget(restore);
    recoveryLayout->addWidget(discard);
    layout->addWidget(m_recovery);
    m_recovery->hide();
    m_draftStatus = new QLabel(this);
    m_draftStatus->setWordWrap(true);
    m_draftStatus->hide();
    layout->addWidget(m_draftStatus);
    m_visualToolbar = new QWidget(this);
    auto *visualTools = new QHBoxLayout(m_visualToolbar);
    visualTools->setContentsMargins(0, 0, 0, 0);
    m_visualStatus = new QLabel(m_visualToolbar);
    m_visualStatus->setWordWrap(true);
    visualTools->addWidget(m_visualStatus, 1);
    m_visualEnable = new QPushButton(i18nc("@action", "Enable Visual Editing…"), m_visualToolbar);
    m_visualEnable->setObjectName(QStringLiteral("editorEnableVisualEditing"));
    m_visualRetry = new QPushButton(i18nc("@action", "Retry"), m_visualToolbar);
    m_visualRetry->setObjectName(QStringLiteral("editorRetryVisualSync"));
    m_visualDiscard = new QPushButton(i18nc("@action", "Discard Visual Edits…"), m_visualToolbar);
    m_visualDiscard->setObjectName(QStringLiteral("editorDiscardVisualEdits"));
    auto *bold = new QPushButton(i18nc("@action", "Bold"), m_visualToolbar);
    bold->setObjectName(QStringLiteral("editorVisualBold"));
    auto *italic = new QPushButton(i18nc("@action", "Italic"), m_visualToolbar);
    italic->setObjectName(QStringLiteral("editorVisualItalic"));
    visualTools->addWidget(m_visualEnable);
    visualTools->addWidget(m_visualRetry);
    visualTools->addWidget(m_visualDiscard);
    visualTools->addWidget(bold);
    visualTools->addWidget(italic);
    layout->addWidget(m_visualToolbar);
    layout->addWidget(dialect, 0, Qt::AlignLeft);
    auto *splitter = new QSplitter(this);
    m_editorStack = new QStackedWidget(splitter);
    m_view = m_document->createView(m_editorStack);
    m_editorStack->addWidget(m_view);
    m_visual = new QTextEdit(m_editorStack);
    m_visual->setObjectName(QStringLiteral("editorVisualText"));
    m_visual->setDocument(new SafeTextDocument(m_visual));
    m_visual->setAcceptRichText(false);
    m_visual->setReadOnly(true);
    m_editorStack->addWidget(m_visual);
    splitter->addWidget(m_editorStack);
    m_preview = new QTextBrowser(splitter);
    m_preview->setObjectName(QStringLiteral("editorRenderedMarkdown"));
    m_preview->setOpenLinks(false);
    m_preview->setOpenExternalLinks(false);
    m_preview->setDocument(new SafeTextDocument(m_preview));
    splitter->addWidget(m_preview);
    m_preview->hide();
    m_vaultPanel = new QWidget(splitter);
    m_vaultPanel->setObjectName(QStringLiteral("editorVaultPanel"));
    auto *vaultLayout = new QVBoxLayout(m_vaultPanel);
    vaultLayout->setContentsMargins(0, 0, 0, 0);
    m_vaultTitle = new QLabel(m_vaultPanel);
    m_vaultTitle->setObjectName(QStringLiteral("editorVaultTitle"));
    m_vaultTitle->setTextFormat(Qt::PlainText);
    m_vaultTitle->setWordWrap(true);
    vaultLayout->addWidget(m_vaultTitle);
    auto *vaultTabs = new QTabWidget(m_vaultPanel);
    m_vaultTabs = vaultTabs;
    vaultTabs->setObjectName(QStringLiteral("editorVaultTabs"));
    vaultTabs->setDocumentMode(true);
    m_backlinks = new QListWidget(vaultTabs);
    m_backlinks->setObjectName(QStringLiteral("editorBacklinks"));
    m_backlinks->setAccessibleName(i18nc("@label", "Notes that link to this note"));
    vaultTabs->addTab(m_backlinks, i18nc("@title:tab", "Backlinks"));
    m_outgoingLinks = new QListWidget(vaultTabs);
    m_outgoingLinks->setObjectName(QStringLiteral("editorOutgoingLinks"));
    m_outgoingLinks->setAccessibleName(i18nc("@label", "Notes and files this note links to"));
    vaultTabs->addTab(m_outgoingLinks, i18nc("@title:tab", "Links"));
    m_tags = new QTreeWidget(vaultTabs);
    m_tags->setObjectName(QStringLiteral("editorTags"));
    m_tags->setAccessibleName(i18nc("@label", "This note's tags and the other notes that share them"));
    m_tags->setHeaderHidden(true);
    m_tags->setRootIsDecorated(true);
    vaultTabs->addTab(m_tags, i18nc("@title:tab", "Tags"));
    vaultLayout->addWidget(vaultTabs, 1);
    m_vaultStatus = new QLabel(m_vaultPanel);
    m_vaultStatus->setObjectName(QStringLiteral("editorVaultStatus"));
    m_vaultStatus->setTextFormat(Qt::PlainText);
    m_vaultStatus->setWordWrap(true);
    vaultLayout->addWidget(m_vaultStatus);
    splitter->addWidget(m_vaultPanel);
    m_vaultPanel->hide();
    layout->addWidget(splitter, 1);
    m_documentStatus = new QWidget(this);
    m_documentStatus->setObjectName(QStringLiteral("editorDocumentStatus"));
    m_documentStatusLayout = new QHBoxLayout(m_documentStatus);
    m_documentStatusLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_documentStatus);
    m_documentStatus->hide();
    connect(open, &QPushButton::clicked, this, [this] {
        const QUrl url = QFileDialog::getOpenFileUrl(this, i18nc("@title:window", "Open Text File"));
        if (!url.isEmpty()) {
            openUrl(url);
        }
    });
    connect(save, &QPushButton::clicked, this, [this] {
        if (syncVisual()) {
            m_document->documentSave();
        }
    });
    connect(saveAs, &QPushButton::clicked, this, [this] {
        if (syncVisual()) {
            m_document->documentSaveAs();
        }
    });
    connect(m_sourceMode, &QPushButton::clicked, this, [this] {
        ++m_modeRevision;
        setVisualMode(false);
    });
    connect(m_visualMode, &QPushButton::clicked, this, [this] {
        ++m_modeRevision;
        setVisualMode(true);
    });
    connect(m_visualEnable, &QPushButton::clicked, this, [this] {
        if (!m_document->isReadWrite()) {
            KMessageBox::error(this, i18nc("@info", "This document is read-only."));
            return;
        }
        if (KMessageBox::warningContinueCancel(this,
                                               i18nc("@info",
                                                     "Visual editing rewrites Markdown and can change or remove unsupported formatting, HTML, and resources. "
                                                     "The original file is not changed until you save."),
                                               i18nc("@title:window", "Enable Visual Editing"))
            != KMessageBox::Continue) {
            return;
        }
        if (Settings::editorDraftsEnabled() && !m_document->text().isEmpty()) {
            const QString snapshot = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const QString error = m_drafts.write({snapshot, m_document->text(), m_document->url(), QDateTime::currentMSecsSinceEpoch()});
            if (!error.isEmpty()) {
                KMessageBox::error(this, i18nc("@info", "Could not preserve the Markdown before visual editing: %1", error));
                return;
            }
            m_drafts.release(snapshot);
        }
        m_visualEditing = true;
        updateVisualControls();
        m_visual->setFocus();
    });
    connect(m_visualRetry, &QPushButton::clicked, this, [this] {
        syncVisual();
    });
    connect(m_visualDiscard, &QPushButton::clicked, this, [this] {
        if (KMessageBox::warningContinueCancel(this,
                                               i18nc("@info", "Discard the visual edits that could not be synchronized?"),
                                               i18nc("@title:window", "Discard Visual Edits"))
            == KMessageBox::Continue) {
            m_visualPending = false;
            m_visualEditing = false;
            refreshVisual();
            updateVisualControls();
        }
    });
    connect(bold, &QPushButton::clicked, this, [this] {
        QTextCharFormat format;
        format.setFontWeight(m_visual->fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
        m_visual->mergeCurrentCharFormat(format);
        m_visual->setFocus();
    });
    connect(italic, &QPushButton::clicked, this, [this] {
        QTextCharFormat format;
        format.setFontItalic(!m_visual->fontItalic());
        m_visual->mergeCurrentCharFormat(format);
        m_visual->setFocus();
    });
    connect(m_visual->document(), &QTextDocument::contentsChanged, this, [this] {
        if (!m_syncingVisual && m_visualEditing) {
            m_visualPending = true;
            syncVisual();
        }
    });
    connect(dialect, &QComboBox::currentIndexChanged, this, &EditorPanel::refreshPreview);
    connect(preview, &QPushButton::toggled, this, [this, dialect](bool shown) {
        refreshPreview();
        dialect->setVisible(shown);
        m_preview->setVisible(shown);
    });
    connect(m_preview, &QTextBrowser::anchorClicked, this, &EditorPanel::openWikiLink);
    connect(chooseVault, &QAction::triggered, this, [this] {
        const QString root = QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Choose Vault Folder"), m_vaultRoot);
        if (!root.isEmpty()) {
            setVault(root);
        }
    });
    connect(m_vaultRescan, &QAction::triggered, this, &EditorPanel::rescanVault);
    connect(m_vaultClose, &QAction::triggered, this, [this] {
        setVault({});
    });
    connect(m_backlinks, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        openUrl(QUrl::fromLocalFile(item->data(Qt::UserRole).toString()));
    });
    connect(m_outgoingLinks, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        openWikiLink(item->data(Qt::UserRole).toUrl());
    });
    connect(m_tags, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) {
            openUrl(QUrl::fromLocalFile(path));
        }
    });
    connect(m_document, &KTextEditor::Document::documentUrlChanged, this, [this] {
        updateVaultPanel();
        refreshPreview();
    });
    // The URL changes before the text loads, so links are counted again once it has.
    connect(m_document, &KParts::ReadOnlyPart::completed, this, &EditorPanel::updateVaultPanel);
    connect(m_document, &KTextEditor::Document::textChanged, this, [this] {
        ++m_editRevision;
        refreshPreview();
        if (!m_syncingVisual && !m_visualPending && m_editorStack->currentWidget() == m_visual) {
            refreshVisual();
        }
        if (Settings::editorDraftsEnabled() && !m_opening && m_document->isModified()) {
            m_draftTimer->start();
        }
    });
    connect(pdf, &QPushButton::clicked, this, [this] {
        if (!syncVisual()) {
            return;
        }
        const QFileInfo source(m_document->url().toLocalFile());
        const QString suggested =
            source.filePath().isEmpty() ? QStringLiteral("document.pdf") : source.dir().filePath(source.completeBaseName() + QStringLiteral(".pdf"));
        const QString destination =
            QFileDialog::getSaveFileName(this, i18nc("@title:window", "Export Markdown as PDF"), suggested, i18nc("@item:inlistbox", "PDF files (*.pdf)"));
        if (destination.isEmpty()) {
            return;
        }
        QPageSize::PageSizeId pageSize = QPageSize::A4;
        if (Settings::editorPdfPageSize() == Settings::EnumEditorPdfPageSize::Letter) {
            pageSize = QPageSize::Letter;
        } else if (Settings::editorPdfPageSize() == Settings::EnumEditorPdfPageSize::Legal) {
            pageSize = QPageSize::Legal;
        }
        const QMarginsF margins(Settings::editorPdfMarginLeft(),
                                Settings::editorPdfMarginTop(),
                                Settings::editorPdfMarginRight(),
                                Settings::editorPdfMarginBottom());
        const QPageLayout pageLayout(QPageSize(pageSize), QPageLayout::Portrait, margins, QPageLayout::Millimeter);
        const QString error = MarkdownExport::toPdf(m_document->text(), m_document->url(), destination, pageLayout);
        if (!error.isEmpty()) {
            KMessageBox::error(this, i18nc("@info", "Could not export PDF: %1", error));
        }
    });
    connect(hide, &QPushButton::clicked, this, &EditorPanel::hideRequested);
    connect(m_document, &KTextEditor::Document::documentNameChanged, this, &EditorPanel::updateTitle);
    connect(m_document, &KTextEditor::Document::modifiedChanged, this, &EditorPanel::updateTitle);
    connect(KTextEditor::Editor::instance(), &KTextEditor::Editor::configChanged, this, &EditorPanel::applySettings);
    connect(m_document, &KTextEditor::Document::modifiedChanged, this, [this] {
        if (Settings::editorDraftsEnabled() && !m_opening && m_document->isModified()) {
            m_draftTimer->start();
        }
    });
    m_draftTimer->setSingleShot(true);
    m_draftTimer->setInterval(Settings::editorDraftIntervalMs());
    connect(m_draftTimer, &QTimer::timeout, this, &EditorPanel::saveDraft);
    connect(m_document, &KTextEditor::Document::documentSavedOrUploaded, this, [this] {
        const QString savedId = m_draftId;
        const quint64 savedRevision = m_editRevision;
        QTimer::singleShot(0, this, [this, savedId, savedRevision] {
            if (savedRevision == m_editRevision && (savedId != m_draftId || !m_document->isModified())) {
                if (savedId == m_draftId) {
                    m_draftTimer->stop();
                }
                setDraftError(m_drafts.remove(savedId));
            }
        });
        if (!m_vaultRoot.isEmpty()) {
            rescanVault();
        }
    });
    connect(m_document, &KParts::ReadOnlyPart::completed, this, [this] {
        const quint64 openRevision = m_openRevision;
        const quint64 modeRevision = m_modeRevision;
        QTimer::singleShot(0, this, [this, openRevision, modeRevision] {
            if (!m_opening && !m_document->isModified() && m_openRevision == openRevision) {
                const QString extension = QFileInfo(m_document->url().path()).suffix();
                const bool markdown =
                    QMimeDatabase().mimeTypeForFile(m_document->url().path(), QMimeDatabase::MatchExtension).inherits(QStringLiteral("text/markdown"))
                    || extension.compare(QLatin1String("md"), Qt::CaseInsensitive) == 0
                    || extension.compare(QLatin1String("markdown"), Qt::CaseInsensitive) == 0
                    || extension.compare(QLatin1String("mkd"), Qt::CaseInsensitive) == 0;
                const VaultSettings::Options options = vaultOptions();
                if (m_modeRevision == modeRevision) {
                    setVisualMode(markdown && options.visual);
                }
                if (markdown && options.preview) {
                    m_previewToggle->setChecked(true);
                }
                showDrafts();
            }
        });
    });
    connect(m_document, &KParts::ReadOnlyPart::canceled, this, [this](const QString &error) {
        if (!error.isEmpty()) {
            KMessageBox::error(this, i18nc("@info", "Could not open the file: %1", error));
        }
    });
    connect(review, &QPushButton::clicked, this, [this] {
        const int index = m_draftChoice->currentIndex();
        if (index < 0 || index >= m_availableDrafts.size()) {
            return;
        }
        QDialog dialog(this);
        dialog.setWindowTitle(i18nc("@title:window", "Review Draft"));
        dialog.resize(700, 500);
        auto *dialogLayout = new QVBoxLayout(&dialog);
        auto *text = new QPlainTextEdit(&dialog);
        text->setReadOnly(true);
        text->setPlainText(m_availableDrafts.at(index).text);
        dialogLayout->addWidget(text);
        dialog.exec();
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        if (!syncVisual()) {
            return;
        }
        const int index = m_draftChoice->currentIndex();
        if (index < 0 || index >= m_availableDrafts.size()) {
            return;
        }
        if (m_document->isModified()
            && KMessageBox::warningContinueCancel(this,
                                                  i18nc("@info", "Replace the current unsaved editor contents with this draft?"),
                                                  i18nc("@title:window", "Restore Draft"))
                != KMessageBox::Continue) {
            return;
        }
        if (m_document->setText(m_availableDrafts.at(index).text)) {
            m_recovery->hide();
        } else {
            KMessageBox::error(this, i18nc("@info", "Could not restore the draft into the editor."));
        }
    });
    connect(discard, &QPushButton::clicked, this, [this] {
        const int index = m_draftChoice->currentIndex();
        if (index < 0 || index >= m_availableDrafts.size()) {
            return;
        }
        const auto &draft = m_availableDrafts.at(index);
        const QString error = m_drafts.remove(draft.id, &draft);
        setDraftError(error);
        if (error.isEmpty()) {
            showDrafts();
        }
    });
    QTimer::singleShot(0, this, [this] {
        if (m_document->url().isEmpty() && !m_document->isModified()) {
            showDrafts();
        }
    });
    updateTitle();
    applySettings();
    setVisualMode(false);
    s_editorPanels.append(this);
}

EditorPanel::~EditorPanel()
{
    s_editorPanels.removeAll(this);
    if (m_vaultCanceled) {
        m_vaultCanceled->store(true);
    }
}

bool EditorPanel::openUrl(const QUrl &url)
{
    if (!syncVisual()) {
        return false;
    }
    if (url == m_document->url()) {
        return true;
    }
    if (m_document->isModified() && !saveDraft()) {
        KMessageBox::error(this, m_draftStatus->text());
        return false;
    }
    const QUrl previousUrl = m_document->url();
    const quint64 previousRevision = m_openRevision;
    ++m_openRevision;
    m_opening = true;
    const bool opened = m_document->openUrl(url);
    m_opening = false;
    if (opened || m_document->url() != previousUrl) {
        m_draftTimer->stop();
        m_drafts.release(m_draftId);
        m_draftId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_recovery->hide();
        setVisualMode(false);
    } else {
        m_openRevision = previousRevision;
    }
    return opened;
}

bool EditorPanel::queryClose()
{
    if (!syncVisual()) {
        return false;
    }
    if (m_document->isModified() && !saveDraft()) {
        KMessageBox::error(this, m_draftStatus->text());
        return false;
    }
    const bool modified = m_document->isModified();
    if (!m_document->queryClose()) {
        return false;
    }
    // waitSaveComplete() reports the last save, so only consult it when the
    // user chose to save; discarding leaves the document modified.
    return !modified || m_document->isModified() || m_document->waitSaveComplete();
}

bool EditorPanel::closeDocument()
{
    if (!queryClose()) {
        return false;
    }
    discardDocument();
    return true;
}

void EditorPanel::discardDocument()
{
    m_draftTimer->stop();
    if (m_document->isModified()) {
        setDraftError(m_drafts.remove(m_draftId));
        // Without this, closeUrl() would ask to save the changes again.
        m_document->setModified(false);
    }
    m_visualPending = false;
    m_visualEditing = false;
    ++m_openRevision;
    m_document->closeUrl();
    m_drafts.release(m_draftId);
    m_draftId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_recovery->hide();
    setVisualMode(false);
    updateTitle();
}

void EditorPanel::setEmbedded(bool embedded)
{
    m_openButton->setVisible(!embedded);
    m_placementButton->setVisible(!embedded);
    m_hideButton->setVisible(!embedded);
    setPanelTitleVisible(!embedded);
    setSizePolicy(embedded ? QSizePolicy::Ignored : QSizePolicy::Preferred, sizePolicy().verticalPolicy());
}

KTextEditor::Document *EditorPanel::document() const
{
    return m_document;
}

void EditorPanel::setVault(const QString &root)
{
    VaultSettings::setVaultFolder(root);
    const QString folder = VaultSettings::vaultFolder();
    for (EditorPanel *panel : std::as_const(s_editorPanels)) {
        panel->m_ownVaultRoot = folder;
        panel->applyVault();
    }
}

void EditorPanel::setWorkspaceVault(const QString &root)
{
    setWorkspaceVaultState({root, {}, std::nullopt});
}

QString EditorPanel::workspaceVault()
{
    return s_workspaceVault.root;
}

void EditorPanel::setWorkspaceVaultState(const WorkspaceVault &vault)
{
    WorkspaceVault clean = vault;
    clean.root = cleanVaultRoot(vault.root);
    if (!clean.isActive()) {
        clean = {};
    }
    if (clean == s_workspaceVault) {
        return;
    }
    s_workspaceVault = clean;
    for (EditorPanel *panel : std::as_const(s_editorPanels)) {
        // The workspace's options may change the dialect as well as the vault.
        panel->applyMarkdownSettings();
    }
}

EditorPanel::WorkspaceVault EditorPanel::workspaceVaultState()
{
    return s_workspaceVault;
}

void EditorPanel::applyVault()
{
    const QString root = s_workspaceVault.isActive() ? s_workspaceVault.root : m_ownVaultRoot;
    if (root == m_vaultRoot) {
        updateVaultPanel();
        return;
    }
    m_vaultRoot = root;
    m_vaultIndex = {};
    rescanVault();
}

QString EditorPanel::vaultRoot() const
{
    return m_vaultRoot;
}

const NoteVault::Index &EditorPanel::vaultIndex() const
{
    return m_vaultIndex;
}

VaultSettings::Options EditorPanel::vaultOptions() const
{
    if (s_workspaceVault.isActive() && s_workspaceVault.options) {
        return *s_workspaceVault.options;
    }
    return VaultSettings::general();
}

void EditorPanel::rescanVault()
{
    if (m_vaultCanceled) {
        m_vaultCanceled->store(true);
        m_vaultCanceled.reset();
    }
    const quint64 generation = ++m_vaultGeneration;
    m_vaultScanning = !m_vaultRoot.isEmpty();
    if (!m_vaultScanning) {
        m_vaultIndex = {};
        updateVaultPanel();
        refreshPreview();
        return;
    }
    const auto canceled = std::make_shared<std::atomic_bool>(false);
    m_vaultCanceled = canceled;
    auto *watcher = new QFutureWatcher<NoteVault::Index>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != m_vaultGeneration) {
            return;
        }
        m_vaultIndex = watcher->result();
        m_vaultScanning = false;
        m_vaultCanceled.reset();
        updateVaultPanel();
        refreshPreview();
        Q_EMIT vaultScanned();
    });
    const QString root = m_vaultRoot;
    watcher->setFuture(QtConcurrent::run([root, canceled] {
        return NoteVault::scan(root, NoteVault::DefaultFileLimit, [canceled] {
            return canceled->load();
        });
    }));
    updateVaultPanel();
}

QString EditorPanel::currentNotePath() const
{
    const QUrl url = m_document->url();
    if (m_vaultIndex.root.isEmpty() || !url.isLocalFile()) {
        return {};
    }
    const QString path = QFileInfo(url.toLocalFile()).canonicalFilePath();
    return NoteVault::isInside(m_vaultIndex.root, path) ? path : QString();
}

void EditorPanel::refreshPreview()
{
    if (!m_previewToggle->isChecked()) {
        return;
    }
    const auto features = m_dialect->currentIndex() == 1 ? QTextDocument::MarkdownDialectCommonMark : QTextDocument::MarkdownDialectGitHub;
    QString text = m_document->text();
    if (vaultOptions().wikiLinks && !currentNotePath().isEmpty()) {
        // The source is unchanged; only the preview's copy gets clickable links.
        text = WikiLinks::toMarkdownLinks(text, [](const WikiLinks::Link &link) {
            QUrl url;
            if (!link.target.isEmpty()) {
                url.setScheme(QString(WikiLinkScheme));
                url.setPath(link.target);
            }
            return url;
        });
    }
    m_preview->document()->setMarkdown(text, features);
}

void EditorPanel::openWikiLink(const QUrl &url)
{
    if (url.scheme() != WikiLinkScheme) {
        return;
    }
    const QString from = currentNotePath();
    const QString target = url.path();
    if (from.isEmpty()) {
        return;
    }
    if (!vaultOptions().wikiLinks) {
        m_vaultStatus->setText(i18nc("@info", "Following wiki links is turned off in Preferences › Applications › Editor."));
        return;
    }
    const NoteVault::Resolution resolution = NoteVault::resolve(m_vaultIndex, from, target);
    switch (resolution.status) {
    case NoteVault::Status::Found:
        if (resolution.path == from) {
            return;
        }
        if (NoteVault::isNoteFileName(resolution.path)) {
            openUrl(QUrl::fromLocalFile(resolution.path));
        } else {
            auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(resolution.path));
            job->setRunExecutables(false);
            job->start();
        }
        return;
    case NoteVault::Status::Missing:
        m_vaultStatus->setText(m_vaultScanning || !m_vaultIndex.isComplete() ? i18nc("@info", "“%1” was not found, but the vault index is incomplete.", target)
                                                                             : i18nc("@info", "No note named “%1” exists in the vault.", target));
        return;
    case NoteVault::Status::Ambiguous: {
        QStringList names;
        for (const QString &candidate : resolution.candidates) {
            names.append(QDir(m_vaultIndex.root).relativeFilePath(candidate));
        }
        m_vaultStatus->setText(i18nc("@info %1 is a note name, %2 a list of paths",
                                     "Several notes are named “%1”: %2. Link to one by its path.",
                                     target,
                                     names.join(QStringLiteral(", "))));
        return;
    }
    case NoteVault::Status::Outside:
        m_vaultStatus->setText(i18nc("@info", "“%1” is outside the vault and was not opened.", target));
        return;
    }
}

void EditorPanel::updateVaultPanel()
{
    const bool hasVault = !m_vaultRoot.isEmpty();
    const bool workspace = s_workspaceVault.isActive();
    // A workspace vault that can't be found stays in view to say why.
    m_vaultPanel->setVisible(hasVault || workspace);
    m_vaultRescan->setEnabled(hasVault);
    // The workspace decides the vault while it has one; the panel's own choice returns afterwards.
    m_vaultChoose->setEnabled(!workspace);
    m_vaultClose->setEnabled(hasVault && !workspace);
    m_vaultButton->setToolTip(workspace ? i18nc("@info:tooltip", "The active workspace uses %1 as the vault for wiki links, backlinks and tags", m_vaultRoot)
                                        : i18nc("@info:tooltip", "Choose a folder of Markdown notes for wiki links, backlinks and tags"));
    if (workspace && !hasVault) {
        m_vaultButton->setToolTip(i18nc("@info:tooltip", "The active workspace's vault folder can't be found"));
        m_vaultTitle->setText(i18nc("@info", "Workspace vault unavailable"));
    } else {
        m_vaultTitle->setText(workspace ? i18nc("@info %1 is a folder name", "Workspace vault: %1", QFileInfo(m_vaultRoot).fileName())
                                        : i18nc("@info %1 is a folder name", "Vault: %1", QFileInfo(m_vaultRoot).fileName()));
    }
    m_vaultTitle->setToolTip(m_vaultRoot);
    m_backlinks->clear();
    m_outgoingLinks->clear();
    m_tags->clear();
    const VaultSettings::Options options = vaultOptions();
    m_vaultTabs->setTabVisible(m_vaultTabs->indexOf(m_backlinks), options.backlinks);
    m_vaultTabs->setTabVisible(m_vaultTabs->indexOf(m_outgoingLinks), options.links);
    m_vaultTabs->setTabVisible(m_vaultTabs->indexOf(m_tags), options.tags);
    m_vaultTabs->setVisible(hasVault && (options.backlinks || options.links || options.tags));
    if (!hasVault) {
        if (workspace) {
            m_vaultStatus->setText(s_workspaceVault.unavailable);
        } else {
            m_vaultStatus->clear();
        }
        return;
    }
    if (m_vaultIndex.root.isEmpty()) {
        const QString reason = VaultSettings::unavailableReason(m_vaultRoot);
        m_vaultStatus->setText(m_vaultScanning ? i18nc("@info", "Scanning the vault…")
                                               : (reason.isEmpty() ? i18nc("@info", "The vault folder %1 can't be read.", m_vaultRoot) : reason));
        return;
    }
    const QString note = currentNotePath();
    if (note.isEmpty()) {
        m_vaultStatus->setText(i18nc("@info", "Open a note in the vault to see its backlinks."));
        return;
    }
    updateVaultLinks(note);
    updateVaultTags(note);
    const QDir root(m_vaultIndex.root);
    const QList<NoteVault::Backlink> backlinks = NoteVault::backlinks(m_vaultIndex, note);
    for (const NoteVault::Backlink &backlink : backlinks) {
        auto *item = new QListWidgetItem(i18nc("@item:inlistbox %1 is a note name, %2 the line that links here",
                                               "%1: %2",
                                               QFileInfo(backlink.path).completeBaseName(),
                                               backlink.link.context),
                                         m_backlinks);
        item->setToolTip(i18nc("@info:tooltip %1 is a path, %2 a line number", "%1, line %2", root.relativeFilePath(backlink.path), backlink.link.line + 1));
        item->setData(Qt::UserRole, backlink.path);
    }

    QStringList status;
    if (options.backlinks && backlinks.isEmpty()) {
        status.append(i18nc("@info", "No other notes link here."));
    }
    int missing = 0;
    int ambiguous = 0;
    for (const WikiLinks::Link &link : WikiLinks::parse(m_document->text())) {
        const NoteVault::Status linkStatus = NoteVault::resolve(m_vaultIndex, note, link.target).status;
        missing += linkStatus == NoteVault::Status::Missing || linkStatus == NoteVault::Status::Outside;
        ambiguous += linkStatus == NoteVault::Status::Ambiguous;
    }
    if (missing > 0) {
        status.append(i18ncp("@info", "%1 link doesn't lead to a note.", "%1 links don't lead to a note.", missing));
    }
    if (ambiguous > 0) {
        status.append(i18ncp("@info", "%1 link matches several notes.", "%1 links match several notes.", ambiguous));
    }
    if (m_vaultIndex.truncated) {
        status.append(i18nc("@info", "Only the first %1 notes were indexed, so backlinks may be missing.", m_vaultIndex.notes.size()));
    }
    if (!m_vaultIndex.skipped.isEmpty()) {
        status.append(i18ncp("@info", "%1 note couldn't be read.", "%1 notes couldn't be read.", m_vaultIndex.skipped.size()));
    }
    if (m_vaultScanning) {
        status.append(i18nc("@info", "Rescanning…"));
    }
    m_vaultStatus->setText(status.join(QLatin1Char(' ')));
}

void EditorPanel::updateVaultLinks(const QString &note)
{
    const QDir root(m_vaultIndex.root);
    QSet<QString> seen;
    for (const WikiLinks::Link &link : WikiLinks::parse(m_document->text())) {
        if (link.target.isEmpty() || seen.contains(link.target.toCaseFolded())) {
            continue;
        }
        seen.insert(link.target.toCaseFolded());
        const NoteVault::Resolution resolution = NoteVault::resolve(m_vaultIndex, note, link.target);
        QString state;
        switch (resolution.status) {
        case NoteVault::Status::Found:
            state = NoteVault::isNoteFileName(resolution.path) ? root.relativeFilePath(resolution.path)
                                                               : i18nc("@item:inlistbox %1 is a path", "attachment %1", root.relativeFilePath(resolution.path));
            break;
        case NoteVault::Status::Missing:
            state = i18nc("@item:inlistbox link state", "not found");
            break;
        case NoteVault::Status::Ambiguous:
            state = i18ncp("@item:inlistbox link state", "matches %1 note", "matches %1 notes", resolution.candidates.size());
            break;
        case NoteVault::Status::Outside:
            state = i18nc("@item:inlistbox link state", "outside the vault");
            break;
        }
        auto *item = new QListWidgetItem(i18nc("@item:inlistbox %1 is a link target, %2 where it leads", "%1 — %2", link.target, state), m_outgoingLinks);
        item->setToolTip(link.context);
        QUrl url;
        url.setScheme(QString(WikiLinkScheme));
        url.setPath(link.target);
        item->setData(Qt::UserRole, url);
        item->setData(Qt::UserRole + 1, int(resolution.status));
    }
}

void EditorPanel::updateVaultTags(const QString &note)
{
    const QDir root(m_vaultIndex.root);
    // The open text decides this note's tags, so unsaved edits show at once.
    for (const QString &tag : WikiLinks::tags(m_document->text())) {
        QStringList others = NoteVault::tagged(m_vaultIndex, tag);
        others.removeAll(note);
        auto *item = new QTreeWidgetItem(m_tags);
        item->setText(0, i18ncp("@item:inlistbox %2 is a tag", "#%2, %1 other note", "#%2, %1 other notes", others.size(), tag));
        item->setData(0, Qt::UserRole + 1, tag);
        for (const QString &path : std::as_const(others)) {
            auto *child = new QTreeWidgetItem(item);
            child->setText(0, QFileInfo(path).completeBaseName());
            child->setToolTip(0, root.relativeFilePath(path));
            child->setData(0, Qt::UserRole, path);
        }
    }
    m_tags->expandAll();
}

void EditorPanel::updateTitle()
{
    m_title->setText(m_document->documentName() + (m_document->isModified() ? QStringLiteral(" *") : QString()));
}

void EditorPanel::setPanelTitleVisible(bool visible)
{
    if (m_panelTitleVisible == visible) {
        return;
    }
    m_panelTitleVisible = visible;
    if (visible) {
        m_header->insertWidget(0, m_title, 1);
        m_documentStatus->hide();
    } else {
        m_documentStatusLayout->addWidget(m_title, 1);
        m_documentStatus->show();
    }
    m_title->show();
}

void EditorPanel::setVisualMode(bool visual)
{
    if (!visual && !syncVisual()) {
        updateVisualControls();
        return;
    }
    if (visual && m_editorStack->currentWidget() != m_visual) {
        m_visualEditing = false;
        refreshVisual();
    } else if (!visual) {
        m_visualEditing = false;
    }
    m_editorStack->setCurrentWidget(visual ? static_cast<QWidget *>(m_visual) : static_cast<QWidget *>(m_view));
    updateVisualControls();
}

void EditorPanel::refreshVisual()
{
    m_syncingVisual = true;
    m_visual->setMarkdown(m_document->text());
    m_syncingVisual = false;
}

bool EditorPanel::syncVisual()
{
    if (!m_visualPending) {
        return true;
    }
    m_syncingVisual = true;
    const bool saved = m_document->setText(m_visual->document()->toMarkdown());
    m_syncingVisual = false;
    if (saved) {
        m_visualPending = false;
    }
    updateVisualControls();
    return saved;
}

void EditorPanel::updateVisualControls()
{
    const bool visual = m_editorStack->currentWidget() == m_visual;
    m_sourceMode->setChecked(!visual);
    m_visualMode->setChecked(visual);
    m_visualToolbar->setVisible(visual);
    m_visual->setReadOnly(!m_visualEditing || m_visualPending);
    m_visualEnable->setVisible(!m_visualEditing && !m_visualPending);
    m_visualRetry->setVisible(m_visualPending);
    m_visualDiscard->setVisible(m_visualPending);
    m_visualToolbar->findChild<QPushButton *>(QStringLiteral("editorVisualBold"))->setEnabled(m_visualEditing && !m_visualPending);
    m_visualToolbar->findChild<QPushButton *>(QStringLiteral("editorVisualItalic"))->setEnabled(m_visualEditing && !m_visualPending);
    m_visualStatus->setText(m_visualPending ? i18nc("@info", "Visual edits could not be synchronized. Copy them, retry, or discard them before leaving.")
                                            : i18nc("@info", "Visual editing can rewrite or remove unsupported Markdown."));
}

bool EditorPanel::saveDraft()
{
    if (!Settings::editorDraftsEnabled() || !m_document->isModified()) {
        return true;
    }
    const QString error = m_drafts.write({m_draftId, m_document->text(), m_document->url(), QDateTime::currentMSecsSinceEpoch()});
    setDraftError(error);
    return error.isEmpty();
}

void EditorPanel::applySettings()
{
    QFont font = m_view->configValue(QStringLiteral("font")).value<QFont>();
    font.setPointSize(Settings::editorFontSize());
    m_view->setConfigValue(QStringLiteral("font"), font);
    m_view->setConfigValue(QStringLiteral("dynamic-word-wrap"), Settings::editorWordWrap());
    m_syncingVisual = true;
    // A fixed-pitch default font makes QTextDocument::toMarkdown() write every
    // fragment as inline code, so the visual editor uses the general font.
    QFont visualFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    visualFont.setPointSize(Settings::editorFontSize());
    m_visual->document()->setDefaultFont(visualFont);
    m_visual->setLineWrapMode(Settings::editorWordWrap() ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    m_syncingVisual = false;
    m_draftTimer->setInterval(Settings::editorDraftIntervalMs());
    const QString retentionError = m_drafts.prune(Settings::editorDraftRetentionDays(), QDateTime::currentMSecsSinceEpoch());
    if (!retentionError.isEmpty()) {
        setDraftError(retentionError);
    }
    if (!Settings::editorDraftsEnabled()) {
        m_draftTimer->stop();
    } else if (m_document->isModified()) {
        m_draftTimer->start();
    }
    applyMarkdownSettings();
}

void EditorPanel::applyMarkdownSettings()
{
    // A dialect chosen in the panel lasts until the setting changes.
    const VaultSettings::Dialect dialect = vaultOptions().dialect;
    if (dialect != m_appliedDialect) {
        m_appliedDialect = dialect;
        m_dialect->setCurrentIndex(dialect == VaultSettings::Dialect::CommonMark ? 1 : 0);
    }
    m_ownVaultRoot = VaultSettings::vaultFolder();
    const QString previous = m_vaultRoot;
    applyVault();
    // A folder that couldn't be read before, such as one on a drive connected since, is read again.
    if (previous == m_vaultRoot && !m_vaultRoot.isEmpty() && !m_vaultScanning && m_vaultIndex.root.isEmpty()) {
        rescanVault();
    }
    refreshPreview();
}

void EditorPanel::setDraftError(const QString &error)
{
    m_draftStatus->setText(error.isEmpty() ? QString() : i18nc("@info", "Draft error: %1", error));
    m_draftStatus->setVisible(!error.isEmpty());
}

void EditorPanel::showDrafts()
{
    QString error = m_drafts.prune(Settings::editorDraftRetentionDays(), QDateTime::currentMSecsSinceEpoch());
    QString readError;
    m_availableDrafts = m_drafts.drafts(m_document->url(), &readError);
    if (error.isEmpty()) {
        error = readError;
    }
    setDraftError(error);
    m_draftChoice->clear();
    for (const auto &draft : std::as_const(m_availableDrafts)) {
        m_draftChoice->addItem(LocalizedDates::dateTime(QDateTime::fromMSecsSinceEpoch(draft.timestamp)));
    }
    m_recovery->setVisible(!m_availableDrafts.isEmpty());
}
