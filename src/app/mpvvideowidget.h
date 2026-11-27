// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QOpenGLWidget>

class MpvPlayer;
struct mpv_render_context;

/**
 * Draws an MpvPlayer's video with libmpv's OpenGL render API, so mpv plays
 * inside a Qt Widgets layout without Qt Quick. Delete it before its player.
 */
class MpvVideoWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit MpvVideoWidget(MpvPlayer *player, QWidget *parent = nullptr);
    ~MpvVideoWidget() override;

    /// Whether the system can create an OpenGL context at all; checked once.
    static bool openGlAvailable();

Q_SIGNALS:
    /// libmpv could not render into this widget's OpenGL context.
    void renderingFailed(const QString &message);

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    void releaseRenderContext();

    MpvPlayer *m_player = nullptr;
    mpv_render_context *m_render = nullptr;
    QMetaObject::Connection m_contextWatched;
};
