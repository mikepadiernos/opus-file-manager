// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "imagedetails.h"
#include "previewcontent.h"

#include <QDateTime>
#include <QDialog>
#include <QImage>
#include <QList>
#include <QQueue>
#include <QSet>
#include <QUrl>

#include <functional>

class QAction;
class QGraphicsPixmapItem;
class QGraphicsScene;
class GalleryView;
class QCloseEvent;
class QLabel;
class QListWidget;
class MediaView;
class QMovie;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;

class GalleryWindow : public QDialog
{
    Q_OBJECT

public:
    explicit GalleryWindow(const QList<QUrl> &urls, const QSet<QUrl> &videos, int initialIndex, QWidget *parent = nullptr);
    ~GalleryWindow() override;
    void applyPresentationSettings();

Q_SIGNALS:
    void currentUrlChanged(const QUrl &url);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void reject() override;

private:
    enum class MarkedOperation {
        Copy,
        Move,
        Trash,
        Delete
    };
    QList<QUrl> markedImages() const;
    void operateOnMarked(MarkedOperation operation);
    void refreshAfterOperation(const QList<QUrl> &sources);
    void showIndex(int index);
    void startImageLoad();
    void showImage(const PreviewContent &content);
    void showVideo(const QUrl &url);
    void clearPlayback();
    void startThumbnailLoad();
    void queueVisibleThumbnails();
    void scheduleDetails();
    void startDetails();
    void showDetails(const ImageDetails::Data &details);
    void fitImage();
    void zoom(qreal factor);
    void toggleSlideshow();
    void updateNavigation();
    void replaceImage(const QImage &image);
    void startEdit(std::function<QImage(const QImage &)> operation);
    void resizeImage();
    void adjustImage();
    void discardEdits();
    void replaceOriginal();
    bool saveImage(const QString &path, const QByteArray &format, int quality = -1);
    void saveCopy();
    bool confirmDiscard();

    QList<QUrl> m_urls;
    QSet<QUrl> m_videos;
    int m_index = -1;
    int m_pendingIndex = -1;
    quint64 m_generation = 0;
    quint64 m_thumbnailGeneration = 0;
    bool m_loading = false;
    QQueue<int> m_thumbnailQueue;
    bool m_loadingThumbnail = false;
    QSet<int> m_thumbnailQueued;
    QSet<int> m_thumbnailDone;
    QImage m_image;
    QMovie *m_movie = nullptr;
    MediaView *m_media = nullptr;
    bool m_modified = false;
    bool m_edited = false;
    bool m_editing = false;
    bool m_initialActualZoom = false;
    bool m_operationRunning = false;
    bool m_detailsLoading = false;
    bool m_detailsPending = false;
    quint64 m_detailsGeneration = 0;
    QString m_baseInfo;
    qint64 m_originalSize = -1;
    QDateTime m_originalModified;
    QLabel *m_title = nullptr;
    QLabel *m_message = nullptr;
    QLabel *m_info = nullptr;
    QWidget *m_infoPanel = nullptr;
    QWidget *m_statsPanel = nullptr;
    QLabel *m_colours = nullptr;
    QLabel *m_histogram = nullptr;
    QGraphicsScene *m_scene = nullptr;
    GalleryView *m_view = nullptr;
    QGraphicsPixmapItem *m_pixmap = nullptr;
    QStackedWidget *m_stage = nullptr;
    QListWidget *m_filmstrip = nullptr;
    QPushButton *m_previous = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_slideshow = nullptr;
    QPushButton *m_rotate = nullptr;
    QPushButton *m_flip = nullptr;
    QPushButton *m_save = nullptr;
    QToolButton *m_editsButton = nullptr;
    QToolButton *m_markedButton = nullptr;
    QAction *m_cropAction = nullptr;
    QTimer *m_timer = nullptr;
};
