// SPDX-License-Identifier: GPL-3.0-or-later

#include "metadataeditor.h"

#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

class MetadataEditorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void editsJpegAndRejectsStalePreview()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty()) {
            QSKIP("ExifTool is not installed");
        }
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("photo.jpg"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        auto snapshot = MetadataEditor::read(path);
        QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
        auto values = snapshot.values;
        values[QStringLiteral("Artist")] = QStringLiteral("Artist ; $(not a shell)");
        values[QStringLiteral("ImageDescription")] = QStringLiteral("A photo");
        const QString error = MetadataEditor::write(snapshot, values);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(MetadataEditor::read(path).values, values);
        values[QStringLiteral("Artist")] = QStringLiteral("Another artist");
        QVERIFY(!MetadataEditor::write(snapshot, values).isEmpty());
        snapshot = MetadataEditor::read(path);
        values = snapshot.values;
        values[QStringLiteral("Artist")].clear();
        const QString clearError = MetadataEditor::write(snapshot, values);
        QVERIFY2(clearError.isEmpty(), qPrintable(clearError));
        QCOMPARE(MetadataEditor::read(path).values, values);
    }

    void rejectsUnsupportedFilesAndLinks()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("plain.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not metadata");
        file.close();
        QVERIFY(!MetadataEditor::read(path).error.isEmpty());
        const QString symlink = root.filePath(QStringLiteral("link"));
        QCOMPARE(::symlink(QFile::encodeName(path).constData(), QFile::encodeName(symlink).constData()), 0);
        QVERIFY(!MetadataEditor::read(symlink).error.isEmpty());
        const QString hardlink = root.filePath(QStringLiteral("hardlink"));
        QCOMPARE(::link(QFile::encodeName(path).constData(), QFile::encodeName(hardlink).constData()), 0);
        QVERIFY(!MetadataEditor::read(path).error.isEmpty());
    }

    void editsMp3Tags()
    {
        const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (ffmpeg.isEmpty() || QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty()) {
            QSKIP("FFmpeg and ExifTool are needed for this test");
        }
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("song.mp3"));
        QProcess generator;
        generator.start(ffmpeg,
                        {QStringLiteral("-v"),
                         QStringLiteral("error"),
                         QStringLiteral("-f"),
                         QStringLiteral("lavfi"),
                         QStringLiteral("-i"),
                         QStringLiteral("anullsrc=r=44100:cl=mono"),
                         QStringLiteral("-t"),
                         QStringLiteral("0.1"),
                         QStringLiteral("-metadata"),
                         QStringLiteral("title=Original"),
                         path});
        QVERIFY(generator.waitForFinished(30000));
        QCOMPARE(generator.exitCode(), 0);
        auto snapshot = MetadataEditor::read(path);
        QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
        QCOMPARE(snapshot.values.value(QStringLiteral("Title")), QStringLiteral("Original"));
        auto values = snapshot.values;
        values[QStringLiteral("Title")] = QStringLiteral("Changed");
        values[QStringLiteral("Artist")] = QStringLiteral("Artist");
        const QString error = MetadataEditor::write(snapshot, values);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(MetadataEditor::read(path).values, values);
        snapshot = MetadataEditor::read(path);
        values[QStringLiteral("Title")].clear();
        const QString clearError = MetadataEditor::write(snapshot, values);
        QVERIFY2(clearError.isEmpty(), qPrintable(clearError));
        QCOMPARE(MetadataEditor::read(path).values, values);
    }

    void editsTiffMetadata()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty()
            || !QImageWriter::supportedImageFormats().contains(QByteArrayLiteral("tiff"))) {
            QSKIP("ExifTool and TIFF image support are needed for this test");
        }
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("image.tiff"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        auto snapshot = MetadataEditor::read(path);
        QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
        auto values = snapshot.values;
        values[QStringLiteral("Copyright")] = QStringLiteral("Copyright example");
        const QString error = MetadataEditor::write(snapshot, values);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(MetadataEditor::read(path).values, values);
    }
};

QTEST_GUILESS_MAIN(MetadataEditorTest)
#include "metadataeditortest.moc"
