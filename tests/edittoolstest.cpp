// SPDX-License-Identifier: GPL-3.0-or-later

#include "edittools.h"

#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using EditTools::Category;
using EditTools::Tool;

namespace
{
EditTools::Finder installed(const QSet<Tool> &tools)
{
    return [tools](Tool tool, const QString &) {
        return tools.contains(tool) ? QStringLiteral("/usr/bin/") + EditTools::executable(tool, QStringLiteral("application/vnd.oasis.opendocument.text"))
                                    : QString();
    };
}
}

class EditToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_folder.isValid());
    }

    void classifiesViewableFiles_data()
    {
        QTest::addColumn<QString>("mimeType");
        QTest::addColumn<Category>("category");
        QTest::newRow("plain") << QStringLiteral("text/plain") << Category::Text;
        QTest::newRow("c++") << QStringLiteral("text/x-c++src") << Category::Text;
        QTest::newRow("shell") << QStringLiteral("application/x-shellscript") << Category::Text;
        QTest::newRow("xml") << QStringLiteral("application/xml") << Category::Text;
        QTest::newRow("empty file") << QStringLiteral("application/x-zerosize") << Category::Text;
        QTest::newRow("odt") << QStringLiteral("application/vnd.oasis.opendocument.text") << Category::Office;
        QTest::newRow("xlsx") << QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") << Category::Office;
        QTest::newRow("svg") << QStringLiteral("image/svg+xml") << Category::Vector;
        QTest::newRow("png") << QStringLiteral("image/png") << Category::Image;
        QTest::newRow("jpeg") << QStringLiteral("image/jpeg") << Category::Image;
        QTest::newRow("flac") << QStringLiteral("audio/flac") << Category::Audio;
        QTest::newRow("video") << QStringLiteral("video/mp4") << Category::None;
        QTest::newRow("pdf") << QStringLiteral("application/pdf") << Category::None;
        QTest::newRow("zip") << QStringLiteral("application/zip") << Category::None;
        QTest::newRow("unknown") << QString() << Category::None;
    }

    void classifiesViewableFiles()
    {
        QFETCH(QString, mimeType);
        QFETCH(Category, category);
        QCOMPARE(EditTools::category(mimeType), category);
    }

    void describesTools()
    {
        QCOMPARE(EditTools::candidates(Category::Vector), (QList<Tool>{Tool::Inkscape, Tool::BuiltInEditor}));
        QVERIFY(EditTools::candidates(Category::None).isEmpty());
        QVERIFY(EditTools::executable(Tool::BuiltInEditor).isEmpty());
        QCOMPARE(EditTools::executable(Tool::Calligra, QStringLiteral("application/vnd.ms-excel")), QStringLiteral("calligrasheets"));
        QCOMPARE(EditTools::desktopName(Tool::LibreOffice, QStringLiteral("application/vnd.oasis.opendocument.presentation")),
                 QStringLiteral("libreoffice-impress"));
        QCOMPARE(EditTools::officeApp(Tool::OnlyOffice), std::optional(OfficeApps::App::OnlyOffice));
        QVERIFY(!EditTools::officeApp(Tool::Krita));
        QVERIFY(EditTools::opensRemoteFiles(Tool::BuiltInEditor));
        QVERIFY(EditTools::opensRemoteFiles(Tool::KolourPaint));
        QVERIFY(!EditTools::opensRemoteFiles(Tool::Gimp));
        for (const Category category : {Category::Text, Category::Office, Category::Vector, Category::Image, Category::Audio}) {
            for (const Tool tool : EditTools::candidates(category)) {
                QVERIFY(!EditTools::name(tool).isEmpty());
                QCOMPARE(EditTools::executable(tool, QStringLiteral("text/plain")).isEmpty() && !EditTools::officeApp(tool), tool == Tool::BuiltInEditor);
            }
        }
    }

    void choosesTheFirstAvailableTool()
    {
        const QUrl text = QUrl::fromLocalFile(m_folder.filePath(QStringLiteral("notes.txt")));
        const QUrl image = QUrl::fromLocalFile(m_folder.filePath(QStringLiteral("--photo.png")));
        const QUrl drawing = QUrl::fromLocalFile(m_folder.filePath(QStringLiteral("logo.svg")));
        QString error;

        auto choice = EditTools::choose(text, QStringLiteral("text/plain"), &error, installed({}));
        QVERIFY(choice);
        QCOMPARE(choice->tool, Tool::BuiltInEditor);
        QVERIFY(choice->executable.isEmpty());

        choice = EditTools::choose(image, QStringLiteral("image/png"), &error, installed({Tool::Krita, Tool::Gimp}));
        QVERIFY(choice);
        QCOMPARE(choice->tool, Tool::Krita);
        QCOMPARE(choice->executable, QStringLiteral("/usr/bin/krita"));
        QCOMPARE(EditTools::arguments(image), QStringList{image.toLocalFile()});

        choice = EditTools::choose(drawing, QStringLiteral("image/svg+xml"), &error, installed({}));
        QVERIFY(choice);
        QCOMPARE(choice->tool, Tool::BuiltInEditor);
        choice = EditTools::choose(drawing, QStringLiteral("image/svg+xml"), &error, installed({Tool::Inkscape}));
        QCOMPARE(choice->tool, Tool::Inkscape);
        QVERIFY(error.isEmpty());
    }

    void reportsMissingToolsAndLocations()
    {
        QString error;
        const QUrl image = QUrl::fromLocalFile(m_folder.filePath(QStringLiteral("photo.png")));
        QVERIFY(!EditTools::choose(image, QStringLiteral("image/png"), &error, installed({})));
        QCOMPARE(error, QStringLiteral("No image editor is installed. Install KolourPaint, Krita or GIMP."));
        QVERIFY(!EditTools::choose(QUrl::fromLocalFile(m_folder.filePath(QStringLiteral("song.flac"))), QStringLiteral("audio/flac"), &error, installed({})));
        QVERIFY(error.startsWith(QStringLiteral("No audio editor is installed.")));
        QVERIFY(!EditTools::choose(QUrl::fromLocalFile(m_folder.filePath(QStringLiteral("clip.mp4"))), QStringLiteral("video/mp4"), &error, installed({})));
        QVERIFY(error.startsWith(QStringLiteral("Plasma Opus has no edit tool for clip.mp4")));
        QVERIFY(!EditTools::choose(QUrl::fromLocalFile(m_folder.path()), QStringLiteral("text/plain"), &error, installed({})));
        QVERIFY(error.contains(QStringLiteral("not a file that can be edited")));

        const QUrl remoteImage(QStringLiteral("sftp://server/home/me/photo.png"));
        QVERIFY(!EditTools::choose(remoteImage, QStringLiteral("image/png"), &error, installed({Tool::Krita})));
        QCOMPARE(error, QStringLiteral("Krita can only open local files. Copy the file to a local folder first."));
        auto choice = EditTools::choose(remoteImage, QStringLiteral("image/png"), &error, installed({Tool::KolourPaint, Tool::Krita}));
        QVERIFY(choice);
        QCOMPARE(choice->tool, Tool::KolourPaint);
        QCOMPARE(EditTools::arguments(remoteImage), QStringList{remoteImage.toString()});

        const QUrl remoteText(QStringLiteral("sftp://server/home/me/notes.txt"));
        choice = EditTools::choose(remoteText, QStringLiteral("text/plain"), &error, installed({}));
        QVERIFY(choice);
        QCOMPARE(choice->tool, Tool::BuiltInEditor);

        const QUrl remoteSheet(QStringLiteral("smb://server/share/budget.ods"));
        choice = EditTools::choose(remoteSheet,
                                   QStringLiteral("application/vnd.oasis.opendocument.spreadsheet"),
                                   &error,
                                   installed({Tool::LibreOffice, Tool::Calligra}));
        QVERIFY(choice);
        QCOMPARE(choice->tool, Tool::Calligra);
    }

private:
    QTemporaryDir m_folder;
};

QTEST_GUILESS_MAIN(EditToolsTest)

#include "edittoolstest.moc"
