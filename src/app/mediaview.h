// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QMediaPlayer;
class QPushButton;
class QSlider;
class QUrl;
class QVideoWidget;

class MediaView : public QWidget
{
    Q_OBJECT

public:
    explicit MediaView(QWidget *parent = nullptr);
    ~MediaView() override;
    void open(const QUrl &url, bool video, bool autoplay);
    void clear();
    QMediaPlayer *player() const;

Q_SIGNALS:
    void playbackError(const QString &message);

private:
    QMediaPlayer *m_player = nullptr;
    QVideoWidget *m_video = nullptr;
    QPushButton *m_play = nullptr;
    QSlider *m_seek = nullptr;
};
