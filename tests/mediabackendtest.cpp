// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediabackend.h"

#include <QTest>

using MediaBackend::Capabilities;
using MediaBackend::Kind;

class MediaBackendTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void qtMultimediaIsAlwaysUsedWhenRequested()
    {
        for (const bool video : {false, true}) {
            const auto choice = MediaBackend::choose(Kind::QtMultimedia, video, {});
            QCOMPARE(choice.kind, Kind::QtMultimedia);
            QVERIFY(choice.notice.isEmpty());
        }
    }

    void mpvPlaysWhenBuiltAndAbleToDraw()
    {
        const Capabilities all{true, true};
        QCOMPARE(MediaBackend::choose(Kind::Mpv, true, all).kind, Kind::Mpv);
        QVERIFY(MediaBackend::choose(Kind::Mpv, true, all).notice.isEmpty());
        // Audio needs no OpenGL.
        const Capabilities noOpenGl{true, false};
        QCOMPARE(MediaBackend::choose(Kind::Mpv, false, noOpenGl).kind, Kind::Mpv);
        QVERIFY(MediaBackend::choose(Kind::Mpv, false, noOpenGl).notice.isEmpty());
    }

    void unavailableMpvFallsBackWithANotice()
    {
        const auto missing = MediaBackend::choose(Kind::Mpv, false, {false, true});
        QCOMPARE(missing.kind, Kind::QtMultimedia);
        QVERIFY(missing.notice.contains(QStringLiteral("no mpv support")));
        const auto noOpenGl = MediaBackend::choose(Kind::Mpv, true, {true, false});
        QCOMPARE(noOpenGl.kind, Kind::QtMultimedia);
        QVERIFY(noOpenGl.notice.contains(QStringLiteral("OpenGL")));
        QVERIFY(MediaBackend::startFailedNotice(QStringLiteral("boom")).contains(QStringLiteral("boom")));
    }

    void describesWhatPlays()
    {
        QCOMPARE(MediaBackend::status(Kind::QtMultimedia, {true, true}), QStringLiteral("Qt Multimedia plays audio and video."));
        QVERIFY(MediaBackend::status(Kind::QtMultimedia, {false, false}).endsWith(QStringLiteral("no mpv support, so mpv can't be chosen.")));
        QVERIFY(MediaBackend::status(Kind::Mpv, {false, true}).contains(QStringLiteral("no mpv support, so Qt Multimedia plays audio and video")));
        QVERIFY(MediaBackend::status(Kind::Mpv, {true, false}).startsWith(QStringLiteral("mpv plays audio. It needs OpenGL")));
        QVERIFY(MediaBackend::status(Kind::Mpv, {true, true}).startsWith(QStringLiteral("mpv plays audio and video")));
    }

    void reportsWhetherMpvIsBuilt()
    {
#ifdef OPUSFM_HAVE_MPV
        QVERIFY(MediaBackend::mpvBuilt());
#else
        QVERIFY(!MediaBackend::mpvBuilt());
#endif
    }
};

QTEST_GUILESS_MAIN(MediaBackendTest)
#include "mediabackendtest.moc"
