// SPDX-License-Identifier: GPL-3.0-or-later

#include "metadataeditdialog.h"
#include "settings.h"

#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

class MetadataEditDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void previewsAndAppliesChanges()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty()) {
            QSKIP("ExifTool is not installed");
        }
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("image.jpg"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        MetadataEditDialog dialog(path);
        auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("metadataPreview"));
        auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("metadataApply"));
        QTRY_VERIFY(preview->isEnabled());
        auto *artist = dialog.findChild<QLineEdit *>(QStringLiteral("metadataField_Artist"));
        QVERIFY(artist);
        artist->setText(QStringLiteral("Test artist"));
        preview->click();
        QVERIFY(apply->isEnabled());
        QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("metadataChanges"))->topLevelItemCount(), 1);
        artist->setText(QStringLiteral("Changed artist"));
        QVERIFY(!apply->isEnabled());
        preview->click();
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        apply->click();
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("metadataStatus"));
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("updated and verified")), 15000);
        QCOMPARE(MetadataEditor::read(path).values.value(QStringLiteral("Artist")), QStringLiteral("Changed artist"));
    }

    void showsUnusableExifToolError()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString tool = root.filePath(QStringLiteral("exiftool"));
        QFile script(tool);
        QVERIFY(script.open(QIODevice::WriteOnly));
        QVERIFY(script.write("#!/bin/sh\necho 'Cannot load Image/ExifTool.pm' >&2\nexit 1\n") > 0);
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QString path = root.filePath(QStringLiteral("image.jpg"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        const QByteArray oldPath = qgetenv("PATH");
        const bool hadPath = qEnvironmentVariableIsSet("PATH");
        const auto restore = qScopeGuard([&] {
            hadPath ? qputenv("PATH", oldPath) : qunsetenv("PATH");
        });
        QVERIFY(qputenv("PATH", QFile::encodeName(root.path())));
        MetadataEditDialog dialog(path);
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("metadataStatus"));
        QTRY_VERIFY(status->text().contains(QStringLiteral("Cannot load Image/ExifTool.pm")));
        QVERIFY(status->text().contains(tool));
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("metadataPreview"))->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("metadataApply"))->isEnabled());
    }

    void hidesInitialFieldsWithoutChangingTheirValues()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty()) {
            QSKIP("ExifTool is not installed");
        }
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString path = root.filePath(QStringLiteral("image.jpg"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        auto snapshot = MetadataEditor::read(path);
        QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
        auto values = snapshot.values;
        values[QStringLiteral("Artist")] = QStringLiteral("Preserved artist");
        const QString error = MetadataEditor::write(snapshot, values);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const bool previous = Settings::metadataImageArtist();
        const auto restore = qScopeGuard([previous] {
            Settings::setMetadataImageArtist(previous);
        });
        Settings::setMetadataImageArtist(false);
        MetadataEditDialog dialog(path);
        dialog.show();
        auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("metadataPreview"));
        QTRY_VERIFY(preview->isEnabled());
        auto *artist = dialog.findChild<QLineEdit *>(QStringLiteral("metadataField_Artist"));
        auto *copyright = dialog.findChild<QLineEdit *>(QStringLiteral("metadataField_Copyright"));
        auto *showAll = dialog.findChild<QCheckBox *>(QStringLiteral("metadataShowAllFields"));
        QVERIFY(artist && copyright && showAll);
        QVERIFY(artist->isHidden());
        showAll->setChecked(true);
        QVERIFY(!artist->isHidden());
        showAll->setChecked(false);
        QVERIFY(artist->isHidden());
        copyright->setText(QStringLiteral("Copyright test"));
        preview->click();
        QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("metadataChanges"))->topLevelItemCount(), 1);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("metadataApply"))->click();
        QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QLabel *>(QStringLiteral("metadataStatus"))->text().contains(QStringLiteral("updated and verified")), 15000);
        snapshot = MetadataEditor::read(path);
        QVERIFY2(snapshot.error.isEmpty(), qPrintable(snapshot.error));
        QCOMPARE(snapshot.values.value(QStringLiteral("Artist")), QStringLiteral("Preserved artist"));
        QCOMPARE(snapshot.values.value(QStringLiteral("Copyright")), QStringLiteral("Copyright test"));
    }

    void hidesSelectedAudioFields()
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
                         path});
        QVERIFY(generator.waitForFinished(30000));
        QCOMPARE(generator.exitCode(), 0);
        const bool previous = Settings::metadataAudioTitle();
        const auto restore = qScopeGuard([previous] {
            Settings::setMetadataAudioTitle(previous);
        });
        Settings::setMetadataAudioTitle(false);
        MetadataEditDialog dialog(path);
        dialog.show();
        QTRY_VERIFY(dialog.findChild<QPushButton *>(QStringLiteral("metadataPreview"))->isEnabled());
        auto *title = dialog.findChild<QLineEdit *>(QStringLiteral("metadataField_Title"));
        auto *album = dialog.findChild<QLineEdit *>(QStringLiteral("metadataField_Album"));
        QVERIFY(title && album);
        QVERIFY(title->isHidden());
        QVERIFY(!album->isHidden());
        dialog.findChild<QCheckBox *>(QStringLiteral("metadataShowAllFields"))->setChecked(true);
        QVERIFY(!title->isHidden());
    }
};

QTEST_MAIN(MetadataEditDialogTest)
#include "metadataeditdialogtest.moc"
