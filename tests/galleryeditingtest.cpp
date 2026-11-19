// SPDX-License-Identifier: GPL-3.0-or-later

#include "gallerywindow.h"
#include "settings.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QImageWriter>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class GalleryEditingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void init()
    {
        Settings::self()->setDefaults();
    }

    void galleryPreferences()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage image(20, 20, QImage::Format_RGB32);
        image.fill(Qt::red);
        const QString first = dir.filePath(QStringLiteral("first.png"));
        const QString second = dir.filePath(QStringLiteral("second.png"));
        QVERIFY(image.save(first));
        QVERIFY(image.save(second));
        Settings::setGalleryShowInformation(true);
        Settings::setGallerySlideshowInterval(7);
        Settings::setGallerySlideshowLoop(false);

        QPointer<GalleryWindow> gallery = new GalleryWindow({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second)}, {}, 0);
        gallery->show();
        auto *info = gallery->findChild<QPushButton *>(QStringLiteral("galleryInformation"));
        auto *histogram = gallery->findChild<QLabel *>(QStringLiteral("galleryHistogram"));
        auto *timer = gallery->findChild<QTimer *>(QStringLiteral("gallerySlideshowTimer"));
        auto *button = gallery->findChild<QPushButton *>(QStringLiteral("gallerySlideshow"));
        auto *filmstrip = gallery->findChild<QListWidget *>(QStringLiteral("galleryFilmstrip"));
        QVERIFY(info && histogram && timer && button && filmstrip);
        QVERIFY(info->isChecked());
        QTRY_VERIFY(!histogram->pixmap().isNull());
        QCOMPARE(timer->interval(), 7000);
        button->click();
        QVERIFY(timer->isActive());
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout"));
        QCOMPARE(filmstrip->currentRow(), 1);
        QVERIFY(timer->isActive());
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout"));
        QVERIFY(!timer->isActive());
        QCOMPARE(filmstrip->currentRow(), 1);

        Settings::setGallerySlideshowLoop(true);
        button->click();
        QVERIFY(QMetaObject::invokeMethod(timer, "timeout"));
        QCOMPARE(filmstrip->currentRow(), 0);
        QVERIFY(timer->isActive());
        button->click();
        info->click();
        QVERIFY(!info->isChecked());
        gallery->close();
        QTRY_VERIFY(gallery.isNull());

        Settings::setGalleryShowInformation(false);
        gallery = new GalleryWindow({QUrl::fromLocalFile(first)}, {}, 0);
        QVERIFY(!gallery->findChild<QPushButton *>(QStringLiteral("galleryInformation"))->isChecked());
        gallery->close();
        QTRY_VERIFY(gallery.isNull());
    }

    void cropResizeAdjustAndReplace()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("source.png"));
        QImage original(100, 80, QImage::Format_ARGB32);
        original.fill(QColor(100, 70, 50, 255));
        QVERIFY(original.save(path));
        QPointer<GalleryWindow> gallery = new GalleryWindow({QUrl::fromLocalFile(path)}, {}, 0);
        gallery->resize(900, 600);
        gallery->show();
        QVERIFY(QTest::qWaitForWindowExposed(gallery));
        auto *view = gallery->findChild<QGraphicsView *>(QStringLiteral("galleryView"));
        QVERIFY(view);
        auto *pixmap = qgraphicsitem_cast<QGraphicsPixmapItem *>(view->scene()->items().first());
        QVERIFY(pixmap);
        QTRY_COMPARE(pixmap->pixmap().size(), original.size());
        auto *info = gallery->findChild<QPushButton *>(QStringLiteral("galleryInformation"));
        auto *colours = gallery->findChild<QLabel *>(QStringLiteral("galleryColours"));
        auto *histogram = gallery->findChild<QLabel *>(QStringLiteral("galleryHistogram"));
        QVERIFY(info && colours && histogram);
        info->click();
        QTRY_VERIFY(!histogram->pixmap().isNull());
        QVERIFY(!colours->pixmap().isNull());

        auto *crop = gallery->findChild<QAction *>(QStringLiteral("galleryCrop"));
        QVERIFY(crop);
        crop->trigger();
        QVERIFY(crop->isChecked());
        const QPoint start = view->mapFromScene(QPointF(10, 10));
        const QPoint end = view->mapFromScene(QPointF(70, 60));
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(view->viewport(), end, 10);
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, end);
        QTRY_VERIFY(pixmap->pixmap().width() < original.width());
        const QSize cropped = pixmap->pixmap().size();
        QVERIFY(cropped.width() >= 58 && cropped.width() <= 62);
        QVERIFY(cropped.height() >= 48 && cropped.height() <= 52);

        QTimer resizeDialog;
        connect(&resizeDialog, &QTimer::timeout, &resizeDialog, [&resizeDialog] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                if (auto *width = dialog->findChild<QSpinBox *>(QStringLiteral("resizeWidth"))) {
                    resizeDialog.stop();
                    width->setValue(20);
                    dialog->accept();
                }
            }
        });
        resizeDialog.start(10);
        gallery->findChild<QAction *>(QStringLiteral("galleryResize"))->trigger();
        QTRY_COMPARE(pixmap->pixmap().width(), 20);
        QCOMPARE(pixmap->pixmap().height(), 20 * cropped.height() / cropped.width());

        QTimer adjustDialog;
        connect(&adjustDialog, &QTimer::timeout, &adjustDialog, [&adjustDialog] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                if (auto *brightness = dialog->findChild<QSlider *>(QStringLiteral("brightness"))) {
                    adjustDialog.stop();
                    brightness->setValue(150);
                    dialog->accept();
                }
            }
        });
        adjustDialog.start(10);
        gallery->findChild<QAction *>(QStringLiteral("galleryAdjust"))->trigger();
        QTRY_VERIFY(pixmap->pixmap().toImage().pixelColor(0, 0).red() > 100);
        QTRY_VERIFY(!colours->pixmap().isNull() && colours->pixmap().toImage().pixelColor(10, 10).red() > 100);
        const QSize edited = pixmap->pixmap().size();

        QImage external(5, 5, QImage::Format_RGB32);
        external.fill(Qt::blue);
        QVERIFY(external.save(path));
        bool warned = false;
        QTimer confirm;
        connect(&confirm, &QTimer::timeout, &confirm, [&confirm, &warned] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                for (const QLabel *label : dialog->findChildren<QLabel *>()) {
                    warned |= label->text().contains(QStringLiteral("changed on disk"));
                }
                confirm.stop();
                dialog->reject();
            }
        });
        confirm.start(10);
        gallery->findChild<QAction *>(QStringLiteral("galleryReplace"))->trigger();
        QVERIFY(warned);
        QCOMPARE(QImage(path).size(), QSize(5, 5));

        QTimer accept;
        connect(&accept, &QTimer::timeout, &accept, [&accept] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                if (auto *buttons = dialog->findChild<QDialogButtonBox *>()) {
                    for (auto *button : buttons->buttons()) {
                        if (buttons->standardButton(button) == QDialogButtonBox::Yes) {
                            accept.stop();
                            button->click();
                            return;
                        }
                    }
                }
            }
        });
        accept.start(10);
        gallery->findChild<QAction *>(QStringLiteral("galleryReplace"))->trigger();
        QCOMPARE(QImage(path).size(), edited);
        gallery->close();
        QTRY_VERIFY(gallery.isNull());
    }

    void saveCopySuggestsConfiguredFormat()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("source.png"));
        QImage image(20, 20, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path));
        QPointer<GalleryWindow> gallery = new GalleryWindow({QUrl::fromLocalFile(path)}, {}, 0);
        gallery->show();
        auto *save = gallery->findChild<QPushButton *>(QStringLiteral("gallerySave"));
        QVERIFY(save);
        QTRY_VERIFY(save->isEnabled());
        for (const auto &[format, suffix] : {std::pair{Settings::EnumGalleryCopyFormat::Png, QStringLiteral(".png")},
                                             std::pair{Settings::EnumGalleryCopyFormat::Jpeg, QStringLiteral(".jpg")},
                                             std::pair{Settings::EnumGalleryCopyFormat::Webp, QStringLiteral(".webp")}}) {
            Settings::setGalleryCopyFormat(format);
            QString suggested;
            QTimer dialogTimer;
            connect(&dialogTimer, &QTimer::timeout, &dialogTimer, [&] {
                if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
                    if (auto *name = dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"))) {
                        suggested = name->text();
                    }
                    dialogTimer.stop();
                    dialog->reject();
                }
            });
            dialogTimer.start(10);
            save->click();
            QVERIFY(suggested.endsWith(QStringLiteral("source-edited") + suffix));
        }
        gallery->close();
        QTRY_VERIFY(gallery.isNull());
    }

    void saveCopyUsesChosenQuality()
    {
        if (!QImageWriter::supportedImageFormats().contains("jpeg")) {
            QSKIP("JPEG writer is not installed");
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("source.png"));
        QImage image(128, 128, QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                image.setPixelColor(x, y, QColor((x * 37 + y * 17) % 256, (x * 13 + y * 43) % 256, (x * 71 + y * 29) % 256));
            }
        }
        QVERIFY(image.save(path));
        Settings::setGalleryCopyFormat(Settings::EnumGalleryCopyFormat::Jpeg);
        Settings::setGalleryCopyQuality(73);
        QPointer<GalleryWindow> gallery = new GalleryWindow({QUrl::fromLocalFile(path)}, {}, 0);
        gallery->show();
        auto *save = gallery->findChild<QPushButton *>(QStringLiteral("gallerySave"));
        QVERIFY(save);
        QTRY_VERIFY(save->isEnabled());
        QString suggested;
        int initialQuality = -1;
        const auto saveAtQuality = [&](const QString &destination, int quality) {
            QTimer dialogs;
            connect(&dialogs, &QTimer::timeout, &dialogs, [&] {
                if (auto *fileDialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
                    auto *name = fileDialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
                    if (!name) {
                        dialogs.stop();
                        fileDialog->reject();
                        return;
                    }
                    suggested = name->text();
                    name->setText(destination);
                    static_cast<QDialog *>(fileDialog)->accept();
                } else if (auto *qualityDialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                    initialQuality = qualityDialog->intValue();
                    qualityDialog->setIntValue(quality);
                    dialogs.stop();
                    qualityDialog->accept();
                }
            });
            dialogs.start(10);
            save->click();
        };
        const QString low = dir.filePath(QStringLiteral("low.jpg"));
        const QString high = dir.filePath(QStringLiteral("high.jpg"));
        saveAtQuality(low, 5);
        QVERIFY(suggested.endsWith(QStringLiteral("source-edited.jpg")));
        QCOMPARE(initialQuality, 73);
        saveAtQuality(high, 95);
        QVERIFY(!QImage(low).isNull());
        QVERIFY(!QImage(high).isNull());
        QVERIFY(QFileInfo(high).size() > QFileInfo(low).size());
        gallery->close();
        QTRY_VERIFY(gallery.isNull());
    }
};

QTEST_MAIN(GalleryEditingTest)
#include "galleryeditingtest.moc"
