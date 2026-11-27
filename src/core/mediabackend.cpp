// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediabackend.h"

#include <KLocalizedString>

namespace MediaBackend
{
bool mpvBuilt()
{
#ifdef OPUSFM_HAVE_MPV
    return true;
#else
    return false;
#endif
}

Choice choose(Kind requested, bool video, const Capabilities &capabilities)
{
    if (requested == Kind::QtMultimedia) {
        return {};
    }
    if (!capabilities.mpvBuilt) {
        return {Kind::QtMultimedia, i18nc("@info", "This build of Plasma Opus has no mpv support, so Qt Multimedia plays this file.")};
    }
    if (video && !capabilities.openGl) {
        return {Kind::QtMultimedia, i18nc("@info", "mpv needs OpenGL to show video, which isn't available here, so Qt Multimedia plays this file.")};
    }
    return {Kind::Mpv, {}};
}

QString startFailedNotice(const QString &reason)
{
    return i18nc("@info %1 is an error message", "mpv could not play this file (%1), so Qt Multimedia plays it.", reason);
}
}
