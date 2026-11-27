// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

/**
 * Chooses the playback backend for audio and video in the preview panel and
 * gallery: Qt Multimedia, or the optional embedded mpv backend.
 */
namespace MediaBackend
{
enum class Kind {
    QtMultimedia,
    Mpv,
};

/// What the running build and platform provide for mpv playback.
struct Capabilities {
    /// Plasma Opus was built with libmpv.
    bool mpvBuilt = false;
    /// An OpenGL context can be created, which mpv needs to show video.
    bool openGl = false;
};

struct Choice {
    Kind kind = Kind::QtMultimedia;
    /// Explains a fallback to Qt Multimedia; empty when the request is met.
    QString notice;
};

/// Whether this build includes the embedded mpv backend.
bool mpvBuilt();

/**
 * Resolves the requested backend for one file. Audio through mpv needs only
 * libmpv; video also needs OpenGL. Unmet requests fall back to Qt Multimedia
 * with a notice.
 */
Choice choose(Kind requested, bool video, const Capabilities &capabilities);

/// The notice shown when mpv was chosen but failed to start or render.
QString startFailedNotice(const QString &reason);
}
