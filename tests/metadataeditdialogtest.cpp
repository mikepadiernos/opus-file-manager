// SPDX-License-Identifier: GPL-3.0-or-later

#include "metadataeditdialog.h"

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
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
};

QTEST_MAIN(MetadataEditDialogTest)
#include "metadataeditdialogtest.moc"
