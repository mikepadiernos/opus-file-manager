// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorpanel.h"
#include "markdownexport.h"
#include "safetextdocument.h"

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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBrowser>
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
    auto *header = new QHBoxLayout;
    header->addWidget(m_title, 1);
    header->addWidget(open);
    header->addWidget(save);
    header->addWidget(saveAs);
    header->addWidget(preview);
    header->addWidget(pdf);
    header->addWidget(placement);
    header->addWidget(hide);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(header);
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
    auto *splitter = new QSplitter(this);
    splitter->addWidget(m_document->createView(splitter));
    m_preview = new QTextBrowser(splitter);
    m_preview->setObjectName(QStringLiteral("editorRenderedMarkdown"));
    m_preview->setOpenLinks(false);
    m_preview->setOpenExternalLinks(false);
    m_preview->setDocument(new SafeTextDocument(m_preview));
    splitter->addWidget(m_preview);
    m_preview->hide();
    layout->addWidget(splitter, 1);
    connect(open, &QPushButton::clicked, this, [this] {
        const QUrl url = QFileDialog::getOpenFileUrl(this, i18nc("@title:window", "Open Text File"));
        if (!url.isEmpty()) {
            openUrl(url);
        }
    });
    connect(save, &QPushButton::clicked, m_document, &KTextEditor::Document::documentSave);
    connect(saveAs, &QPushButton::clicked, m_document, &KTextEditor::Document::documentSaveAs);
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
        if (!m_opening && m_document->isModified()) {
            m_draftTimer->start();
        }
    });
    connect(pdf, &QPushButton::clicked, this, [this] {
        const QFileInfo source(m_document->url().toLocalFile());
        const QString suggested =
            source.filePath().isEmpty() ? QStringLiteral("document.pdf") : source.dir().filePath(source.completeBaseName() + QStringLiteral(".pdf"));
        const QString destination =
            QFileDialog::getSaveFileName(this, i18nc("@title:window", "Export Markdown as PDF"), suggested, i18nc("@item:inlistbox", "PDF files (*.pdf)"));
        if (destination.isEmpty()) {
            return;
        }
        const QString error = MarkdownExport::toPdf(m_document->text(), m_document->url(), destination);
        if (!error.isEmpty()) {
            KMessageBox::error(this, i18nc("@info", "Could not export PDF: %1", error));
        }
    });
    connect(hide, &QPushButton::clicked, this, &EditorPanel::hideRequested);
    connect(m_document, &KTextEditor::Document::documentNameChanged, this, &EditorPanel::updateTitle);
    connect(m_document, &KTextEditor::Document::modifiedChanged, this, &EditorPanel::updateTitle);
    connect(m_document, &KTextEditor::Document::modifiedChanged, this, [this] {
        if (!m_opening && m_document->isModified()) {
            m_draftTimer->start();
        }
    });
    m_draftTimer->setSingleShot(true);
    m_draftTimer->setInterval(700);
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
        QTimer::singleShot(0, this, [this] {
            if (!m_opening && !m_document->isModified()) {
                showDrafts();
            }
        });
        connect(m_document, &KParts::ReadOnlyPart::canceled, this, [this](const QString &error) {
            if (!error.isEmpty()) {
                KMessageBox::error(this, i18nc("@info", "Could not open the file: %1", error));
            }
        });
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
        const QString error = m_drafts.remove(m_availableDrafts.at(index).id);
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
}

bool EditorPanel::openUrl(const QUrl &url)
{
    if (url == m_document->url()) {
        return true;
    }
    if (m_document->isModified() && !saveDraft()) {
        KMessageBox::error(this, m_draftStatus->text());
        return false;
    }
    const QUrl previousUrl = m_document->url();
    m_opening = true;
    const bool opened = m_document->openUrl(url);
    m_opening = false;
    if (opened || m_document->url() != previousUrl) {
        m_draftTimer->stop();
        m_draftId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_recovery->hide();
    }
    return opened;
}

bool EditorPanel::queryClose()
{
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

bool EditorPanel::saveDraft()
{
    if (!m_document->isModified()) {
        return true;
    }
    const QString error = m_drafts.write({m_draftId, m_document->text(), m_document->url(), QDateTime::currentMSecsSinceEpoch()});
    setDraftError(error);
    return error.isEmpty();
}

void EditorPanel::setDraftError(const QString &error)
{
    m_draftStatus->setText(error.isEmpty() ? QString() : i18nc("@info", "Draft error: %1", error));
    m_draftStatus->setVisible(!error.isEmpty());
}

void EditorPanel::showDrafts()
{
    QString error;
    m_availableDrafts = m_drafts.drafts(m_document->url(), &error);
    setDraftError(error);
    m_draftChoice->clear();
    for (const auto &draft : std::as_const(m_availableDrafts)) {
        m_draftChoice->addItem(QLocale().toString(QDateTime::fromMSecsSinceEpoch(draft.timestamp), QLocale::ShortFormat));
    }
    m_recovery->setVisible(!m_availableDrafts.isEmpty());
}
