// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpvplayer.h"
#include "wavfixture.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <clocale>

class MpvPlayerTest : public QObject
{
    Q_OBJECT

private:
    // Headless runs have no audio device or display.
    const QHash<QString, QString> m_headless{{QStringLiteral("ao"), QStringLiteral("null")}, {QStringLiteral("vo"), QStringLiteral("null")}};
    QTemporaryDir m_dir;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(writeSilentWav(m_dir.filePath(QStringLiteral("silence.wav")), 3000));
    }

    void startsWithTheNumericLocaleItNeeds()
    {
        std::setlocale(LC_NUMERIC, "");
        MpvPlayer player(m_headless);
        QVERIFY2(player.isValid(), qPrintable(player.errorString()));
        QVERIFY(player.errorString().isEmpty());
    }

    void playsPausesAndSeeks()
    {
        MpvPlayer player(m_headless);
        QVERIFY(player.isValid());
        QSignalSpy loaded(&player, &MpvPlayer::fileLoaded);
        player.load(m_dir.filePath(QStringLiteral("silence.wav")), false, false);
        QVERIFY(loaded.wait(10000));
        QTRY_VERIFY(qAbs(player.duration() - 3.0) < 0.1);
        QVERIFY(player.isPaused());
        QCOMPARE(player.path(), m_dir.filePath(QStringLiteral("silence.wav")));

        player.setPaused(false);
        QTRY_VERIFY(!player.isPaused());
        QTRY_VERIFY(player.position() > 0);
        player.setPaused(true);
        QTRY_VERIFY(player.isPaused());

        player.seek(2.0);
        QTRY_VERIFY(qAbs(player.position() - 2.0) < 0.2);
    }

    void autoplaysAndRestartsAtTheEnd()
    {
        MpvPlayer player(m_headless);
        player.load(m_dir.filePath(QStringLiteral("silence.wav")), false, true);
        QTRY_VERIFY_WITH_TIMEOUT(!player.isPaused(), 10000);
        player.seek(2.9);
        // keep-open pauses on the last frame instead of unloading the file.
        QTRY_VERIFY_WITH_TIMEOUT(player.isPaused(), 10000);
        player.setPaused(false);
        QTRY_VERIFY(!player.isPaused());
        QTRY_VERIFY(player.position() < 2.0);
    }

    void reportsFilesItCannotPlay()
    {
        MpvPlayer player(m_headless);
        QSignalSpy errors(&player, &MpvPlayer::errorOccurred);
        player.load(m_dir.filePath(QStringLiteral("missing.wav")), false, true);
        QVERIFY(errors.wait(10000));
        QVERIFY(!errors.constFirst().constFirst().toString().isEmpty());

        QFile text(m_dir.filePath(QStringLiteral("notes.wav")));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("not audio at all");
        text.close();
        errors.clear();
        player.load(text.fileName(), false, true);
        QVERIFY(errors.wait(10000));
    }

    void repeatedCreationAndDestruction()
    {
        for (int i = 0; i < 5; ++i) {
            MpvPlayer player(m_headless);
            QVERIFY(player.isValid());
            player.load(m_dir.filePath(QStringLiteral("silence.wav")), false, true);
            if (i % 2) {
                QTRY_VERIFY(!player.isPaused());
            }
            player.stop();
        }
    }
};

QTEST_GUILESS_MAIN(MpvPlayerTest)
#include "mpvplayertest.moc"
