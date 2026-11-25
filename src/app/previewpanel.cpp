// SPDX-License-Identifier: GPL-3.0-or-later

#include "previewpanel.h"
#include "safetextdocument.h"

#include "imagebackground.h"
#include "imagedetails.h"
#include "localizeddates.h"
#include "localizednumbers.h"
#include "mediaview.h"
#include "settings.h"
#include "symbolicicon.h"

#include <KFileMetaData/PropertyInfo>
#include <KIO/Global>
#include <KIO/TransferJob>
#include <KLocalizedString>
#include <KParts/PartLoader>
#include <KParts/ReadOnlyPart>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>

#include <QActionGroup>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMediaPlayer>
#include <QMenu>
#include <QMimeDatabase>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTemporaryFile>
#include <QTextBlock>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace
{
constexpr qint64 ImageLimit = 8 * 1024 * 1024;
constexpr qint64 RemoteFileLimit = 128 * 1024 * 1024;
constexpr qsizetype HighlightLineLimit = 16384;

class ImagePreviewLabel : public QLabel
{
public:
    using QLabel::QLabel;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        const QBrush brush = imageBackgroundBrush();
        if (brush.style() != Qt::NoBrush) {
            QPainter painter(this);
            painter.fillRect(rect(), brush);
        }
        QLabel::paintEvent(event);
    }
};

bool isDocumentMime(const QString &mimeType)
{
    return mimeType == QLatin1String("application/pdf") || mimeType == QLatin1String("application/msword")
        || mimeType == QLatin1String("application/vnd.ms-excel") || mimeType == QLatin1String("application/vnd.ms-powerpoint")
        || mimeType.startsWith(QLatin1String("application/vnd.openxmlformats-officedocument."))
        || mimeType.startsWith(QLatin1String("application/vnd.oasis.opendocument."));
}

bool isTextMime(const QString &mimeType)
{
    return QMimeDatabase().mimeTypeForName(mimeType).inherits(QStringLiteral("text/plain")) || mimeType == QLatin1String("application/xhtml+xml")
        || mimeType == QLatin1String("text/markdown");
}
}

class PreviewSyntaxHighlighter : public KSyntaxHighlighting::SyntaxHighlighter
{
public:
    using SyntaxHighlighter::SyntaxHighlighter;

protected:
    void highlightBlock(const QString &text) override
    {
        if (text.size() > HighlightLineLimit) {
            setCurrentBlockState(-1);
            return;
        }
        SyntaxHighlighter::highlightBlock(text);
    }
};

class PreviewTextEdit : public QPlainTextEdit
{
    class LineNumbers : public QWidget
    {
    public:
        explicit LineNumbers(PreviewTextEdit *editor)
            : QWidget(editor)
            , m_editor(editor)
        {
            setObjectName(QStringLiteral("previewLineNumbers"));
        }

    protected:
        void paintEvent(QPaintEvent *event) override
        {
            m_editor->paintLineNumbers(event);
        }

    private:
        PreviewTextEdit *m_editor;
    };

public:
    explicit PreviewTextEdit(QWidget *parent)
        : QPlainTextEdit(parent)
        , m_lineNumbers(new LineNumbers(this))
        , m_highlighter(new PreviewSyntaxHighlighter(document()))
    {
        setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        connect(this, &QPlainTextEdit::blockCountChanged, this, [this] {
            updateLineNumberWidth();
        });
        connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, int dy) {
            if (dy) {
                m_lineNumbers->scroll(0, dy);
            } else {
                m_lineNumbers->update(0, rect.y(), m_lineNumbers->width(), rect.height());
            }
            if (rect.contains(viewport()->rect())) {
                updateLineNumberWidth();
            }
        });
        updateLineNumberWidth();
        updateTheme();
    }

    ~PreviewTextEdit() override
    {
        delete m_highlighter;
    }

    bool setFileName(const QString &name, bool highlighting)
    {
        if (!highlighting) {
            m_highlighter->setDocument(nullptr);
            return false;
        }
        if (!m_highlighter->document()) {
            m_highlighter->setDocument(document());
        }
        const auto definition = m_repository.definitionForFileName(name);
        m_highlighter->setDefinition(definition);
        return definition.isValid();
    }

    void setLineNumbersVisible(bool visible)
    {
        m_showLineNumbers = visible;
        m_lineNumbers->setVisible(visible);
        updateLineNumberWidth();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QPlainTextEdit::resizeEvent(event);
        const QRect rect = contentsRect();
        const int width = lineNumberWidth();
        m_lineNumbers->setGeometry(layoutDirection() == Qt::RightToLeft ? rect.right() - width + 1 : rect.left(), rect.top(), width, rect.height());
    }

    void changeEvent(QEvent *event) override
    {
        QPlainTextEdit::changeEvent(event);
        if (event->type() == QEvent::PaletteChange) {
            updateTheme();
        } else if (event->type() == QEvent::LayoutDirectionChange) {
            updateLineNumberWidth();
        }
    }

private:
    int lineNumberWidth() const
    {
        return m_showLineNumbers ? 12 + fontMetrics().horizontalAdvance(LocalizedNumbers::shape(QLocale().toString(std::max(1, blockCount())))) : 0;
    }

    void updateLineNumberWidth()
    {
        const int width = lineNumberWidth();
        const bool rightToLeft = layoutDirection() == Qt::RightToLeft;
        setViewportMargins(rightToLeft ? 0 : width, 0, rightToLeft ? width : 0, 0);
        const QRect rect = contentsRect();
        m_lineNumbers->setGeometry(rightToLeft ? rect.right() - width + 1 : rect.left(), rect.top(), width, rect.height());
    }

    void updateTheme()
    {
        const auto theme =
            palette().base().color().lightness() < 128 ? KSyntaxHighlighting::Repository::DarkTheme : KSyntaxHighlighting::Repository::LightTheme;
        m_highlighter->setTheme(m_repository.defaultTheme(theme));
    }

    void paintLineNumbers(QPaintEvent *event)
    {
        QPainter painter(m_lineNumbers);
        painter.fillRect(event->rect(), palette().alternateBase());
        painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        QTextBlock block = firstVisibleBlock();
        int top = int(blockBoundingGeometry(block).translated(contentOffset()).top());
        while (block.isValid() && top <= event->rect().bottom()) {
            const int height = int(blockBoundingRect(block).height());
            if (block.isVisible() && top + height >= event->rect().top()) {
                const bool rightToLeft = layoutDirection() == Qt::RightToLeft;
                painter.drawText(rightToLeft ? 5 : 0,
                                 top,
                                 m_lineNumbers->width() - 5,
                                 fontMetrics().height(),
                                 rightToLeft ? Qt::AlignLeft : Qt::AlignRight,
                                 LocalizedNumbers::shape(QLocale().toString(block.blockNumber() + 1)));
            }
            top += height;
            block = block.next();
        }
    }

    LineNumbers *m_lineNumbers;
    KSyntaxHighlighting::Repository m_repository;
    PreviewSyntaxHighlighter *m_highlighter;
    bool m_showLineNumbers = true;
};

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent)
    , m_title(new QLabel(this))
    , m_message(new QLabel(this))
    , m_imageLabel(new ImagePreviewLabel(this))
    , m_footer(new QLabel(this))
    , m_previous(new QToolButton(this))
    , m_next(new QToolButton(this))
    , m_gallery(new QToolButton(this))
    , m_open(new QToolButton(this))
    , m_renderMode(new QToolButton(this))
    , m_autoplay(new QToolButton(this))
    , m_text(new PreviewTextEdit(this))
    , m_renderedText(new QTextBrowser(this))
    , m_content(new QStackedWidget(this))
{
    setObjectName(QStringLiteral("previewPanel"));
    m_preferFormatted = Settings::previewFormatted();
    m_showLineNumbers = Settings::previewLineNumbers();
    m_syntaxHighlighting = Settings::previewSyntaxHighlighting();
    m_text->setLineNumbersVisible(m_showLineNumbers);
    setMinimumWidth(240);
    m_title->setTextFormat(Qt::PlainText);
    m_title->setObjectName(QStringLiteral("previewTitle"));
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_message->setTextFormat(Qt::PlainText);
    m_message->setObjectName(QStringLiteral("previewMessage"));
    m_message->setWordWrap(true);
    m_message->setAlignment(Qt::AlignCenter);
    m_imageLabel->setObjectName(QStringLiteral("previewImage"));
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_imageLabel->installEventFilter(this);
    m_footer->setObjectName(QStringLiteral("previewFooter"));
    m_footer->setTextFormat(Qt::PlainText);
    m_footer->setWordWrap(true);
    m_footer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_text->setObjectName(QStringLiteral("previewText"));
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_renderedText->setObjectName(QStringLiteral("previewRenderedText"));
    m_renderedText->setDocument(new SafeTextDocument(m_renderedText));
    m_renderedText->setOpenLinks(false);
    m_renderedText->setOpenExternalLinks(false);
    m_renderedText->setLineWrapMode(QTextEdit::NoWrap);

    auto *close = new QToolButton(this);
    close->setObjectName(QStringLiteral("previewClose"));
    close->setIcon(SymbolicIcon::fromTheme(QStringLiteral("window-close")));
    close->setToolTip(i18nc("@info:tooltip", "Close preview"));
    close->setAccessibleName(i18nc("@action", "Close preview"));
    connect(close, &QToolButton::clicked, this, &PreviewPanel::closeRequested);
    m_header = new QHBoxLayout;
    m_header->setObjectName(QStringLiteral("previewHeaderLayout"));
    m_header->addWidget(m_title, 1);
    auto *placement = new QToolButton(this);
    placement->setObjectName(QStringLiteral("previewPlacement"));
    placement->setIcon(SymbolicIcon::fromTheme(QStringLiteral("configure")));
    placement->setToolTip(i18nc("@info:tooltip", "Preview panel placement"));
    placement->setAccessibleName(i18nc("@action", "Preview panel placement"));
    placement->setPopupMode(QToolButton::InstantPopup);
    auto *placementMenu = new QMenu(placement);
    m_placementActions = new QActionGroup(placementMenu);
    m_placementActions->setExclusive(true);
    const auto addPlacement = [this, placementMenu](const QString &name, const QString &text, Placement value) {
        QAction *action = placementMenu->addAction(text);
        action->setObjectName(name);
        action->setCheckable(true);
        action->setData(static_cast<int>(value));
        m_placementActions->addAction(action);
        connect(action, &QAction::triggered, this, [this, value] {
            Q_EMIT placementRequested(value);
        });
    };
    addPlacement(QStringLiteral("previewPlacePane"), i18nc("@action", "Pane"), Placement::Pane);
    addPlacement(QStringLiteral("previewPlaceBelow"), i18nc("@action", "Below Panes"), Placement::Below);
    addPlacement(QStringLiteral("previewPlaceRight"), i18nc("@action", "Right of Panes"), Placement::Right);
    placement->setMenu(placementMenu);
    m_previous->setObjectName(QStringLiteral("previewPrevious"));
    m_next->setObjectName(QStringLiteral("previewNext"));
    m_gallery->setObjectName(QStringLiteral("previewGallery"));
    m_open->setObjectName(QStringLiteral("previewOpen"));
    m_renderMode->setObjectName(QStringLiteral("previewRenderMode"));
    m_renderMode->setCheckable(true);
    m_renderMode->setText(i18nc("@action", "Render"));
    m_renderMode->setToolTip(i18nc("@info:tooltip", "Show formatted text instead of source"));
    connect(m_renderMode, &QToolButton::toggled, this, &PreviewPanel::updateTextMode);
    m_renderMode->hide();
    m_autoplay->setObjectName(QStringLiteral("previewAutoplay"));
    m_autoplay->setText(i18nc("@action:button", "Autoplay"));
    m_autoplay->setToolTip(i18nc("@info:tooltip", "Automatically play audio and video previews"));
    m_autoplay->setCheckable(true);
    m_autoplay->setChecked(Settings::previewMediaAutoplay());
    m_autoplay->hide();
    connect(m_autoplay, &QToolButton::toggled, this, [this](bool enabled) {
        Settings::setPreviewMediaAutoplay(enabled);
        Settings::self()->save();
        if (enabled && m_media && m_media->player()) {
            m_media->player()->play();
        }
    });
    m_previous->setIcon(SymbolicIcon::fromTheme(QStringLiteral("go-previous")));
    m_previous->setToolTip(i18nc("@info:tooltip", "Previous image"));
    m_next->setIcon(SymbolicIcon::fromTheme(QStringLiteral("go-next")));
    m_next->setToolTip(i18nc("@info:tooltip", "Next image"));
    m_gallery->setIcon(SymbolicIcon::fromTheme(QStringLiteral("view-media-preview")));
    m_gallery->setToolTip(i18nc("@info:tooltip", "Open gallery"));
    m_open->setText(i18nc("@action", "Open"));
    connect(m_previous, &QToolButton::clicked, this, [this] {
        const int index = m_images.indexOf(m_url);
        if (index > 0) {
            Q_EMIT imageRequested(m_images.at(index - 1));
        }
    });
    connect(m_next, &QToolButton::clicked, this, [this] {
        const int index = m_images.indexOf(m_url);
        if (index >= 0 && index + 1 < m_images.size()) {
            Q_EMIT imageRequested(m_images.at(index + 1));
        }
    });
    connect(m_gallery, &QToolButton::clicked, this, &PreviewPanel::galleryRequested);
    connect(m_open, &QToolButton::clicked, this, &PreviewPanel::openRequested);
    m_header->addWidget(m_previous);
    m_header->addWidget(m_next);
    m_header->addWidget(m_gallery);
    m_header->addWidget(m_renderMode);
    m_header->addWidget(m_autoplay);
    m_header->addWidget(placement);
    m_header->addWidget(close);

    m_content->addWidget(m_message);
    m_content->addWidget(m_imageLabel);
    m_content->addWidget(m_text);
    m_content->addWidget(m_renderedText);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(qMax(8, layout->contentsMargins().left()), -1, qMax(8, layout->contentsMargins().right()), -1);
    layout->addLayout(m_header);
    layout->addWidget(m_content, 1);
    m_footerLayout = new QHBoxLayout;
    m_footerLayout->setObjectName(QStringLiteral("previewFooterLayout"));
    m_footerLayout->addWidget(m_footer, 1);
    m_footerLayout->addWidget(m_open);
    layout->addLayout(m_footerLayout);
    clear();
}

void PreviewPanel::setTitleVisible(bool visible)
{
    if (m_titleVisible != visible) {
        m_titleVisible = visible;
        if (visible) {
            m_header->insertWidget(0, m_title, 1);
        } else {
            m_footerLayout->insertWidget(0, m_title, 1);
        }
    }
    m_title->setVisible(visible || !m_url.isEmpty());
}

void PreviewPanel::setPlacement(Placement placement)
{
    for (QAction *action : m_placementActions->actions()) {
        action->setChecked(action->data().toInt() == static_cast<int>(placement));
    }
}

PreviewPanel::~PreviewPanel()
{
    clearDocument();
    clearMedia();
    clearRemotePreview();
}

void PreviewPanel::applyPresentationSettings()
{
    m_imageLabel->update();
    const bool formattedChanged = m_preferFormatted != Settings::previewFormatted();
    const bool changed =
        formattedChanged || m_showLineNumbers != Settings::previewLineNumbers() || m_syntaxHighlighting != Settings::previewSyntaxHighlighting();
    m_preferFormatted = Settings::previewFormatted();
    m_showLineNumbers = Settings::previewLineNumbers();
    m_syntaxHighlighting = Settings::previewSyntaxHighlighting();
    m_text->setLineNumbersVisible(m_showLineNumbers);
    if (!changed || m_textMimeType.isEmpty()) {
        return;
    }
    if (formattedChanged && !m_renderMode->isHidden()) {
        const QSignalBlocker blocker(m_renderMode);
        m_renderMode->setChecked(m_preferFormatted);
    }
    updateTextMode();
}

void PreviewPanel::setItems(const KFileItemList &items, const QList<QUrl> &images)
{
    applyPresentationSettings();
    if (items.size() != 1 || items.first().isDir()) {
        clear();
        return;
    }

    const KFileItem item = items.first();
    m_images = images;
    const int imageIndex = m_images.indexOf(item.url());
    m_previous->setEnabled(imageIndex > 0);
    m_next->setEnabled(imageIndex >= 0 && imageIndex + 1 < m_images.size());
    m_gallery->setEnabled(item.url().isLocalFile()
                          && (item.mimetype().startsWith(QLatin1String("image/")) || item.mimetype().startsWith(QLatin1String("video/"))));
    m_open->setEnabled(true);
    m_title->setText(imageIndex >= 0 ? i18nc("@title", "%1 (%2 of %3)", item.text(), imageIndex + 1, m_images.size()) : item.text());
    m_title->show();
    const qint64 textLimit = qint64(Settings::textPreviewLimitKiB()) * 1024;
    if (item.url() == m_url && textLimit == m_textLimit) {
        return;
    }
    m_url = item.url();
    m_textLimit = textLimit;
    ++m_generation;
    m_pendingUrl = {};
    m_pendingMimeType.clear();
    m_pendingFile.clear();
    m_metadataUrl = {};
    m_metadataMimeType.clear();
    m_metadataFile.clear();
    m_metadataLabels.clear();
    m_truncated = false;
    m_longLines = false;
    m_textMimeType.clear();
    m_textContent.clear();
    m_renderMode->hide();
    clearDocument();
    clearMedia();
    clearRemotePreview();
    const QString modified = LocalizedDates::fileTime(item);
    const QString size = LocalizedNumbers::shape(KIO::convertSize(item.size()));
    m_baseFooter = modified.isEmpty() ? size : i18nc("@info", "%1 · %2", size, modified);
    m_footer->setText(m_baseFooter);
    showMessage(i18nc("@info", "Loading preview…"));

    if (!m_url.isLocalFile()) {
        startRemotePreview(m_url);
        return;
    }
    const QString mimeType = m_url.isLocalFile() ? item.mimetype() : QString();
    scheduleMetadata(m_url, mimeType);
    if (m_url.isLocalFile() && isDocumentMime(mimeType)) {
        showDocument(m_url, mimeType);
        return;
    }
    if (m_url.isLocalFile() && (mimeType.startsWith(QLatin1String("audio/")) || mimeType.startsWith(QLatin1String("video/")))) {
        showMedia(m_url, mimeType.startsWith(QLatin1String("video/")));
        return;
    }
    m_pendingUrl = m_url;
    m_pendingMimeType = mimeType;
    if (!m_loading) {
        startPendingPreview();
    }
}

void PreviewPanel::startPendingPreview()
{
    const quint64 generation = m_generation;
    const QUrl url = std::exchange(m_pendingUrl, {});
    const QString mimeType = std::exchange(m_pendingMimeType, {});
    const QSharedPointer<QTemporaryFile> file = std::exchange(m_pendingFile, {});
    const qint64 textLimit = m_textLimit;
    m_loading = true;
    auto *watcher = new QFutureWatcher<PreviewContent>(this);
    connect(watcher, &QFutureWatcher<PreviewContent>::finished, this, [this, watcher, generation, file, mimeType] {
        const PreviewContent content = watcher->result();
        watcher->deleteLater();
        m_loading = false;
        if (generation == m_generation) {
            showContent(content, mimeType);
        }
        if (!m_pendingUrl.isEmpty()) {
            startPendingPreview();
        }
    });
    watcher->setFuture(QtConcurrent::run([url, mimeType, textLimit] {
        return Preview::load(url, mimeType, textLimit);
    }));
}

void PreviewPanel::clear()
{
    ++m_generation;
    m_url = {};
    m_pendingUrl = {};
    m_pendingMimeType.clear();
    m_pendingFile.clear();
    m_images.clear();
    m_baseFooter.clear();
    m_metadataUrl = {};
    m_metadataMimeType.clear();
    m_metadataFile.clear();
    m_metadataLabels.clear();
    m_truncated = false;
    m_longLines = false;
    m_textMimeType.clear();
    m_textContent.clear();
    m_renderMode->hide();
    clearDocument();
    clearMedia();
    clearRemotePreview();
    m_title->setText(i18nc("@title", "Preview"));
    m_title->setVisible(m_titleVisible);
    m_footer->clear();
    m_previous->setEnabled(false);
    m_next->setEnabled(false);
    m_gallery->setEnabled(false);
    m_open->setEnabled(false);
    showMessage(i18nc("@info", "Select one file to preview."));
}

void PreviewPanel::startRemotePreview(const QUrl &url)
{
    QString suffix = QFileInfo(url.path()).suffix();
    static const QRegularExpression safeSuffix(QStringLiteral("^[A-Za-z0-9]{1,12}$"));
    if (!safeSuffix.match(suffix).hasMatch()) {
        suffix = QStringLiteral("tmp");
    }
    m_remoteFile = QSharedPointer<QTemporaryFile>::create(QDir::temp().filePath(QStringLiteral("opus-preview-XXXXXX.%1").arg(suffix)));
    if (!m_remoteFile->open()) {
        const QString error = m_remoteFile->errorString();
        m_remoteFile.clear();
        showMessage(i18nc("@info", "Could not create a temporary preview: %1", error));
        return;
    }
    m_remoteMimeType = QMimeDatabase().mimeTypeForUrl(url).name();
    m_remoteJob = KIO::get(url, KIO::NoReload, KIO::HideProgressInfo);
    const QPointer<KIO::TransferJob> transfer(m_remoteJob);
    connect(m_remoteJob, &KIO::TransferJob::mimeTypeFound, this, [this, transfer](KIO::Job *, const QString &mimeType) {
        if (transfer && m_remoteJob == transfer && mimeType != QLatin1String("application/octet-stream")) {
            m_remoteMimeType = mimeType;
        }
    });
    connect(m_remoteJob, &KIO::TransferJob::data, this, [this, transfer](KIO::Job *, const QByteArray &data) {
        if (!transfer || m_remoteJob != transfer || data.isEmpty()) {
            return;
        }
        const bool text = isTextMime(m_remoteMimeType);
        const bool image = m_remoteMimeType.startsWith(QLatin1String("image/"));
        const qint64 limit = text ? m_textLimit : image ? ImageLimit : RemoteFileLimit;
        const qint64 remaining = limit + 1 - m_remoteFile->size();
        const qint64 amount = std::min<qint64>(data.size(), std::max<qint64>(0, remaining));
        if (m_remoteFile->write(data.constData(), amount) != amount) {
            const QString error = m_remoteFile->errorString();
            clearRemotePreview();
            showMessage(i18nc("@info", "Could not save the remote preview: %1", error));
            return;
        }
        if (m_remoteFile->size() > limit) {
            if (text) {
                disconnect(m_remoteJob, nullptr, this, nullptr);
                std::exchange(m_remoteJob, nullptr)->kill(KJob::Quietly);
                finishRemotePreview();
            } else {
                clearRemotePreview();
                showMessage(i18nc("@info", "This remote file is too large to preview."));
            }
        }
    });
    connect(m_remoteJob, &KJob::result, this, [this, transfer](KJob *job) {
        if (!transfer || m_remoteJob != transfer) {
            return;
        }
        m_remoteJob = nullptr;
        if (job->error()) {
            const QString error = job->errorString();
            clearRemotePreview();
            showMessage(i18nc("@info", "Could not load the remote preview: %1", error));
            return;
        }
        finishRemotePreview();
    });
}

void PreviewPanel::finishRemotePreview()
{
    if (!m_remoteFile->flush()) {
        const QString error = m_remoteFile->errorString();
        clearRemotePreview();
        showMessage(i18nc("@info", "Could not save the remote preview: %1", error));
        return;
    }
    m_remoteFile->close();
    const QUrl localUrl = QUrl::fromLocalFile(m_remoteFile->fileName());
    if (m_remoteMimeType.isEmpty() || m_remoteMimeType == QLatin1String("application/octet-stream")) {
        QFile file(localUrl.toLocalFile());
        if (!file.open(QIODevice::ReadOnly)) {
            const QString error = file.errorString();
            clearRemotePreview();
            showMessage(i18nc("@info", "Could not load the remote preview: %1", error));
            return;
        }
        m_remoteMimeType = QMimeDatabase().mimeTypeForData(file.peek(16384)).name();
    }
    scheduleMetadata(localUrl, m_remoteMimeType, m_remoteFile);
    if (isDocumentMime(m_remoteMimeType)) {
        m_renderFile = std::exchange(m_remoteFile, {});
        showDocument(localUrl, m_remoteMimeType);
    } else if (m_remoteMimeType.startsWith(QLatin1String("audio/")) || m_remoteMimeType.startsWith(QLatin1String("video/"))) {
        m_renderFile = std::exchange(m_remoteFile, {});
        showMedia(localUrl, m_remoteMimeType.startsWith(QLatin1String("video/")));
    } else {
        m_pendingUrl = localUrl;
        m_pendingMimeType = m_remoteMimeType;
        m_pendingFile = std::exchange(m_remoteFile, {});
        if (!m_loading) {
            startPendingPreview();
        }
    }
}

void PreviewPanel::clearRemotePreview()
{
    if (m_remoteJob) {
        auto *job = std::exchange(m_remoteJob, nullptr);
        disconnect(job, nullptr, this, nullptr);
        job->kill(KJob::Quietly);
    }
    m_remoteFile.clear();
    m_renderFile.clear();
    m_remoteMimeType.clear();
}

void PreviewPanel::showMedia(const QUrl &url, bool video)
{
    if (!QFileInfo(url.toLocalFile()).isFile()) {
        showMessage(i18nc("@info", "Could not open this media file."));
        return;
    }
    if (!m_media) {
        m_media = new MediaView(m_content);
        m_media->setObjectName(QStringLiteral("previewMedia"));
        m_content->addWidget(m_media);
        connect(m_media, &MediaView::playbackError, this, [this](const QString &message) {
            showMessage(i18nc("@info", "Could not open media: %1", message));
        });
    }
    {
        const QSignalBlocker blocker(m_autoplay);
        m_autoplay->setChecked(Settings::previewMediaAutoplay());
    }
    m_autoplay->show();
    m_content->setCurrentWidget(m_media);
    m_media->open(url, video, Settings::previewMediaAutoplay());
}

void PreviewPanel::clearMedia()
{
    m_autoplay->hide();
    if (m_media) {
        m_media->clear();
    }
}

void PreviewPanel::showDocument(const QUrl &url, const QString &mimeType)
{
    const QFileInfo file(url.toLocalFile());
    if (!file.isFile()) {
        showMessage(i18nc("@info", "Could not open this document."));
        return;
    }
    const auto result = KParts::PartLoader::instantiatePartForMimeType<KParts::ReadOnlyPart>(mimeType, m_content, this);
    if (!result) {
        showMessage(result.errorString.isEmpty() ? i18nc("@info", "No document viewer is installed for this file type.")
                                                 : i18nc("@info", "Could not load a document viewer: %1", result.errorString));
        return;
    }
    if (!result.plugin->widget()) {
        delete result.plugin;
        showMessage(i18nc("@info", "The document viewer did not provide a preview."));
        return;
    }
    m_document = result.plugin;
    connect(m_document, &QObject::destroyed, this, [this] {
        m_document = nullptr;
    });
    QWidget *documentWidget = m_document->widget();
    m_content->addWidget(documentWidget);
    const QPointer<KParts::ReadOnlyPart> document(m_document);
    connect(m_document, &KParts::ReadOnlyPart::canceled, this, [this, document](const QString &message) {
        if (document && m_document == document) {
            showMessage(message.isEmpty() ? i18nc("@info", "Could not open this document.") : i18nc("@info", "Could not open document: %1", message));
        }
    });
    m_content->setCurrentWidget(documentWidget);
    if (!m_document->openUrl(url) && m_content->currentWidget() == documentWidget) {
        showMessage(i18nc("@info", "Could not open this document."));
    }
}

void PreviewPanel::clearDocument()
{
    if (m_document) {
        auto *document = std::exchange(m_document, nullptr);
        disconnect(document, nullptr, this, nullptr);
        delete document;
    }
}

void PreviewPanel::showMessage(const QString &message)
{
    m_image = {};
    m_imageLabel->clear();
    m_text->clear();
    m_renderedText->clear();
    m_message->setText(message);
    m_content->setCurrentWidget(m_message);
}

void PreviewPanel::showContent(const PreviewContent &content, const QString &mimeType)
{
    switch (content.kind) {
    case PreviewContent::Kind::Text:
        m_textContent = content.text;
        m_textMimeType = mimeType;
        m_renderMode->setVisible(mimeType == QLatin1String("text/html") || mimeType == QLatin1String("application/xhtml+xml")
                                 || mimeType == QLatin1String("text/markdown"));
        m_renderMode->setChecked(!m_renderMode->isHidden() && m_preferFormatted);
        updateTextMode();
        m_truncated = content.truncated;
        updateFooter();
        break;
    case PreviewContent::Kind::Image:
        m_image = content.image;
        m_content->setCurrentWidget(m_imageLabel);
        updateImage();
        break;
    case PreviewContent::Kind::Remote:
        showMessage(i18nc("@info", "Previewing remote files is not supported yet."));
        break;
    case PreviewContent::Kind::TooLarge:
        showMessage(i18nc("@info", "This image is too large to preview."));
        break;
    case PreviewContent::Kind::Error:
        showMessage(i18nc("@info", "Could not load preview: %1", content.text));
        break;
    case PreviewContent::Kind::Unsupported:
        showMessage(i18nc("@info", "No preview is available for this file."));
        break;
    }
}

void PreviewPanel::updateTextMode()
{
    if (!m_renderMode->isHidden() && m_renderMode->isChecked()) {
        m_longLines = false;
        if (m_textMimeType == QLatin1String("text/html") || m_textMimeType == QLatin1String("application/xhtml+xml")) {
            m_renderedText->setHtml(m_textContent);
        } else {
            m_renderedText->setMarkdown(m_textContent);
        }
        m_content->setCurrentWidget(m_renderedText);
    } else {
        const bool highlighted = m_text->setFileName(m_url.fileName(), m_syntaxHighlighting);
        m_longLines = false;
        if (highlighted) {
            qsizetype start = 0;
            while (start < m_textContent.size()) {
                const qsizetype next = m_textContent.indexOf(QLatin1Char('\n'), start);
                const qsizetype end = next < 0 ? m_textContent.size() : next;
                if (end - start > HighlightLineLimit) {
                    m_longLines = true;
                    break;
                }
                start = end + 1;
            }
        }
        m_text->setPlainText(m_textContent);
        m_content->setCurrentWidget(m_text);
    }
    updateFooter();
}

void PreviewPanel::scheduleMetadata(const QUrl &url, const QString &mimeType, const QSharedPointer<QTemporaryFile> &file)
{
    if (!mimeType.startsWith(QLatin1String("image/")) && !mimeType.startsWith(QLatin1String("audio/")) && !mimeType.startsWith(QLatin1String("video/"))) {
        return;
    }
    m_metadataUrl = url;
    m_metadataMimeType = mimeType;
    m_metadataFile = file;
    if (!m_metadataLoading) {
        startMetadata();
    }
}

void PreviewPanel::startMetadata()
{
    const quint64 generation = m_generation;
    const QUrl url = std::exchange(m_metadataUrl, {});
    const QString mimeType = std::exchange(m_metadataMimeType, {});
    const QSharedPointer<QTemporaryFile> file = std::exchange(m_metadataFile, {});
    m_metadataLoading = true;
    auto *watcher = new QFutureWatcher<KFileMetaData::PropertyMultiMap>(this);
    connect(watcher, &QFutureWatcher<KFileMetaData::PropertyMultiMap>::finished, this, [this, watcher, generation, mimeType] {
        const auto metadata = watcher->result();
        watcher->deleteLater();
        m_metadataLoading = false;
        if (generation == m_generation) {
            using namespace KFileMetaData;
            const QList<Property::Property> properties = mimeType.startsWith(QLatin1String("image/"))
                ? QList<Property::Property>{Property::Manufacturer,
                                            Property::Model,
                                            Property::PhotoDateTimeOriginal,
                                            Property::PhotoISOSpeedRatings,
                                            Property::PhotoFNumber,
                                            Property::PhotoExposureTime,
                                            Property::PhotoFocalLength}
                : QList<Property::Property>{Property::Title, Property::Artist, Property::Album, Property::ReleaseYear};
            m_metadataLabels.clear();
            // Properties turned off in Preferences › Tagging › Embedded Metadata aren't shown.
            const QStringList disabled = Settings::disabledEmbeddedMetadataKeys();
            for (const Property::Property property : properties) {
                const auto values = metadata.values(property);
                if (!values.isEmpty() && !disabled.contains(PropertyInfo(property).name())) {
                    const PropertyInfo info(property);
                    const QString value = info.formatAsDisplayString(values.first()).left(160);
                    if (!value.isEmpty()) {
                        m_metadataLabels.append(i18nc("@info metadata label and value", "%1: %2", info.displayName(), value));
                    }
                }
            }
            updateFooter();
        }
        if (!m_metadataUrl.isEmpty()) {
            startMetadata();
        }
    });
    watcher->setFuture(QtConcurrent::run([url, mimeType, file] {
        Q_UNUSED(file);
        return ImageDetails::extractMetadata(url.toLocalFile(), mimeType);
    }));
}

void PreviewPanel::updateFooter()
{
    QStringList parts{m_baseFooter};
    parts.append(m_metadataLabels);
    if (m_truncated) {
        parts.append(i18nc("@info", "Preview truncated."));
    }
    if (m_longLines) {
        parts.append(i18nc("@info", "Very long lines are not syntax-highlighted."));
    }
    m_footer->setText(parts.join(QStringLiteral(" · ")));
}

bool PreviewPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_imageLabel && event->type() == QEvent::Resize) {
        updateImage();
    }
    return QWidget::eventFilter(watched, event);
}

void PreviewPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateImage();
}

void PreviewPanel::updateImage()
{
    if (!m_image.isNull() && !m_imageLabel->size().isEmpty()) {
        const QSize target = m_imageLabel->size() * m_imageLabel->devicePixelRatioF();
        QPixmap pixmap = QPixmap::fromImage(m_image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        pixmap.setDevicePixelRatio(m_imageLabel->devicePixelRatioF());
        m_imageLabel->setPixmap(pixmap);
    }
}
