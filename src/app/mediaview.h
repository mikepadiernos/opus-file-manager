// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QUrl>
#include <QWidget>

class MpvPlayer;
class MpvVideoWidget;
class QLabel;
class QMediaPlayer;
class QPushButton;
class QSlider;
class QToolButton;
class QVBoxLayout;
class QVideoWidget;

/**
 * Plays audio and video in the preview panel and gallery through Qt
 * Multimedia or, when chosen and available, the embedded mpv backend.
 */
class MediaView : public QWidget
{
    Q_OBJECT

public:
    explicit MediaView(QWidget *parent = nullptr);
    ~MediaView() override;
    void open(const QUrl &url, bool video, bool autoplay);
    void clear();
    /// The Qt Multimedia player, or null while mpv plays the file.
    QMediaPlayer *player() const;
    /// The mpv player, or null while Qt Multimedia plays the file.
    MpvPlayer *mpvPlayer() const;

    /// Extra libmpv options for players created afterwards, such as `ao=null` in tests.
    static void setMpvOptions(const QHash<QString, QString> &options);

Q_SIGNALS:
    void playbackError(const QString &message);

private:
    void openQtMultimedia(bool autoplay);
    bool openMpv(bool autoplay);
    void fallBackToQtMultimedia(const QString &notice);
    void releaseMpv();
    void showNotice(const QString &notice);
    void updateBackendButton();

    QUrl m_url;
    bool m_isVideo = false;
    bool m_autoplay = false;
    QVBoxLayout *m_layout = nullptr;
    QLabel *m_notice = nullptr;
    QMediaPlayer *m_player = nullptr;
    QVideoWidget *m_video = nullptr;
    MpvPlayer *m_mpv = nullptr;
    MpvVideoWidget *m_mpvVideo = nullptr;
    QPushButton *m_play = nullptr;
    QSlider *m_seek = nullptr;
    QToolButton *m_backend = nullptr;
    /// The backend that the button shows, which the open file was last opened with.
    bool m_showsMpv = false;
};
