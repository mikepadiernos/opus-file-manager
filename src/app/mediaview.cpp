// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediaview.h"

#include <KLocalizedString>

#include <QAudioOutput>
#include <QHBoxLayout>
#include <QMediaPlayer>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QVideoWidget>

#include <algorithm>
#include <limits>
#include <utility>

MediaView::MediaView(QWidget *parent)
    : QWidget(parent)
    , m_video(new QVideoWidget(this))
    , m_play(new QPushButton(i18nc("@action", "Play"), this))
    , m_seek(new QSlider(Qt::Horizontal, this))
{
    m_seek->setRange(0, 0);
    auto *controls = new QHBoxLayout;
    controls->addWidget(m_play);
    controls->addWidget(m_seek, 1);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_video, 1);
    layout->addLayout(controls);
    connect(m_play, &QPushButton::clicked, this, [this] {
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
        if (m_player) {
            m_player->setPosition(qint64(value) * 1000);
        }
    });
}

MediaView::~MediaView()
{
    clear();
}

void MediaView::open(const QUrl &url, bool video, bool autoplay)
{
    clear();
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
    m_video->setVisible(video);
    m_player->setSource(url);
    if (autoplay) {
        m_player->play();
    }
}

void MediaView::clear()
{
    if (m_player) {
        auto *player = std::exchange(m_player, nullptr);
        disconnect(player, nullptr, this, nullptr);
        player->stop();
        delete player;
    }
    m_play->setText(i18nc("@action", "Play"));
    m_seek->setRange(0, 0);
}

QMediaPlayer *MediaView::player() const
{
    return m_player;
}
