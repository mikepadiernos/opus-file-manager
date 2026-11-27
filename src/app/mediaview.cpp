// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediaview.h"

#include "mediabackend.h"
#include "settings.h"

#ifdef OPUSFM_HAVE_MPV
#include "mpvplayer.h"
#include "mpvvideowidget.h"
#endif

#include <KLocalizedString>

#include <QActionGroup>
#include <QAudioOutput>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
QHash<QString, QString> s_mpvOptions;

#ifdef OPUSFM_HAVE_MPV
int toSeconds(double seconds)
{
    return int(std::clamp<double>(std::floor(seconds), 0, std::numeric_limits<int>::max()));
}
#endif

MediaBackend::Kind requestedBackend()
{
    return Settings::mediaBackend() == Settings::EnumMediaBackend::Mpv ? MediaBackend::Kind::Mpv : MediaBackend::Kind::QtMultimedia;
}
}

MediaView::MediaView(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_notice(new QLabel(this))
    , m_video(new QVideoWidget(this))
    , m_play(new QPushButton(i18nc("@action", "Play"), this))
    , m_seek(new QSlider(Qt::Horizontal, this))
    , m_backend(new QToolButton(this))
{
    m_notice->setObjectName(QStringLiteral("mediaNotice"));
    m_notice->setWordWrap(true);
    m_notice->hide();
    m_seek->setRange(0, 0);

    m_backend->setObjectName(QStringLiteral("mediaBackend"));
    m_backend->setPopupMode(QToolButton::InstantPopup);
    m_backend->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_backend->setAccessibleName(i18nc("@action:button", "Playback backend"));
    auto *menu = new QMenu(m_backend);
    auto *group = new QActionGroup(menu);
    QAction *qt = menu->addAction(i18nc("@item:inmenu playback backend", "Qt Multimedia"));
    qt->setObjectName(QStringLiteral("mediaBackendQtMultimedia"));
    QAction *mpv = menu->addAction(i18nc("@item:inmenu playback backend", "mpv"));
    mpv->setObjectName(QStringLiteral("mediaBackendMpv"));
    for (QAction *action : {qt, mpv}) {
        action->setCheckable(true);
        group->addAction(action);
    }
    if (!MediaBackend::mpvBuilt()) {
        mpv->setEnabled(false);
        mpv->setToolTip(i18nc("@info:tooltip", "This build of Plasma Opus has no mpv support"));
        menu->setToolTipsVisible(true);
    }
    m_backend->setMenu(menu);
    const auto choose = [this](Settings::EnumMediaBackend::type backend) {
        if (Settings::mediaBackend() == backend) {
            return;
        }
        Settings::setMediaBackend(backend);
        Settings::self()->save();
        updateBackendButton();
        if (m_url.isValid()) {
            bool playing = m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
#ifdef OPUSFM_HAVE_MPV
            playing = playing || (m_mpv && !m_mpv->isPaused());
#endif
            open(QUrl(m_url), m_isVideo, playing);
        }
    };
    connect(qt, &QAction::triggered, this, [choose] {
        choose(Settings::EnumMediaBackend::QtMultimedia);
    });
    connect(mpv, &QAction::triggered, this, [choose] {
        choose(Settings::EnumMediaBackend::Mpv);
    });
    updateBackendButton();

    auto *controls = new QHBoxLayout;
    controls->addWidget(m_play);
    controls->addWidget(m_seek, 1);
    controls->addWidget(m_backend);
    m_layout->addWidget(m_notice);
    m_layout->addWidget(m_video, 1);
    m_layout->addLayout(controls);
    connect(m_play, &QPushButton::clicked, this, [this] {
#ifdef OPUSFM_HAVE_MPV
        if (m_mpv) {
            m_mpv->setPaused(!m_mpv->isPaused());
            return;
        }
#endif
        if (!m_player) {
            return;
        }
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_player->pause();
        } else {
            m_player->play();
        }
    });
    connect(m_seek, &QSlider::sliderMoved, this, [this](int value) {
#ifdef OPUSFM_HAVE_MPV
        if (m_mpv) {
            m_mpv->seek(value);
            return;
        }
#endif
        if (m_player) {
            m_player->setPosition(qint64(value) * 1000);
        }
    });
}

MediaView::~MediaView()
{
    clear();
}

void MediaView::setMpvOptions(const QHash<QString, QString> &options)
{
    s_mpvOptions = options;
}

void MediaView::open(const QUrl &url, bool video, bool autoplay)
{
    clear();
    m_url = url;
    m_isVideo = video;
    m_autoplay = autoplay;
    updateBackendButton();
    const MediaBackend::Kind requested = requestedBackend();
    MediaBackend::Capabilities capabilities;
    capabilities.mpvBuilt = MediaBackend::mpvBuilt();
#ifdef OPUSFM_HAVE_MPV
    // Creating a test context is only worth it when mpv would draw video.
    capabilities.openGl = requested == MediaBackend::Kind::Mpv && video && MpvVideoWidget::openGlAvailable();
#endif
    const MediaBackend::Choice choice = MediaBackend::choose(requested, video, capabilities);
    showNotice(choice.notice);
    if (choice.kind == MediaBackend::Kind::Mpv && openMpv(autoplay)) {
        return;
    }
    openQtMultimedia(autoplay);
}

void MediaView::openQtMultimedia(bool autoplay)
{
    m_player = new QMediaPlayer(this);
    m_player->setObjectName(QStringLiteral("mediaPlayer"));
    m_player->setAudioOutput(new QAudioOutput(m_player));
    m_player->setVideoOutput(m_video);
    const QPointer<QMediaPlayer> player(m_player);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this, player](QMediaPlayer::PlaybackState state) {
        if (player && m_player == player) {
            m_play->setText(state == QMediaPlayer::PlayingState ? i18nc("@action", "Pause") : i18nc("@action", "Play"));
        }
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this, player](qint64 duration) {
        if (player && m_player == player) {
            m_seek->setRange(0, int(std::min<qint64>(duration / 1000, std::numeric_limits<int>::max())));
        }
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this, player](qint64 position) {
        if (player && m_player == player && !m_seek->isSliderDown()) {
            m_seek->setValue(int(std::min<qint64>(position / 1000, std::numeric_limits<int>::max())));
        }
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this, player](QMediaPlayer::Error, const QString &message) {
        if (player && m_player == player) {
            Q_EMIT playbackError(message);
        }
    });
    m_video->setVisible(m_isVideo);
    m_player->setSource(m_url);
    if (autoplay) {
        m_player->play();
    }
}

bool MediaView::openMpv(bool autoplay)
{
#ifdef OPUSFM_HAVE_MPV
    m_mpv = new MpvPlayer(s_mpvOptions, this);
    m_mpv->setObjectName(QStringLiteral("mpvPlayer"));
    if (!m_mpv->isValid()) {
        const QString reason = m_mpv->errorString();
        releaseMpv();
        showNotice(MediaBackend::startFailedNotice(reason));
        return false;
    }
    connect(m_mpv, &MpvPlayer::pausedChanged, this, [this](bool paused) {
        m_play->setText(paused ? i18nc("@action", "Play") : i18nc("@action", "Pause"));
    });
    connect(m_mpv, &MpvPlayer::durationChanged, this, [this](double seconds) {
        m_seek->setRange(0, toSeconds(seconds));
    });
    connect(m_mpv, &MpvPlayer::positionChanged, this, [this](double seconds) {
        if (!m_seek->isSliderDown()) {
            m_seek->setValue(toSeconds(seconds));
        }
    });
    connect(m_mpv, &MpvPlayer::errorOccurred, this, &MediaView::playbackError);
    m_video->hide();
    if (m_isVideo) {
        m_mpvVideo = new MpvVideoWidget(m_mpv, this);
        m_layout->insertWidget(m_layout->indexOf(m_video), m_mpvVideo, 1);
        connect(m_mpvVideo, &MpvVideoWidget::renderingFailed, this, [this](const QString &message) {
            fallBackToQtMultimedia(MediaBackend::startFailedNotice(message));
        });
    }
    m_mpv->load(m_url.toLocalFile(), m_isVideo, autoplay);
    return true;
#else
    Q_UNUSED(autoplay)
    return false;
#endif
}

void MediaView::fallBackToQtMultimedia(const QString &notice)
{
    releaseMpv();
    showNotice(notice);
    openQtMultimedia(m_autoplay);
}

void MediaView::releaseMpv()
{
#ifdef OPUSFM_HAVE_MPV
    // The video widget frees its render context, which must happen before the player stops.
    delete std::exchange(m_mpvVideo, nullptr);
    if (auto *mpv = std::exchange(m_mpv, nullptr)) {
        disconnect(mpv, nullptr, this, nullptr);
        delete mpv;
    }
#endif
}

void MediaView::clear()
{
    if (m_player) {
        auto *player = std::exchange(m_player, nullptr);
        disconnect(player, nullptr, this, nullptr);
        player->stop();
        delete player;
    }
    releaseMpv();
    m_url.clear();
    showNotice({});
    m_play->setText(i18nc("@action", "Play"));
    m_seek->setRange(0, 0);
}

QMediaPlayer *MediaView::player() const
{
    return m_player;
}

MpvPlayer *MediaView::mpvPlayer() const
{
    return m_mpv;
}

void MediaView::showNotice(const QString &notice)
{
    m_notice->setText(notice);
    m_notice->setVisible(!notice.isEmpty());
}

void MediaView::updateBackendButton()
{
    const bool mpv = requestedBackend() == MediaBackend::Kind::Mpv;
    m_backend->setText(mpv ? i18nc("@action:button playback backend", "mpv") : i18nc("@action:button playback backend", "Qt Multimedia"));
    m_backend->setToolTip(i18nc("@info:tooltip", "Choose whether Qt Multimedia or mpv plays audio and video"));
    const QList<QAction *> actions = m_backend->menu()->actions();
    actions.at(0)->setChecked(!mpv);
    actions.at(1)->setChecked(mpv);
}
