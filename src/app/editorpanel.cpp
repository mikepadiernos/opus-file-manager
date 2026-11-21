// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorpanel.h"
#include "localizeddates.h"
#include "markdownexport.h"
#include "safetextdocument.h"
#include "settings.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeDatabase>
#include <QPageSize>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <utility>

namespace
{
QString defaultDraftDirectory()
{
    const QString data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return data.isEmpty() ? QString() : QDir(data).filePath(QStringLiteral("drafts"));
}
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
    preview->setObjectName(QStringLiteral("editorMarkdownPreview"));
    preview->setCheckable(true);
    auto *pdf = new QPushButton(i18nc("@action", "Export PDF…"), this);
    pdf->setObjectName(QStringLiteral("editorExportPdf"));
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
    m_header = new QHBoxLayout;
    m_header->addWidget(m_title, 1);
    m_header->addWidget(open);
    m_header->addWidget(save);
    m_header->addWidget(saveAs);
    m_header->addWidget(m_sourceMode);
    m_header->addWidget(m_visualMode);
    m_header->addWidget(preview);
    m_header->addWidget(pdf);
    m_header->addWidget(placement);
    m_header->addWidget(hide);
    auto *layout = new QVBoxLayout(this);
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
    connect(preview, &QPushButton::toggled, this, [this](bool shown) {
        if (shown) {
            m_preview->setMarkdown(m_document->text());
        }
        m_preview->setVisible(shown);
    });
    connect(m_document, &KTextEditor::Document::textChanged, this, [this, preview] {
        ++m_editRevision;
        if (preview->isChecked()) {
            m_preview->setMarkdown(m_document->text());
        }
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
                if (m_modeRevision == modeRevision) {
                    setVisualMode(markdown && Settings::editorMarkdownMode() == Settings::EnumEditorMarkdownMode::Visual);
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
    return m_document->queryClose() && m_document->waitSaveComplete();
}

KTextEditor::Document *EditorPanel::document() const
{
    return m_document;
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
    m_visual->document()->setDefaultFont(font);
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
