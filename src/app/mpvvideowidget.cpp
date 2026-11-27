// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpvvideowidget.h"

#include "mpvplayer.h"

#include <KLocalizedString>

#include <QGuiApplication>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QPointer>
#include <qguiapplication_platform.h>

#include <mpv/render_gl.h>

#include <optional>
#include <utility>

namespace
{
void *procAddress(void *, const char *name)
{
    QOpenGLContext *context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void *>(context->getProcAddress(name)) : nullptr;
}
}

MpvVideoWidget::MpvVideoWidget(MpvPlayer *player, QWidget *parent)
    : QOpenGLWidget(parent)
    , m_player(player)
{
    setObjectName(QStringLiteral("mpvVideo"));
    setMinimumSize(16, 16);
}

MpvVideoWidget::~MpvVideoWidget()
{
    makeCurrent();
    releaseRenderContext();
    doneCurrent();
}

bool MpvVideoWidget::openGlAvailable()
{
    static std::optional<bool> available;
    if (!available) {
        QOpenGLContext context;
        available = context.create();
    }
    return *available;
}

void MpvVideoWidget::initializeGL()
{
    releaseRenderContext();
    if (!m_player || !m_player->handle()) {
        return;
    }
    mpv_opengl_init_params gl{&procAddress, nullptr};
    int advanced = 1;
    QList<mpv_render_param> params{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced},
    };
    // Hardware decoding interop needs the display connection Qt already uses.
#if QT_CONFIG(xcb)
    if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
        params.append({MPV_RENDER_PARAM_X11_DISPLAY, x11->display()});
    }
#endif
#if QT_CONFIG(wayland)
    if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
        params.append({MPV_RENDER_PARAM_WL_DISPLAY, wayland->display()});
    }
#endif
    params.append({MPV_RENDER_PARAM_INVALID, nullptr});
    const int created = mpv_render_context_create(&m_render, m_player->handle(), params.data());
    if (created < 0) {
        m_render = nullptr;
        const QString message = QString::fromUtf8(mpv_error_string(created));
        QMetaObject::invokeMethod(
            this,
            [this, message] {
                Q_EMIT renderingFailed(message);
            },
            Qt::QueuedConnection);
        return;
    }
    mpv_render_context_set_update_callback(
        m_render,
        [](void *context) {
            auto *widget = static_cast<MpvVideoWidget *>(context);
            QMetaObject::invokeMethod(
                widget,
                [widget] {
                    widget->update();
                },
                Qt::QueuedConnection);
        },
        this);
    // The context can be replaced when the widget moves to another window.
    if (!m_contextWatched) {
        m_contextWatched = connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
            makeCurrent();
            releaseRenderContext();
            doneCurrent();
            disconnect(std::exchange(m_contextWatched, {}));
        });
    }
}

void MpvVideoWidget::paintGL()
{
    if (!m_render) {
        return;
    }
    const qreal ratio = devicePixelRatioF();
    mpv_opengl_fbo fbo{int(defaultFramebufferObject()), int(width() * ratio), int(height() * ratio), 0};
    int flip = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(m_render, params);
}

void MpvVideoWidget::releaseRenderContext()
{
    if (m_render) {
        // Returns only once mpv stops calling the update callback.
        mpv_render_context_free(std::exchange(m_render, nullptr));
    }
}
