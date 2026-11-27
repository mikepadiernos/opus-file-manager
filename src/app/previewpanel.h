// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "previewcontent.h"

#include <KFileItem>
#include <QSharedPointer>
#include <QWidget>

#include <functional>

namespace KIO
{
class TransferJob;
}
class EditorPanel;
class FolderPreview;
class FontPreview;
class MailPreview;
class QLabel;
class QActionGroup;
class QHBoxLayout;
class MediaView;
class PreviewTextEdit;
class QStackedWidget;
class QTemporaryFile;
class QTextBrowser;
class QToolButton;
namespace KParts
{
class ReadOnlyPart;
}

class PreviewPanel : public QWidget
{
    Q_OBJECT

public:
    enum class Placement {
        Pane,
        Below,
        Right
    };
    explicit PreviewPanel(QWidget *parent = nullptr);
    ~PreviewPanel() override;
    void setItems(const KFileItemList &items, const QList<QUrl> &images = {});
    void applyPresentationSettings();
    void clear();
    void setPlacement(Placement placement);
    void setTitleVisible(bool visible);
    /**
     * Sets the presentation for embedding the panel in a narrow column: it
     * hides the close, placement, gallery, image navigation and open buttons,
     * whose signals the host does not handle, and drops the minimum width.
     */
    void setCompact(bool compact);
    bool isCompact() const;
    /**
     * Shows the folder @p item: its contents, folders first, in a read-only
     * list of at most FolderPreview::EntryLimit entries, listed
     * asynchronously, with its type and modification time below. Pending
     * previews and listings are cancelled.
     */
    void showFolder(const KFileItem &item);
    /// Whether showFolder() lists hidden files.
    void setShowHiddenFiles(bool show);
    /**
     * Whether a preview or folder listing is loading, while the panel shows
     * "Loading preview…". Clearing the panel or showing another item stops
     * the work.
     */
    bool isBusy() const;
    /**
     * Whether a text preview is switched to the editor. Unsaved changes keep
     * the file open when the selection changes; the new selection is shown
     * after the user saves, closes the editor or chooses Preview Selection.
     */
    bool isEditing() const;
    /**
     * Lets the user save or discard unsaved changes in the editor, then
     * returns to the preview. Returns false if the user keeps editing. Call
     * it before hiding the panel or quitting.
     */
    bool queryClose();
    /**
     * Leaves the editor without asking, discarding unsaved changes and their
     * draft, after the editor panel has taken over the session.
     */
    void endEditing();

    /**
     * Asks the host for a server-rendered preview of the remote @p item, wanted
     * at @p size device pixels, before the panel downloads the file. Returns
     * true when the host will answer with showServerPreview().
     */
    using ServerPreviewHandler = std::function<bool(const KFileItem &item, const QSize &size)>;
    /// Lets the host offer server-rendered previews of remote files other than text.
    void setServerPreviewHandler(ServerPreviewHandler handler);
    /**
     * Shows @p image, the server's preview of @p url, with a button that
     * downloads the whole file instead. A null @p image downloads the file
     * within the usual limits, as without a server preview. Answers for
     * items no longer shown are ignored.
     */
    void showServerPreview(const QUrl &url, const QImage &image);
    /// Whether the panel shows a server's preview instead of the file.
    bool isShowingServerPreview() const;

Q_SIGNALS:
    void busyChanged(bool busy);
    void closeRequested();
    void galleryRequested();
    void openRequested();
    void imageRequested(const QUrl &url);
    void placementRequested(Placement placement);
    /**
     * Asks the host to continue editing @p url in the editor panel, with
     * @p text when @p modified. The host calls endEditing() to take over the
     * session.
     */
    void editorRequested(const QUrl &url, const QString &text, bool modified);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void showMessage(const QString &message);
    void setBusy(bool busy);
    void showContent(const PreviewContent &content, const QString &mimeType);
    void updateTextMode();
    void scheduleMetadata(const QUrl &url, const QString &mimeType, const QSharedPointer<QTemporaryFile> &file = {});
    void startMetadata();
    void updateFooter();
    void startPendingPreview();
    void startRemotePreview(const QUrl &url);
    void downloadFullPreview();
    void finishRemotePreview();
    void clearRemotePreview();
    void showDocument(const QUrl &url, const QString &mimeType);
    void clearDocument();
    void showFont(const QUrl &url, const QUrl &source, const QString &mimeType);
    void showMail(const QUrl &url);
    void showMedia(const QUrl &url, bool video);
    void clearMedia();
    void updateImage();
    void startEditing();
    void stopEditing();
    void updateEditControls();
    void showEditNotice(const QString &message, bool pendingSelection = false);

    QLabel *m_title = nullptr;
    QLabel *m_message = nullptr;
    QLabel *m_imageLabel = nullptr;
    QLabel *m_footer = nullptr;
    QHBoxLayout *m_header = nullptr;
    QHBoxLayout *m_footerLayout = nullptr;
    bool m_titleVisible = true;
    bool m_compact = false;
    QToolButton *m_close = nullptr;
    QToolButton *m_placement = nullptr;
    QToolButton *m_previous = nullptr;
    QToolButton *m_next = nullptr;
    QToolButton *m_gallery = nullptr;
    QToolButton *m_open = nullptr;
    QToolButton *m_renderMode = nullptr;
    QToolButton *m_autoplay = nullptr;
    QToolButton *m_edit = nullptr;
    QToolButton *m_editMove = nullptr;
    QToolButton *m_editClose = nullptr;
    QWidget *m_editNotice = nullptr;
    QLabel *m_editNoticeText = nullptr;
    QToolButton *m_editShowSelection = nullptr;
    EditorPanel *m_editor = nullptr;
    bool m_editing = false;
    KFileItem m_item;
    bool m_hasPendingItems = false;
    KFileItemList m_pendingItems;
    QList<QUrl> m_pendingImages;
    QActionGroup *m_placementActions = nullptr;
    PreviewTextEdit *m_text = nullptr;
    QTextBrowser *m_renderedText = nullptr;
    QStackedWidget *m_content = nullptr;
    KParts::ReadOnlyPart *m_document = nullptr;
    FontPreview *m_font = nullptr;
    MailPreview *m_mail = nullptr;
    MediaView *m_media = nullptr;
    QImage m_image;
    QUrl m_url;
    qint64 m_textLimit = 0;
    /// The document formats previewed when the preview was loaded, one bit per DocumentFormats::Kind.
    quint32 m_formatPreviews = 0;
    QUrl m_pendingUrl;
    QString m_pendingMimeType;
    QSharedPointer<QTemporaryFile> m_pendingFile;
    QSharedPointer<QTemporaryFile> m_remoteFile;
    QSharedPointer<QTemporaryFile> m_renderFile;
    KIO::TransferJob *m_remoteJob = nullptr;
    QString m_remoteMimeType;
    QString m_baseFooter;
    QStringList m_metadataLabels;
    QUrl m_metadataUrl;
    QString m_metadataMimeType;
    QSharedPointer<QTemporaryFile> m_metadataFile;
    bool m_metadataLoading = false;
    bool m_truncated = false;
    bool m_longLines = false;
    bool m_preferFormatted = true;
    bool m_showLineNumbers = true;
    bool m_syntaxHighlighting = true;
    /// Whether text previews offer editing, start it on a double-click, and return to the preview after saving.
    bool m_editingOffered = true;
    bool m_editOnDoubleClick = false;
    bool m_returnAfterSave = true;
    QString m_textMimeType;
    QString m_textContent;
    QList<QUrl> m_images;
    quint64 m_generation = 0;
    bool m_loading = false;
    FolderPreview *m_folder = nullptr;
    bool m_showHiddenFiles = false;
    bool m_busy = false;
    ServerPreviewHandler m_serverPreviewHandler;
    /// Whether the host was asked for a server preview of m_url and hasn't answered.
    bool m_awaitingServerPreview = false;
    bool m_serverPreview = false;
    QToolButton *m_downloadFull = nullptr;
};
