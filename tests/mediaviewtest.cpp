// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediaview.h"
#include "mediabackend.h"
#include "settings.h"
#include "wavfixture.h"

#ifdef OPUSFM_HAVE_MPV
#include "mpvplayer.h"
#include "mpvvideowidget.h"
#endif

#include <QAction>
#include <QLabel>
#include <QMediaPlayer>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

class MediaViewTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QUrl audio() const
    {
        return QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("silence.wav")));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        QVERIFY(writeSilentWav(audio().toLocalFile(), 3000));
        MediaView::setMpvOptions({{QStringLiteral("ao"), QStringLiteral("null")}});
    }

    void init()
    {
        Settings::setMediaBackend(Settings::EnumMediaBackend::QtMultimedia);
        Settings::self()->save();
    }

    void qtMultimediaIsTheDefault()
    {
        MediaView view;
        view.show();
        view.open(audio(), false, false);
        QVERIFY(view.player());
        QVERIFY(!view.mpvPlayer());
        auto *backend = view.findChild<QToolButton *>(QStringLiteral("mediaBackend"));
        QVERIFY(backend);
        QCOMPARE(backend->text(), QStringLiteral("Qt Multimedia"));
        QVERIFY(view.findChild<QAction *>(QStringLiteral("mediaBackendQtMultimedia"))->isChecked());
        QVERIFY(!view.findChild<QLabel *>(QStringLiteral("mediaNotice"))->isVisible());
    }

    void choosingMpvReopensTheFileAndIsRemembered()
    {
        MediaView view;
        view.show();
        view.open(audio(), false, false);
        auto *mpv = view.findChild<QAction *>(QStringLiteral("mediaBackendMpv"));
        QVERIFY(mpv);
        if (!MediaBackend::mpvBuilt()) {
            QVERIFY(!mpv->isEnabled());
            QSKIP("This build has no mpv support");
        }
        mpv->trigger();
        QCOMPARE(Settings::mediaBackend(), Settings::EnumMediaBackend::Mpv);
        QCOMPARE(view.findChild<QToolButton *>(QStringLiteral("mediaBackend"))->text(), QStringLiteral("mpv"));
        QVERIFY(!view.player());
#ifdef OPUSFM_HAVE_MPV
        MpvPlayer *player = view.mpvPlayer();
        QVERIFY(player);
        QCOMPARE(player->path(), audio().toLocalFile());
        // Audio never needs a video surface.
        QVERIFY(!view.findChild<MpvVideoWidget *>());
        auto *play = view.findChild<QPushButton *>();
        QTRY_VERIFY(player->isPaused());
        play->click();
        QTRY_VERIFY(!player->isPaused());
        QCOMPARE(play->text(), QStringLiteral("Pause"));

        // Another view, such as the gallery, opens with the remembered choice.
        MediaView other;
        other.open(audio(), false, false);
        QVERIFY(other.mpvPlayer());

        view.findChild<QAction *>(QStringLiteral("mediaBackendQtMultimedia"))->trigger();
        QVERIFY(view.player());
        QVERIFY(!view.mpvPlayer());
#endif
    }

    void videoWithoutOpenGlFallsBackWithANotice()
    {
        if (!MediaBackend::mpvBuilt()) {
            QSKIP("This build has no mpv support");
        }
#ifdef OPUSFM_HAVE_MPV
        Settings::setMediaBackend(Settings::EnumMediaBackend::Mpv);
        MediaView view;
        view.show();
        view.open(audio(), true, false);
        auto *notice = view.findChild<QLabel *>(QStringLiteral("mediaNotice"));
        if (MpvVideoWidget::openGlAvailable()) {
            QVERIFY(view.mpvPlayer());
            QVERIFY(view.findChild<MpvVideoWidget *>());
        } else {
            QVERIFY(view.player());
            QVERIFY(!view.mpvPlayer());
            QVERIFY(notice->isVisible());
            QVERIFY(notice->text().contains(QStringLiteral("OpenGL")));
        }
        view.clear();
        QVERIFY(!notice->isVisible());
        QVERIFY(!view.findChild<MpvVideoWidget *>());
#endif
    }

    void missingMpvSupportIsReported()
    {
        if (MediaBackend::mpvBuilt()) {
            QSKIP("This build has mpv support");
        }
        Settings::setMediaBackend(Settings::EnumMediaBackend::Mpv);
        MediaView view;
        view.show();
        view.open(audio(), false, false);
        QVERIFY(view.player());
        QVERIFY(view.findChild<QLabel *>(QStringLiteral("mediaNotice"))->text().contains(QStringLiteral("no mpv support")));
    }

    void mpvReportsPlaybackErrors()
    {
        if (!MediaBackend::mpvBuilt()) {
            QSKIP("This build has no mpv support");
        }
        Settings::setMediaBackend(Settings::EnumMediaBackend::Mpv);
        MediaView view;
        QSignalSpy errors(&view, &MediaView::playbackError);
        view.open(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("missing.wav"))), false, true);
        QVERIFY(errors.wait(10000));
    }

    void repeatedOpenAndClear()
    {
        if (!MediaBackend::mpvBuilt()) {
            QSKIP("This build has no mpv support");
        }
        Settings::setMediaBackend(Settings::EnumMediaBackend::Mpv);
        MediaView view;
        view.show();
        for (int i = 0; i < 5; ++i) {
            view.open(audio(), false, i % 2);
            QVERIFY(view.mpvPlayer());
            view.clear();
            QVERIFY(!view.mpvPlayer());
        }
    }
};

QTEST_MAIN(MediaViewTest)
#include "mediaviewtest.moc"
