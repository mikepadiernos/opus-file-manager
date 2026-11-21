// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "previewcontent.h"

#include <KFileItem>
#include <QSharedPointer>
#include <QWidget>

namespace KIO
{
class TransferJob;
}
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

Q_SIGNALS:
    void closeRequested();
    void galleryRequested();
    void openRequested();
    void imageRequested(const QUrl &url);
    void placementRequested(Placement placement);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void showMessage(const QString &message);
    void showContent(const PreviewContent &content, const QString &mimeType);
    void updateTextMode();
    void scheduleMetadata(const QUrl &url, const QString &mimeType, const QSharedPointer<QTemporaryFile> &file = {});
    void startMetadata();
    void updateFooter();
    void startPendingPreview();
    void startRemotePreview(const QUrl &url);
    void finishRemotePreview();
    void clearRemotePreview();
    void showDocument(const QUrl &url, const QString &mimeType);
    void clearDocument();
    void showMedia(const QUrl &url, bool video);
    void clearMedia();
    void updateImage();

    QLabel *m_title = nullptr;
    QLabel *m_message = nullptr;
    QLabel *m_imageLabel = nullptr;
    QLabel *m_footer = nullptr;
    QHBoxLayout *m_header = nullptr;
    QHBoxLayout *m_footerLayout = nullptr;
    bool m_titleVisible = true;
    QToolButton *m_previous = nullptr;
    QToolButton *m_next = nullptr;
    QToolButton *m_gallery = nullptr;
    QToolButton *m_open = nullptr;
    QToolButton *m_renderMode = nullptr;
    QToolButton *m_autoplay = nullptr;
    QActionGroup *m_placementActions = nullptr;
    PreviewTextEdit *m_text = nullptr;
    QTextBrowser *m_renderedText = nullptr;
    QStackedWidget *m_content = nullptr;
    KParts::ReadOnlyPart *m_document = nullptr;
    MediaView *m_media = nullptr;
    QImage m_image;
    QUrl m_url;
    qint64 m_textLimit = 0;
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
    QString m_textMimeType;
    QString m_textContent;
    QList<QUrl> m_images;
    quint64 m_generation = 0;
    bool m_loading = false;
};
