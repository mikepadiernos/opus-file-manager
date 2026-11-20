// SPDX-License-Identifier: GPL-3.0-or-later

#include "batchrenamedialog.h"

#include <KPluginMetaData>
#include <KSharedConfig>

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

#include <algorithm>

class BatchRenameDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void previewsAndRenamesOnlyAfterConfirmation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile first(root.filePath(QStringLiteral("a.txt")));
        QFile second(root.filePath(QStringLiteral("b.txt")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        first.close();
        QVERIFY(second.open(QIODevice::WriteOnly));
        second.close();
        BatchRenameDialog dialog({first.fileName(), second.fileName()});
        auto *pattern = dialog.findChild<QLineEdit *>(QStringLiteral("batchRenamePattern"));
        auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("batchRenamePreview"));
        auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("batchRenameApply"));
        auto *results = dialog.findChild<QTreeWidget *>(QStringLiteral("batchRenameResults"));
        QVERIFY(pattern && preview && apply && results);
        pattern->setText(QStringLiteral("same"));
        preview->click();
        QTRY_VERIFY(preview->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 2);
        QVERIFY(!apply->isEnabled());
        pattern->setText(QStringLiteral("{nn}"));
        preview->click();
        QTRY_VERIFY(preview->isEnabled());
        QVERIFY(apply->isEnabled());
        QCOMPARE(results->topLevelItem(0)->text(1), root.filePath(QStringLiteral("01.txt")));
        pattern->setText(QStringLiteral("changed-{nn}"));
        QVERIFY(!apply->isEnabled());
        pattern->setText(QStringLiteral("{nn}"));
        preview->click();
        QTRY_VERIFY(preview->isEnabled());
        QVERIFY(apply->isEnabled());
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::No)->click();
        });
        apply->click();
        QVERIFY(QFile::exists(first.fileName()));
        QVERIFY(apply->isEnabled());
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        apply->click();
        QTRY_VERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("batchRenameCancel"))->isEnabled());
        QVERIFY(!QFile::exists(first.fileName()));
        QVERIFY(!QFile::exists(second.fileName()));
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("01.txt"))));
        QVERIFY(QFile::exists(root.filePath(QStringLiteral("02.txt"))));
        QVERIFY(!apply->isEnabled());
        QVERIFY(!preview->isEnabled());
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("batchRenameStatus"))->text().contains(QStringLiteral("Renamed 2")));
    }

    void previewsRealImageAndAudioMetadata()
    {
        const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/kfilemetadata"));
        const auto hasPlugin = [&plugins](const QString &name) {
            return std::any_of(plugins.cbegin(), plugins.cend(), [&name](const KPluginMetaData &plugin) {
                return plugin.pluginId().contains(name);
            });
        };
        const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (!hasPlugin(QStringLiteral("exiv2")) || !hasPlugin(QStringLiteral("taglib")) || ffmpeg.isEmpty()) {
            QSKIP("Image/audio metadata extractors and ffmpeg are required for this fixture.");
        }

        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString imagePath = root.filePath(QStringLiteral("photo.jpg"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));
        QFile imageFile(imagePath);
        QVERIFY(imageFile.open(QIODevice::ReadOnly));
        QByteArray jpeg = imageFile.readAll();
        imageFile.close();
        jpeg.insert(2, QByteArray::fromHex("ffe1002745786966000049492a000800000001000f010200050000001a0000000000000041636d6500"));
        QVERIFY(imageFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(imageFile.write(jpeg), jpeg.size());
        imageFile.close();
        const QString audioPath = root.filePath(QStringLiteral("song.mp3"));
        QProcess generator;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), root.path());
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), root.path());
        generator.setProcessEnvironment(environment);
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
                         QStringLiteral("artist=Test Artist"),
                         QStringLiteral("-c:a"),
                         QStringLiteral("libmp3lame"),
                         QStringLiteral("-y"),
                         audioPath});
        QVERIFY(generator.waitForStarted());
        QVERIFY(generator.waitForFinished(15000));
        if (generator.exitCode() != 0) {
            QSKIP("The available ffmpeg cannot create the audio fixture.");
        }
        for (const auto &fixture : {qMakePair(imagePath, QStringLiteral("{camera}")), qMakePair(audioPath, QStringLiteral("{artist}"))}) {
            BatchRenameDialog dialog({fixture.first});
            dialog.findChild<QLineEdit *>(QStringLiteral("batchRenamePattern"))->setText(fixture.second);
            auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("batchRenamePreview"));
            preview->click();
            QTRY_VERIFY(preview->isEnabled());
            QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("batchRenameApply"))->isEnabled());
            const QString expected = fixture.first == imagePath ? root.filePath(QStringLiteral("Acme.jpg")) : root.filePath(QStringLiteral("Test Artist.mp3"));
            QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("batchRenameResults"))->topLevelItem(0)->text(1), expected);
        }
    }

    void managesDefaultPresetAndRequiresCollisionPreview()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile first(root.filePath(QStringLiteral("a.txt")));
        QFile second(root.filePath(QStringLiteral("b.txt")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        first.close();
        QVERIFY(second.open(QIODevice::WriteOnly));
        second.close();
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("presetsrc")), KConfig::SimpleConfig);
        {
            BatchRenameDialog manager({}, nullptr, config, true);
            manager.findChild<QLineEdit *>(QStringLiteral("batchRenamePattern"))->setText(QStringLiteral("initial"));
            QTimer::singleShot(0, [] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(QStringLiteral("Same name"));
                input->accept();
            });
            manager.findChild<QPushButton *>(QStringLiteral("batchRenameSavePreset"))->click();
            manager.findChild<QLineEdit *>(QStringLiteral("batchRenamePattern"))->setText(QStringLiteral("collision"));
            QTimer::singleShot(0, [] {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                QVERIFY(box);
                box->button(QMessageBox::Yes)->click();
            });
            manager.findChild<QPushButton *>(QStringLiteral("batchRenameUpdatePreset"))->click();
            manager.findChild<QPushButton *>(QStringLiteral("batchRenameSetDefault"))->click();
            QCOMPARE(manager.findChild<QLabel *>(QStringLiteral("batchRenameDefault"))->text(), QStringLiteral("Default preset: Same name"));
        }
        config->reparseConfiguration();
        BatchRenameDialog dialog({first.fileName(), second.fileName()}, nullptr, config);
        QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("batchRenamePattern"))->text(), QStringLiteral("collision"));
        auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("batchRenameApply"));
        auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("batchRenamePreview"));
        QVERIFY(!apply->isEnabled());
        preview->click();
        QTRY_VERIFY(preview->isEnabled());
        QVERIFY(!apply->isEnabled());
        QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("batchRenameResults"))->topLevelItemCount(), 2);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("batchRenameDeletePreset"))->click();
        QVERIFY(config->group(QStringLiteral("BatchRenamePresets")).readEntry("DefaultPreset", QString()).isEmpty());
        QCOMPARE(dialog.findChild<QLabel *>(QStringLiteral("batchRenameDefault"))->text(), QStringLiteral("Default preset: none"));
        QVERIFY(QFile::exists(first.fileName()));
        QVERIFY(QFile::exists(second.fileName()));
    }

    void savesLoadsAndDeletesRegexPresets()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile source(root.filePath(QStringLiteral("IMG_123.jpg")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.close();
        const auto config = KSharedConfig::openConfig(root.filePath(QStringLiteral("presetsrc")), KConfig::SimpleConfig);
        {
            BatchRenameDialog dialog({source.fileName()}, nullptr, config);
            dialog.findChild<QComboBox *>(QStringLiteral("batchRenameMode"))->setCurrentIndex(1);
            dialog.findChild<QLineEdit *>(QStringLiteral("batchRenamePattern"))->setText(QStringLiteral("IMG_(\\d+)"));
            dialog.findChild<QLineEdit *>(QStringLiteral("batchRenameReplacement"))->setText(QStringLiteral("photo-$1"));
            QTimer::singleShot(0, [] {
                auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(input);
                input->setTextValue(QStringLiteral("Camera"));
                input->accept();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("batchRenameSavePreset"))->click();
            QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("batchRenamePresets"))->count(), 1);
        }
        config->reparseConfiguration();
        BatchRenameDialog dialog({source.fileName()}, nullptr, config);
        dialog.findChild<QPushButton *>(QStringLiteral("batchRenameLoadPreset"))->click();
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("batchRenameMode"))->currentIndex(), 1);
        QCOMPARE(dialog.findChild<QLineEdit *>(QStringLiteral("batchRenameReplacement"))->text(), QStringLiteral("photo-$1"));
        auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("batchRenamePreview"));
        preview->click();
        QTRY_VERIFY(preview->isEnabled());
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("batchRenameApply"))->isEnabled());
        QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("batchRenameResults"))->topLevelItem(0)->text(1),
                 root.filePath(QStringLiteral("photo-123.jpg")));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("batchRenameDeletePreset"))->click();
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("batchRenamePresets"))->count(), 0);
        QVERIFY(QFile::exists(source.fileName()));
    }
};

QTEST_MAIN(BatchRenameDialogTest)
#include "batchrenamedialogtest.moc"
