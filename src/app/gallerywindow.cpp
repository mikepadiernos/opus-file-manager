// SPDX-License-Identifier: GPL-3.0-or-later

#include "gallerywindow.h"

#include "batchconversiondialog.h"
#include "fileoperations.h"
#include "imagebackground.h"
#include "imageediting.h"
#include "localizeddates.h"
#include "localizednumbers.h"
#include "mediaview.h"
#include "settings.h"
#include "symbolicicon.h"

#include <KFileMetaData/PropertyInfo>
#include <KIO/Global>
#include <KJob>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMimeDatabase>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrentRun>

#include <algorithm>
#include <utility>

class GalleryView : public QGraphicsView
{
public:
    using QGraphicsView::QGraphicsView;
    std::function<void(const QRect &)> cropFinished;
    std::function<void()> doubleClicked;

    void setCropping(bool cropping)
    {
        m_cropping = cropping;
        setDragMode(cropping ? QGraphicsView::RubberBandDrag : QGraphicsView::ScrollHandDrag);
        setCursor(cropping ? Qt::CrossCursor : Qt::ArrowCursor);
        setToolTip(cropping ? i18nc("@info:tooltip", "Drag over the image to select the crop area.") : QString());
        if (cropping) {
            setFocus();
        }
    }

    void zoom(qreal factor)
    {
        const qreal current = transform().m11();
        const qreal target = std::clamp(current * factor, 0.1, 32.0);
        scale(target / current, target / current);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (m_cropping && event->key() == Qt::Key_Escape && cropFinished) {
            cropFinished({});
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (!m_cropping && event->button() == Qt::LeftButton && doubleClicked) {
            doubleClicked();
            event->accept();
            return;
        }
        QGraphicsView::mouseDoubleClickEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (m_cropping && event->button() == Qt::LeftButton) {
            m_cropStart = event->pos();
        }
        QGraphicsView::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_cropping && event->button() == Qt::LeftButton) {
            const QRect viewportRectangle = QRect(m_cropStart, event->pos()).normalized();
            const QRect rectangle = mapToScene(viewportRectangle).boundingRect().toAlignedRect().intersected(sceneRect().toAlignedRect());
            QGraphicsView::mouseReleaseEvent(event);
            if (cropFinished) {
                cropFinished(viewportRectangle.width() > 2 && viewportRectangle.height() > 2 ? rectangle : QRect());
            }
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent *event) override
    {
        if (scene()->items().isEmpty()) {
            QGraphicsView::wheelEvent(event);
            return;
        }
        const int delta = event->angleDelta().y() ? event->angleDelta().y() : event->pixelDelta().y();
        if (!delta) {
            QGraphicsView::wheelEvent(event);
            return;
        }
        zoom(delta > 0 ? 1.25 : 1.0 / 1.25);
        event->accept();
    }

private:
    bool m_cropping = false;
    QPoint m_cropStart;
};

GalleryWindow::GalleryWindow(const QList<QUrl> &urls, const QSet<QUrl> &videos, int initialIndex, QWidget *parent)
    : QDialog(parent)
    , m_urls(urls)
    , m_videos(videos)
    , m_title(new QLabel(this))
    , m_message(new QLabel(this))
    , m_info(new QLabel(this))
    , m_infoPanel(new QScrollArea(this))
    , m_statsPanel(new QWidget(m_infoPanel))
    , m_colours(new QLabel(this))
    , m_histogram(new QLabel(this))
    , m_scene(new QGraphicsScene(this))
    , m_view(new GalleryView(m_scene, this))
    , m_pixmap(m_scene->addPixmap(QPixmap()))
    , m_stage(new QStackedWidget(this))
    , m_filmstrip(new QListWidget(this))
    , m_previous(new QPushButton(i18nc("@action", "Previous"), this))
    , m_next(new QPushButton(i18nc("@action", "Next"), this))
    , m_slideshow(new QPushButton(i18nc("@action", "Start Slideshow"), this))
    , m_rotate(new QPushButton(i18nc("@action", "Rotate Right"), this))
    , m_flip(new QPushButton(i18nc("@action", "Flip Horizontally"), this))
    , m_save(new QPushButton(i18nc("@action", "Save Copy…"), this))
    , m_editsButton(new QToolButton(this))
    , m_markedButton(new QToolButton(this))
    , m_timer(new QTimer(this))
{
    setObjectName(QStringLiteral("galleryWindow"));
    m_initialActualZoom = Settings::galleryInitialZoom() == Settings::EnumGalleryInitialZoom::ActualSize;
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18nc("@title:window", "Gallery"));
    resize(1000, 700);
    m_title->setObjectName(QStringLiteral("galleryTitle"));
    m_timer->setObjectName(QStringLiteral("gallerySlideshowTimer"));
    m_slideshow->setObjectName(QStringLiteral("gallerySlideshow"));
    m_title->setTextFormat(Qt::PlainText);
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_message->setObjectName(QStringLiteral("galleryMessage"));
    m_message->setTextFormat(Qt::PlainText);
    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    m_info->setObjectName(QStringLiteral("galleryInfo"));
    m_info->setTextFormat(Qt::PlainText);
    m_info->setWordWrap(true);
    m_info->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_colours->setObjectName(QStringLiteral("galleryColours"));
    m_histogram->setObjectName(QStringLiteral("galleryHistogram"));
    auto *infoContent = new QWidget(m_infoPanel);
    auto *infoLayout = new QVBoxLayout(infoContent);
    infoLayout->addWidget(m_info);
    auto *statsLayout = new QVBoxLayout(m_statsPanel);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->addWidget(new QLabel(i18nc("@title", "Dominant Colours"), m_statsPanel));
    statsLayout->addWidget(m_colours);
    statsLayout->addWidget(new QLabel(i18nc("@title", "Luminance Histogram"), m_statsPanel));
    statsLayout->addWidget(m_histogram);
    infoLayout->addWidget(m_statsPanel);
    infoLayout->addStretch();
    auto *infoScroll = qobject_cast<QScrollArea *>(m_infoPanel);
    infoScroll->setWidget(infoContent);
    infoScroll->setWidgetResizable(true);
    m_infoPanel->setMinimumWidth(200);
    m_infoPanel->setMaximumWidth(320);
    m_infoPanel->hide();

    m_view->setObjectName(QStringLiteral("galleryView"));
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform);
    m_view->setAlignment(Qt::AlignCenter);
    applyPresentationSettings();
    m_stage->addWidget(m_message);
    m_stage->addWidget(m_view);

    m_filmstrip->setObjectName(QStringLiteral("galleryFilmstrip"));
    m_filmstrip->setViewMode(QListView::IconMode);
    m_filmstrip->setFlow(QListView::LeftToRight);
    m_filmstrip->setWrapping(false);
    m_filmstrip->setResizeMode(QListView::Adjust);
    m_filmstrip->setMovement(QListView::Static);
    m_filmstrip->setIconSize(QSize(96, 72));
    m_filmstrip->setGridSize(QSize(112, 105));
    m_filmstrip->setFixedHeight(125);
    m_filmstrip->viewport()->installEventFilter(this);
    connect(m_filmstrip->horizontalScrollBar(), &QScrollBar::valueChanged, this, &GalleryWindow::queueVisibleThumbnails);
    const QIcon placeholder = SymbolicIcon::fromTheme(QStringLiteral("image-x-generic"));
    const QIcon videoIcon = SymbolicIcon::fromTheme(QStringLiteral("video-x-generic"));
    for (const QUrl &url : std::as_const(m_urls)) {
        auto *item = new QListWidgetItem(m_videos.contains(url) ? videoIcon : placeholder, url.fileName(), m_filmstrip);
        item->setToolTip(url.toDisplayString(QUrl::PreferLocalFile));
        if (!m_videos.contains(url)) {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
    }

    auto *zoomOut = new QPushButton(i18nc("@action", "Zoom Out"), this);
    auto *zoomIn = new QPushButton(i18nc("@action", "Zoom In"), this);
    auto *fit = new QPushButton(i18nc("@action", "Fit"), this);
    auto *info = new QPushButton(i18nc("@action", "Information"), this);
    info->setCheckable(true);
    info->setObjectName(QStringLiteral("galleryInformation"));
    auto *thumbnails = new QPushButton(i18nc("@action", "Thumbnails"), this);
    thumbnails->setObjectName(QStringLiteral("galleryThumbnails"));
    thumbnails->setCheckable(true);
    thumbnails->setChecked(Settings::galleryShowThumbnails());
    m_filmstrip->setVisible(thumbnails->isChecked());
    connect(thumbnails, &QPushButton::toggled, this, [this](bool visible) {
        m_filmstrip->setVisible(visible);
        if (visible) {
            queueVisibleThumbnails();
        }
    });
    auto *close = new QPushButton(i18nc("@action", "Close"), this);
    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(m_title, 1);
    toolbar->addWidget(zoomOut);
    toolbar->addWidget(zoomIn);
    toolbar->addWidget(fit);
    toolbar->addWidget(m_slideshow);
    toolbar->addWidget(info);
    toolbar->addWidget(thumbnails);
    toolbar->addWidget(close);
    auto *stage = new QHBoxLayout;
    stage->addWidget(m_previous);
    stage->addWidget(m_stage, 1);
    stage->addWidget(m_next);
    stage->addWidget(m_infoPanel);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addLayout(stage, 1);
    auto *edits = new QHBoxLayout;
    m_rotate->setObjectName(QStringLiteral("galleryRotate"));
    m_flip->setObjectName(QStringLiteral("galleryFlip"));
    m_save->setObjectName(QStringLiteral("gallerySave"));
    m_save->setToolTip(i18nc("@info:tooltip", "Save the displayed image without its original metadata."));
    edits->addWidget(m_rotate);
    edits->addWidget(m_flip);
    m_editsButton->setText(i18nc("@action", "More Edits"));
    m_editsButton->setPopupMode(QToolButton::InstantPopup);
    auto *editMenu = new QMenu(m_editsButton);
    m_editsButton->setMenu(editMenu);
    auto *rotateLeft = editMenu->addAction(i18nc("@action", "Rotate Left"));
    auto *rotateHalf = editMenu->addAction(i18nc("@action", "Rotate 180°"));
    auto *flipVertical = editMenu->addAction(i18nc("@action", "Flip Vertically"));
    editMenu->addSeparator();
    m_cropAction = editMenu->addAction(i18nc("@action", "Crop…"));
    m_cropAction->setObjectName(QStringLiteral("galleryCrop"));
    m_cropAction->setCheckable(true);
    auto *resize = editMenu->addAction(i18nc("@action", "Resize…"));
    auto *adjust = editMenu->addAction(i18nc("@action", "Adjust Colours…"));
    editMenu->addSeparator();
    auto *discard = editMenu->addAction(i18nc("@action", "Discard Edits"));
    auto *replace = editMenu->addAction(i18nc("@action", "Replace Original…"));
    resize->setObjectName(QStringLiteral("galleryResize"));
    adjust->setObjectName(QStringLiteral("galleryAdjust"));
    discard->setObjectName(QStringLiteral("galleryDiscard"));
    replace->setObjectName(QStringLiteral("galleryReplace"));
    edits->addWidget(m_editsButton);
    edits->addWidget(m_save);
    m_markedButton->setObjectName(QStringLiteral("galleryMarked"));
    m_markedButton->setText(i18nc("@action", "Marked Images"));
    m_markedButton->setPopupMode(QToolButton::InstantPopup);
    auto *markedMenu = new QMenu(m_markedButton);
    m_markedButton->setMenu(markedMenu);
    const auto addMarkedAction = [this, markedMenu](const QString &text, const QString &name, MarkedOperation operation) {
        QAction *action = markedMenu->addAction(text);
        action->setObjectName(name);
        connect(action, &QAction::triggered, this, [this, operation] {
            operateOnMarked(operation);
        });
    };
    addMarkedAction(i18nc("@action", "Copy To…"), QStringLiteral("galleryMarkedCopy"), MarkedOperation::Copy);
    addMarkedAction(i18nc("@action", "Move To…"), QStringLiteral("galleryMarkedMove"), MarkedOperation::Move);
    addMarkedAction(i18nc("@action", "Move to Trash"), QStringLiteral("galleryMarkedTrash"), MarkedOperation::Trash);
    addMarkedAction(i18nc("@action", "Delete Permanently"), QStringLiteral("galleryMarkedDelete"), MarkedOperation::Delete);
    markedMenu->addSeparator();
    auto *convert = markedMenu->addAction(i18nc("@action", "Convert Images…"));
    convert->setObjectName(QStringLiteral("galleryMarkedConvert"));
    connect(convert, &QAction::triggered, this, [this] {
        if (m_timer->isActive()) {
            toggleSlideshow();
        }
        BatchConversionDialog::run(this, markedImages());
    });
    edits->addWidget(m_markedButton);
    edits->addStretch();
    layout->addLayout(edits);
    layout->addWidget(m_filmstrip);

    connect(m_previous, &QPushButton::clicked, this, [this] {
        showIndex(m_index - 1);
    });
    connect(m_next, &QPushButton::clicked, this, [this] {
        showIndex(m_index + 1);
    });
    connect(zoomOut, &QPushButton::clicked, this, [this] {
        zoom(1.0 / 1.25);
    });
    connect(zoomIn, &QPushButton::clicked, this, [this] {
        zoom(1.25);
    });
    connect(fit, &QPushButton::clicked, this, &GalleryWindow::fitImage);
    connect(info, &QPushButton::toggled, this, [this](bool visible) {
        m_infoPanel->setVisible(visible);
        scheduleDetails();
    });
    info->setChecked(Settings::galleryShowInformation());
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    connect(m_rotate, &QPushButton::clicked, this, [this] {
        startEdit([](const QImage &image) {
            return image.transformed(QTransform().rotate(90));
        });
    });
    connect(m_flip, &QPushButton::clicked, this, [this] {
        startEdit([](const QImage &image) {
            return image.transformed(QTransform::fromScale(-1, 1));
        });
    });
    connect(rotateLeft, &QAction::triggered, this, [this] {
        startEdit([](const QImage &image) {
            return image.transformed(QTransform().rotate(-90));
        });
    });
    connect(rotateHalf, &QAction::triggered, this, [this] {
        startEdit([](const QImage &image) {
            return image.transformed(QTransform().rotate(180));
        });
    });
    connect(flipVertical, &QAction::triggered, this, [this] {
        startEdit([](const QImage &image) {
            return image.transformed(QTransform::fromScale(1, -1));
        });
    });
    connect(m_cropAction, &QAction::toggled, this, [this](bool cropping) {
        if (cropping && m_timer->isActive()) {
            toggleSlideshow();
        }
        m_view->setCropping(cropping);
    });
    m_view->cropFinished = [this](const QRect &rectangle) {
        m_cropAction->setChecked(false);
        if (!rectangle.isEmpty()) {
            startEdit([rectangle](const QImage &image) {
                return ImageEditing::crop(image, rectangle);
            });
        }
    };
    m_view->doubleClicked = [this] {
        if (m_image.isNull()) {
            return;
        }
        if (qAbs(m_view->transform().m11() - 2.0) < 0.001) {
            fitImage();
        } else {
            m_view->resetTransform();
            m_view->scale(2.0, 2.0);
        }
    };
    connect(resize, &QAction::triggered, this, &GalleryWindow::resizeImage);
    connect(adjust, &QAction::triggered, this, &GalleryWindow::adjustImage);
    connect(discard, &QAction::triggered, this, &GalleryWindow::discardEdits);
    connect(replace, &QAction::triggered, this, &GalleryWindow::replaceOriginal);
    connect(m_save, &QPushButton::clicked, this, &GalleryWindow::saveCopy);
    connect(m_slideshow, &QPushButton::clicked, this, &GalleryWindow::toggleSlideshow);
    connect(m_filmstrip, &QListWidget::currentRowChanged, this, &GalleryWindow::showIndex);
    connect(m_filmstrip, &QListWidget::itemChanged, this, [this] {
        updateNavigation();
    });
    m_timer->setInterval(Settings::gallerySlideshowInterval() * 1000);
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (m_index + 1 >= m_urls.size()) {
            if (!Settings::gallerySlideshowLoop()) {
                toggleSlideshow();
                return;
            }
            showIndex(0);
        } else {
            showIndex(m_index + 1);
        }
    });
    const auto shortcut = [this](const QKeySequence &key, auto callback) {
        auto *binding = new QShortcut(key, this);
        connect(binding, &QShortcut::activated, this, callback);
    };
    shortcut(Qt::Key_Left, [this] {
        showIndex(m_index + (layoutDirection() == Qt::RightToLeft ? 1 : -1));
    });
    shortcut(Qt::Key_Right, [this] {
        showIndex(m_index + (layoutDirection() == Qt::RightToLeft ? -1 : 1));
    });
    shortcut(Qt::Key_Home, [this] {
        showIndex(0);
    });
    shortcut(Qt::Key_End, [this] {
        showIndex(m_urls.size() - 1);
    });
    shortcut(Qt::Key_Plus, [this] {
        zoom(1.25);
    });
    shortcut(Qt::Key_Minus, [this] {
        zoom(1.0 / 1.25);
    });
    shortcut(Qt::Key_0, [this] {
        fitImage();
    });
    shortcut(Qt::Key_Space, [this] {
        toggleSlideshow();
    });
    shortcut(Qt::Key_I, [info] {
        info->toggle();
    });
    shortcut(Qt::Key_R, [this] {
        if (m_rotate->isEnabled()) {
            m_rotate->click();
        }
    });
    shortcut(Qt::Key_F, [this] {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    });
    shortcut(QKeySequence::Copy, [this] {
        if (!m_image.isNull()) {
            QApplication::clipboard()->setImage(m_image);
        }
    });

    if (!m_urls.isEmpty()) {
        showIndex(std::clamp(initialIndex, 0, int(m_urls.size()) - 1));
        QTimer::singleShot(0, this, &GalleryWindow::queueVisibleThumbnails);
    } else {
        m_message->setText(i18nc("@info", "No images are available."));
        updateNavigation();
    }
}

GalleryWindow::~GalleryWindow()
{
    clearPlayback();
}

void GalleryWindow::applyPresentationSettings()
{
    m_view->setBackgroundBrush(imageBackgroundBrush());
}

QList<QUrl> GalleryWindow::markedImages() const
{
    QList<QUrl> result;
    for (int i = 0; i < m_filmstrip->count(); ++i) {
        const QListWidgetItem *item = m_filmstrip->item(i);
        if ((item->flags() & Qt::ItemIsUserCheckable) && item->checkState() == Qt::Checked) {
            result.append(m_urls.at(i));
        }
    }
    return result;
}

void GalleryWindow::operateOnMarked(MarkedOperation operation)
{
    const QList<QUrl> sources = markedImages();
    if (sources.isEmpty() || m_editing || m_operationRunning) {
        return;
    }
    if (m_timer->isActive()) {
        toggleSlideshow();
    }
    QUrl destination;
    if (operation == MarkedOperation::Copy || operation == MarkedOperation::Move) {
        destination =
            QFileDialog::getExistingDirectoryUrl(this, i18nc("@title:window", "Choose Destination"), m_urls.at(m_index).adjusted(QUrl::RemoveFilename));
        if (destination.isEmpty()) {
            return;
        }
    }
    if (operation != MarkedOperation::Copy && sources.contains(m_urls.at(m_index)) && !confirmDiscard()) {
        return;
    }
    QWidget *jobWindow = parentWidget();
    KJob *job = nullptr;
    switch (operation) {
    case MarkedOperation::Copy:
        job = FileOperations::copy(sources, destination, jobWindow);
        break;
    case MarkedOperation::Move:
        job = FileOperations::move(sources, destination, jobWindow);
        break;
    case MarkedOperation::Trash:
        job = FileOperations::trash(sources, jobWindow);
        break;
    case MarkedOperation::Delete:
        job = FileOperations::deletePermanently(sources, jobWindow);
        break;
    }
    if (!job) {
        KMessageBox::error(this, i18nc("@info", "Could not start the file operation."));
        return;
    }
    m_operationRunning = true;
    setEnabled(false);
    connect(job, &KJob::result, this, [this, sources, operation] {
        if (operation != MarkedOperation::Copy) {
            refreshAfterOperation(sources);
        }
        m_operationRunning = false;
        setEnabled(true);
    });
}

void GalleryWindow::refreshAfterOperation(const QList<QUrl> &sources)
{
    const int oldIndex = m_index;
    const QUrl current = m_urls.value(m_index);
    bool removed = false;
    {
        const QSignalBlocker blocker(m_filmstrip);
        for (int i = m_urls.size() - 1; i >= 0; --i) {
            const QUrl url = m_urls.at(i);
            const QFileInfo file(url.toLocalFile());
            if (sources.contains(url) && !file.exists() && !file.isSymLink()) {
                m_urls.removeAt(i);
                m_videos.remove(url);
                delete m_filmstrip->takeItem(i);
                removed = true;
            }
        }
    }
    if (!removed) {
        return;
    }
    ++m_thumbnailGeneration;
    m_thumbnailQueue.clear();
    m_thumbnailQueued.clear();
    m_thumbnailDone.clear();
    m_index = m_urls.indexOf(current);
    if (m_index >= 0) {
        if (m_pendingIndex >= 0) {
            m_pendingIndex = m_index;
        }
        m_filmstrip->setCurrentRow(m_index);
        m_title->setText(i18nc("@title", "%1 (%2 of %3)", current.fileName(), m_index + 1, m_urls.size()));
        updateNavigation();
    } else {
        m_modified = false;
        m_edited = false;
        ++m_generation;
        m_pendingIndex = -1;
        clearPlayback();
        m_image = {};
        m_pixmap->setPixmap({});
        m_info->clear();
        scheduleDetails();
        if (m_urls.isEmpty()) {
            m_title->clear();
            m_message->setText(i18nc("@info", "No images are available."));
            m_stage->setCurrentWidget(m_message);
            updateNavigation();
        } else {
            showIndex(std::min(oldIndex, int(m_urls.size()) - 1));
        }
    }
    queueVisibleThumbnails();
}

void GalleryWindow::showIndex(int index)
{
    if (m_editing) {
        const QSignalBlocker blocker(m_filmstrip);
        m_filmstrip->setCurrentRow(m_index);
        return;
    }
    if (index < 0 || index >= m_urls.size() || index == m_index) {
        return;
    }
    if (!confirmDiscard()) {
        const QSignalBlocker blocker(m_filmstrip);
        m_filmstrip->setCurrentRow(m_index);
        return;
    }
    m_index = index;
    m_cropAction->setChecked(false);
    ++m_generation;
    const QUrl url = m_urls.at(index);
    m_pendingIndex = m_videos.contains(url) ? -1 : index;
    clearPlayback();
    m_image = {};
    m_modified = false;
    m_edited = false;
    m_pixmap->setPixmap({});
    m_info->clear();
    scheduleDetails();
    m_message->setText(i18nc("@info", "Loading image…"));
    m_stage->setCurrentWidget(m_message);
    m_title->setText(i18nc("@title", "%1 (%2 of %3)", m_urls.at(index).fileName(), index + 1, m_urls.size()));
    m_filmstrip->setCurrentRow(index);
    queueVisibleThumbnails();
    updateNavigation();
    Q_EMIT currentUrlChanged(m_urls.at(index));
    if (m_videos.contains(url)) {
        showVideo(url);
    } else if (!m_loading) {
        startImageLoad();
    }
}

void GalleryWindow::startImageLoad()
{
    const int index = std::exchange(m_pendingIndex, -1);
    const quint64 generation = m_generation;
    const QUrl url = m_urls.at(index);
    m_loading = true;
    auto *watcher = new QFutureWatcher<PreviewContent>(this);
    connect(watcher, &QFutureWatcher<PreviewContent>::finished, this, [this, watcher, generation] {
        const PreviewContent content = watcher->result();
        watcher->deleteLater();
        m_loading = false;
        if (generation == m_generation) {
            showImage(content);
        }
        if (m_pendingIndex >= 0) {
            startImageLoad();
        }
    });
    watcher->setFuture(QtConcurrent::run([url] {
        return Preview::loadImage(url, 0);
    }));
}

void GalleryWindow::showImage(const PreviewContent &content)
{
    if (content.kind != PreviewContent::Kind::Image) {
        const QString message = content.kind == PreviewContent::Kind::TooLarge ? i18nc("@info", "This image is too large to open in the gallery.")
            : content.kind == PreviewContent::Kind::Error                      ? i18nc("@info", "Could not load image: %1", content.text)
                                                                               : i18nc("@info", "This image cannot be opened in the gallery.");
        m_message->setText(message);
        m_stage->setCurrentWidget(m_message);
        updateNavigation();
        return;
    }
    m_image = content.image;
    m_pixmap->setPixmap(QPixmap::fromImage(m_image));
    m_scene->setSceneRect(m_pixmap->boundingRect());
    const QFileInfo file(m_urls.at(m_index).toLocalFile());
    m_originalSize = file.size();
    m_originalModified = file.lastModified();
    const QString created = file.birthTime().isValid() ? LocalizedDates::dateTime(file.birthTime()) : i18nc("@info", "Unknown");
    const QString mime = QMimeDatabase().mimeTypeForFile(file.filePath(), QMimeDatabase::MatchExtension).name();
    m_baseInfo = i18nc("@info",
                       "Name: %1\nDimensions: %2 × %3\nMegapixels: %4\nAspect ratio: %5\nSize: %6",
                       file.fileName(),
                       LocalizedNumbers::shape(QLocale().toString(m_image.width())),
                       LocalizedNumbers::shape(QLocale().toString(m_image.height())),
                       LocalizedNumbers::shape(QLocale().toString(double(m_image.width()) * m_image.height() / 1000000.0, 'f', 2)),
                       LocalizedNumbers::shape(QLocale().toString(double(m_image.width()) / m_image.height(), 'f', 2)),
                       LocalizedNumbers::shape(KIO::convertSize(file.size())))
        + QLatin1Char('\n')
        + i18nc("@info",
                "MIME type: %1\nModified: %2\nCreated: %3\nPath: %4",
                mime,
                LocalizedDates::dateTime(file.lastModified()),
                created,
                file.absoluteFilePath());
    m_info->setText(m_baseInfo);
    m_stage->setCurrentWidget(m_view);
    if (m_initialActualZoom) {
        m_view->resetTransform();
    } else {
        fitImage();
    }
    if (content.animated) {
        m_movie = new QMovie(m_urls.at(m_index).toLocalFile(), {}, this);
        m_movie->setObjectName(QStringLiteral("galleryAnimation"));
        connect(m_movie, &QMovie::frameChanged, this, [this] {
            m_image = m_movie->currentImage();
            m_pixmap->setPixmap(m_movie->currentPixmap());
        });
        connect(m_movie, &QMovie::error, this, [this] {
            m_message->setText(i18nc("@info", "Could not play animation: %1", m_movie->lastErrorString()));
            m_stage->setCurrentWidget(m_message);
        });
        m_movie->start();
    }
    updateNavigation();
    scheduleDetails();
}

void GalleryWindow::showVideo(const QUrl &url)
{
    const QFileInfo file(url.toLocalFile());
    if (!file.isFile()) {
        m_message->setText(i18nc("@info", "Could not open this video."));
        return;
    }
    if (!m_media) {
        m_media = new MediaView(m_stage);
        m_media->setObjectName(QStringLiteral("galleryMedia"));
        m_stage->addWidget(m_media);
        connect(m_media, &MediaView::playbackError, this, [this](const QString &message) {
            m_message->setText(i18nc("@info", "Could not play video: %1", message));
            m_stage->setCurrentWidget(m_message);
        });
    }
    m_info->setText(i18nc("@info",
                          "Name: %1\nSize: %2\nModified: %3\nPath: %4",
                          file.fileName(),
                          KIO::convertSize(file.size()),
                          LocalizedDates::dateTime(file.lastModified()),
                          file.absoluteFilePath()));
    m_stage->setCurrentWidget(m_media);
    m_media->open(url, true, true);
}

void GalleryWindow::clearPlayback()
{
    if (m_movie) {
        delete std::exchange(m_movie, nullptr);
    }
    if (m_media) {
        m_media->clear();
    }
}

void GalleryWindow::startThumbnailLoad()
{
    if (m_loadingThumbnail || m_thumbnailQueue.isEmpty()) {
        return;
    }
    const int index = m_thumbnailQueue.dequeue();
    const QUrl url = m_urls.at(index);
    m_loadingThumbnail = true;
    auto *watcher = new QFutureWatcher<PreviewContent>(this);
    const quint64 generation = m_thumbnailGeneration;
    connect(watcher, &QFutureWatcher<PreviewContent>::finished, this, [this, watcher, index, generation] {
        const PreviewContent content = watcher->result();
        watcher->deleteLater();
        m_loadingThumbnail = false;
        if (generation == m_thumbnailGeneration) {
            m_thumbnailQueued.remove(index);
            m_thumbnailDone.insert(index);
            if (content.kind == PreviewContent::Kind::Image) {
                m_filmstrip->item(index)->setIcon(QIcon(QPixmap::fromImage(content.image)));
            }
        }
        startThumbnailLoad();
    });
    watcher->setFuture(QtConcurrent::run([url] {
        return Preview::loadImage(url, 96);
    }));
}

void GalleryWindow::queueVisibleThumbnails()
{
    if (m_filmstrip->isHidden()) {
        return;
    }
    const auto queue = [this](int index) {
        if (index >= 0 && index < m_urls.size() && !m_videos.contains(m_urls.at(index)) && !m_thumbnailDone.contains(index)
            && !m_thumbnailQueued.contains(index)) {
            m_thumbnailQueue.enqueue(index);
            m_thumbnailQueued.insert(index);
        }
    };
    queue(m_index);
    const int width = m_filmstrip->viewport()->width();
    const int y = m_filmstrip->viewport()->height() / 2;
    for (int x = 0; x < width; x += 56) {
        queue(m_filmstrip->indexAt(QPoint(x, y)).row());
    }
    queue(m_filmstrip->indexAt(QPoint(std::max(0, width - 1), y)).row());
    startThumbnailLoad();
}

bool GalleryWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_filmstrip->viewport() && event->type() == QEvent::Resize) {
        QTimer::singleShot(0, this, &GalleryWindow::queueVisibleThumbnails);
    }
    return QDialog::eventFilter(watched, event);
}

void GalleryWindow::scheduleDetails()
{
    ++m_detailsGeneration;
    m_detailsPending = false;
    m_colours->clear();
    m_histogramData.clear();
    m_histogram->clear();
    m_statsPanel->setVisible(!m_image.isNull());
    if (m_infoPanel->isHidden() || m_image.isNull()) {
        return;
    }
    m_detailsPending = true;
    if (!m_detailsLoading) {
        startDetails();
    }
}

void GalleryWindow::startDetails()
{
    m_detailsPending = false;
    m_detailsLoading = true;
    const quint64 generation = m_detailsGeneration;
    const QImage image = m_image;
    const QString path = m_urls.at(m_index).toLocalFile();
    auto *watcher = new QFutureWatcher<ImageDetails::Data>(this);
    connect(watcher, &QFutureWatcher<ImageDetails::Data>::finished, this, [this, watcher, generation] {
        const ImageDetails::Data details = watcher->result();
        watcher->deleteLater();
        m_detailsLoading = false;
        if (generation == m_detailsGeneration && !m_infoPanel->isHidden()) {
            showDetails(details);
        }
        if (m_detailsPending) {
            startDetails();
        }
    });
    watcher->setFuture(QtConcurrent::run([image, path] {
        return ImageDetails::analyze(image, path);
    }));
}

void GalleryWindow::showDetails(const ImageDetails::Data &details)
{
    QStringList lines;
    using namespace KFileMetaData;
    for (const Property::Property property : {Property::Manufacturer,
                                              Property::Model,
                                              Property::PhotoDateTimeOriginal,
                                              Property::PhotoISOSpeedRatings,
                                              Property::PhotoFNumber,
                                              Property::PhotoExposureTime,
                                              Property::PhotoFocalLength}) {
        const QList<QVariant> values = details.metadata.values(property);
        if (values.isEmpty()) {
            continue;
        }
        const PropertyInfo info(property);
        const QString value = info.formatAsDisplayString(values.first()).left(512);
        if (!value.isEmpty()) {
            lines.append(i18nc("@info image metadata label and value", "%1: %2", info.displayName(), value));
        }
    }
    m_info->setText(lines.isEmpty() ? m_baseInfo : m_baseInfo + QStringLiteral("\n\n") + lines.join(QLatin1Char('\n')));

    QPixmap colours(200, 24);
    colours.fill(Qt::transparent);
    QPainter coloursPainter(&colours);
    for (int i = 0; i < details.colours.size(); ++i) {
        coloursPainter.fillRect(i * 40, 0, 40, 24, details.colours.at(i));
    }
    m_colours->setPixmap(colours);

    m_histogramData = details.histogram;
    paintHistogram();
}

void GalleryWindow::paintHistogram()
{
    QPixmap histogram(200, 70);
    histogram.fill(palette().base().color());
    QPainter histogramPainter(&histogram);
    histogramPainter.setPen(Qt::NoPen);
    histogramPainter.setBrush(palette().highlight());
    const int maximum = m_histogramData.isEmpty() ? 0 : *std::max_element(m_histogramData.cbegin(), m_histogramData.cend());
    if (maximum > 0) {
        for (int i = 0; i < m_histogramData.size(); ++i) {
            const int height = 68 * m_histogramData.at(i) / maximum;
            histogramPainter.drawRect(i * 5, 70 - height, 4, height);
        }
    }
    histogramPainter.end();
    m_histogram->setPixmap(histogram);
}

void GalleryWindow::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && !m_histogram->pixmap().isNull()) {
        paintHistogram();
    }
}

void GalleryWindow::fitImage()
{
    if (!m_image.isNull()) {
        m_view->fitInView(m_pixmap, Qt::KeepAspectRatio);
    }
}

void GalleryWindow::zoom(qreal factor)
{
    if (m_image.isNull()) {
        return;
    }
    m_view->zoom(factor);
}

void GalleryWindow::toggleSlideshow()
{
    if (m_editing) {
        return;
    }
    if (m_timer->isActive()) {
        m_timer->stop();
        m_slideshow->setText(i18nc("@action", "Start Slideshow"));
    } else if (m_urls.size() > 1) {
        m_timer->start();
        m_slideshow->setText(i18nc("@action", "Stop Slideshow"));
    }
}

void GalleryWindow::updateNavigation()
{
    m_previous->setEnabled(!m_editing && m_index > 0);
    m_next->setEnabled(!m_editing && m_index >= 0 && m_index + 1 < m_urls.size());
    m_slideshow->setEnabled(!m_editing && m_urls.size() > 1);
    const bool editable = !m_editing && !m_image.isNull() && !m_movie;
    m_rotate->setEnabled(editable);
    m_flip->setEnabled(editable);
    m_editsButton->setEnabled(editable);
    m_save->setEnabled(editable);
    m_markedButton->setEnabled(!m_editing && !markedImages().isEmpty());
    m_filmstrip->setEnabled(!m_editing);
}

void GalleryWindow::startEdit(std::function<QImage(const QImage &)> operation)
{
    if (m_editing || m_image.isNull() || m_movie) {
        return;
    }
    if (m_timer->isActive()) {
        toggleSlideshow();
    }
    m_cropAction->setChecked(false);
    m_editing = true;
    m_message->setText(i18nc("@info", "Applying image edit…"));
    m_stage->setCurrentWidget(m_message);
    updateNavigation();
    const QImage source = m_image;
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher] {
        const QImage image = watcher->result();
        watcher->deleteLater();
        m_editing = false;
        m_stage->setCurrentWidget(m_view);
        replaceImage(image);
        updateNavigation();
    });
    watcher->setFuture(QtConcurrent::run([source, operation = std::move(operation)] {
        return operation(source);
    }));
}

void GalleryWindow::replaceImage(const QImage &image)
{
    if (image.isNull()) {
        KMessageBox::error(this, i18nc("@info", "Could not edit this image."));
        return;
    }
    if (m_timer->isActive()) {
        toggleSlideshow();
    }
    m_image = image;
    m_modified = true;
    m_edited = true;
    m_pixmap->setPixmap(QPixmap::fromImage(m_image));
    m_scene->setSceneRect(m_pixmap->boundingRect());
    m_baseInfo = i18nc("@info", "Edited dimensions: %1 × %2\nOriginal: %3", m_image.width(), m_image.height(), m_urls.at(m_index).toLocalFile());
    m_info->setText(m_baseInfo);
    scheduleDetails();
    fitImage();
}

void GalleryWindow::resizeImage()
{
    if (m_timer->isActive()) {
        toggleSlideshow();
    }
    QDialog dialog(this);
    dialog.setWindowTitle(i18nc("@title:window", "Resize Image"));
    auto *width = new QSpinBox(&dialog);
    auto *height = new QSpinBox(&dialog);
    width->setObjectName(QStringLiteral("resizeWidth"));
    height->setObjectName(QStringLiteral("resizeHeight"));
    width->setRange(1, 40000000);
    height->setRange(1, 40000000);
    width->setValue(m_image.width());
    height->setValue(m_image.height());
    auto *aspect = new QCheckBox(i18nc("@option:check", "Keep aspect ratio"), &dialog);
    aspect->setChecked(true);
    const QSize original = m_image.size();
    connect(width, &QSpinBox::valueChanged, &dialog, [height, aspect, original](int value) {
        if (aspect->isChecked()) {
            const QSignalBlocker blocker(height);
            height->setValue(int(std::clamp<qint64>(qint64(value) * original.height() / original.width(), 1, 40000000)));
        }
    });
    connect(height, &QSpinBox::valueChanged, &dialog, [width, aspect, original](int value) {
        if (aspect->isChecked()) {
            const QSignalBlocker blocker(width);
            width->setValue(int(std::clamp<qint64>(qint64(value) * original.width() / original.height(), 1, 40000000)));
        }
    });
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:spinbox", "Width (pixels):"), width);
    form->addRow(i18nc("@label:spinbox", "Height (pixels):"), height);
    form->addRow(aspect);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, width, height] {
        if (qint64(width->value()) * height->value() > 40000000) {
            KMessageBox::error(&dialog, i18nc("@info", "The resized image must not exceed 40 megapixels."));
        } else {
            dialog.accept();
        }
    });
    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        const QSize target(width->value(), height->value());
        if (target != original) {
            startEdit([target](const QImage &image) {
                return ImageEditing::resize(image, target);
            });
        }
    }
}

void GalleryWindow::adjustImage()
{
    if (m_timer->isActive()) {
        toggleSlideshow();
    }
    QDialog dialog(this);
    dialog.setWindowTitle(i18nc("@title:window", "Adjust Image Colours"));
    auto *preview = new QLabel(&dialog);
    preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumSize(320, 220);
    const QImage small = m_image.scaled(QSize(600, 400), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    auto *form = new QFormLayout;
    const auto control = [&dialog, form](const QString &label, const QString &name, int minimum, int maximum, int value) {
        auto *slider = new QSlider(Qt::Horizontal, &dialog);
        slider->setObjectName(name);
        slider->setRange(minimum, maximum);
        slider->setValue(value);
        auto *number = new QSpinBox(&dialog);
        number->setRange(minimum, maximum);
        number->setSuffix(QStringLiteral("%"));
        number->setValue(value);
        QObject::connect(slider, &QSlider::valueChanged, number, &QSpinBox::setValue);
        QObject::connect(number, &QSpinBox::valueChanged, slider, &QSlider::setValue);
        auto *row = new QHBoxLayout;
        row->addWidget(slider, 1);
        row->addWidget(number);
        form->addRow(label, row);
        return slider;
    };
    auto *brightness = control(i18nc("@label:slider", "Brightness:"), QStringLiteral("brightness"), 50, 150, 100);
    auto *contrast = control(i18nc("@label:slider", "Contrast:"), QStringLiteral("contrast"), 50, 150, 100);
    auto *saturation = control(i18nc("@label:slider", "Saturation:"), QStringLiteral("saturation"), 0, 200, 100);
    auto *grayscale = control(i18nc("@label:slider", "Grayscale:"), QStringLiteral("grayscale"), 0, 100, 0);
    const auto values = [brightness, contrast, saturation, grayscale] {
        return ImageEditing::Adjustments{brightness->value(), contrast->value(), saturation->value(), grayscale->value()};
    };
    auto *timer = new QTimer(&dialog);
    timer->setSingleShot(true);
    timer->setInterval(75);
    connect(timer, &QTimer::timeout, &dialog, [preview, small, values] {
        preview->setPixmap(QPixmap::fromImage(ImageEditing::adjust(small, values())));
    });
    for (QSlider *slider : {brightness, contrast, saturation, grayscale}) {
        connect(slider, &QSlider::valueChanged, timer, qOverload<>(&QTimer::start));
    }
    preview->setPixmap(QPixmap::fromImage(small));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(preview, 1);
    layout->addLayout(form);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        const ImageEditing::Adjustments options = values();
        if (options.brightness != 100 || options.contrast != 100 || options.saturation != 100 || options.grayscale != 0) {
            startEdit([options](const QImage &image) {
                return ImageEditing::adjust(image, options);
            });
        }
    }
}

void GalleryWindow::discardEdits()
{
    if (!m_edited || !confirmDiscard()) {
        return;
    }
    m_modified = false;
    const int index = m_index;
    m_index = -1;
    showIndex(index);
}

void GalleryWindow::saveCopy()
{
    if (m_image.isNull()) {
        return;
    }
    if (m_timer->isActive()) {
        toggleSlideshow();
    }
    const QFileInfo original(m_urls.at(m_index).toLocalFile());
    const QString extension = Settings::galleryCopyFormat() == Settings::EnumGalleryCopyFormat::Jpeg ? QStringLiteral("jpg")
        : Settings::galleryCopyFormat() == Settings::EnumGalleryCopyFormat::Webp                     ? QStringLiteral("webp")
                                                                                                     : QStringLiteral("png");
    const QString suggested = original.dir().filePath(original.completeBaseName() + QStringLiteral("-edited.") + extension);
    const QString path = QFileDialog::getSaveFileName(this,
                                                      i18nc("@title:window", "Save Image Copy"),
                                                      suggested,
                                                      i18nc("@item:inlistbox", "Images (*.png *.jpg *.jpeg *.webp)"));
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo destination(path);
    const QString originalCanonicalPath = original.canonicalFilePath();
    if (destination.absoluteFilePath() == original.absoluteFilePath()
        || (!originalCanonicalPath.isEmpty() && destination.canonicalFilePath() == originalCanonicalPath)) {
        KMessageBox::error(this, i18nc("@info", "Choose a different file for the image copy."));
        return;
    }
    QByteArray format = QFileInfo(path).suffix().toLower().toLatin1();
    if (format == "jpg") {
        format = "jpeg";
    }
    if ((format != "png" && format != "jpeg" && format != "webp") || !QImageWriter::supportedImageFormats().contains(format)) {
        KMessageBox::error(this, i18nc("@info", "Choose a supported PNG, JPEG or WebP file name."));
        return;
    }
    int quality = -1;
    if (format == "jpeg" || format == "webp") {
        bool accepted = false;
        quality = QInputDialog::getInt(this,
                                       i18nc("@title:window", "Image Quality"),
                                       i18nc("@label", "Quality (0–100):"),
                                       Settings::galleryCopyQuality(),
                                       0,
                                       100,
                                       1,
                                       &accepted);
        if (!accepted) {
            return;
        }
    }
    if (saveImage(path, format, quality)) {
        m_modified = false;
    }
}

bool GalleryWindow::saveImage(const QString &path, const QByteArray &format, int quality)
{
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        KMessageBox::error(this, i18nc("@info", "Could not save image: %1", output.errorString()));
        return false;
    }
    QImageWriter writer(&output, format);
    if (quality >= 0) {
        writer.setQuality(quality);
    }
    if (!writer.write(m_image)) {
        const QString error = writer.errorString();
        output.cancelWriting();
        KMessageBox::error(this, i18nc("@info", "Could not save image: %1", error));
        return false;
    }
    if (!output.commit()) {
        KMessageBox::error(this, i18nc("@info", "Could not save image: %1", output.errorString()));
        return false;
    }
    return true;
}

void GalleryWindow::replaceOriginal()
{
    if (m_image.isNull() || m_movie) {
        return;
    }
    if (m_timer->isActive()) {
        toggleSlideshow();
    }
    const QFileInfo original(m_urls.at(m_index).toLocalFile());
    if (!original.isFile() || original.isSymLink()) {
        KMessageBox::error(this, i18nc("@info", "The original is missing or is a symbolic link. Save a copy instead."));
        return;
    }
    const bool changed = original.size() != m_originalSize || original.lastModified() != m_originalModified;
    const QString warning = changed ? i18nc("@info", "The original file has changed on disk. Replace it anyway? Its metadata will not be preserved.")
                                    : i18nc("@info", "Replace the original file? Its metadata will not be preserved.");
    if (KMessageBox::warningContinueCancel(this, warning, i18nc("@title:window", "Replace Original"), KStandardGuiItem::cont(), KStandardGuiItem::cancel())
        != KMessageBox::Continue) {
        return;
    }
    const QByteArray format = QImageReader(original.filePath()).format().toLower();
    if (format.isEmpty() || !QImageWriter::supportedImageFormats().contains(format)) {
        KMessageBox::error(this, i18nc("@info", "The original image format cannot be written. Save a copy instead."));
        return;
    }
    if (saveImage(original.filePath(), format)) {
        const QFileInfo saved(original.filePath());
        m_originalSize = saved.size();
        m_originalModified = saved.lastModified();
        m_modified = false;
        m_edited = false;
    }
}

bool GalleryWindow::confirmDiscard()
{
    if (m_editing) {
        return KMessageBox::warningContinueCancel(this,
                                                  i18nc("@info", "An image edit is still running. Close and discard it?"),
                                                  i18nc("@title:window", "Image Edit in Progress"),
                                                  KStandardGuiItem::discard(),
                                                  KStandardGuiItem::cancel())
            == KMessageBox::Continue;
    }
    if (!m_modified) {
        return true;
    }
    if (m_timer->isActive()) {
        toggleSlideshow();
    }
    return KMessageBox::warningContinueCancel(this,
                                              i18nc("@info", "Discard the unsaved image edits?"),
                                              i18nc("@title:window", "Unsaved Image Edits"),
                                              KStandardGuiItem::discard(),
                                              KStandardGuiItem::cancel())
        == KMessageBox::Continue;
}

void GalleryWindow::closeEvent(QCloseEvent *event)
{
    if (m_operationRunning) {
        event->ignore();
        return;
    }
    if (confirmDiscard()) {
        m_modified = false;
        QDialog::closeEvent(event);
    } else {
        event->ignore();
    }
}

void GalleryWindow::reject()
{
    if (m_operationRunning) {
        return;
    }
    if (confirmDiscard()) {
        m_modified = false;
        QDialog::reject();
    }
}
