// SPDX-License-Identifier: GPL-3.0-or-later

#include "fontpreview.h"
#include "previewpanel.h"

#include <KFileItem>
#include <KParts/PartLoader>

#include <QDirIterator>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

class FontPreviewTest : public QObject
{
    Q_OBJECT

private:
    static QString systemFont()
    {
        QDirIterator it(QStringLiteral("/usr/share/fonts"), {QStringLiteral("*.otf"), QStringLiteral("*.ttf")}, QDir::Files, QDirIterator::Subdirectories);
        return it.hasNext() ? it.next() : QString();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_folder.isValid());
        m_path = qgetenv("PATH");
        // No font installer is in PATH.
        qputenv("PATH", m_folder.path().toLocal8Bit());
    }

    void cleanupTestCase()
    {
        qputenv("PATH", m_path);
    }

    void showsEditableSamples()
    {
        const QString font = systemFont();
        if (font.isEmpty()) {
            QSKIP("No system font file to preview");
        }
        const QString copy = m_folder.filePath(QStringLiteral("sample") + font.mid(font.lastIndexOf(QLatin1Char('.'))));
        QVERIFY(QFile::copy(font, copy));
        const QString mimeType = font.endsWith(QLatin1String(".otf")) ? QStringLiteral("font/otf") : QStringLiteral("font/ttf");

        FontPreview preview;
        QVERIFY2(preview.open(copy, QUrl::fromLocalFile(copy), mimeType).isEmpty(), qPrintable(copy));
        QVERIFY(!preview.family().isEmpty());

        auto *sample = preview.findChild<QLineEdit *>(QStringLiteral("fontPreviewSample"));
        QVERIFY(sample);
        auto *large = preview.findChild<QLabel *>(QStringLiteral("fontPreviewSample40"));
        QVERIFY(large);
        QCOMPARE(large->text(), preview.sampleText());
        QCOMPARE(large->font().family(), preview.family());
        QCOMPARE(large->font().pointSize(), 40);
        sample->setText(QStringLiteral("Plasma Opus 123"));
        QCOMPARE(large->text(), QStringLiteral("Plasma Opus 123"));

        auto *install = preview.findChild<QToolButton *>(QStringLiteral("fontPreviewInstall"));
        QVERIFY(install);
        QVERIFY(!install->isEnabled());
        QVERIFY(install->toolTip().contains(QStringLiteral("kfontinst")));
        QVERIFY(preview.open(copy, QUrl::fromLocalFile(copy), QStringLiteral("font/woff2")).isEmpty());
        QVERIFY(install->toolTip().contains(QStringLiteral("does not install this font format")));

        preview.clear();
        QVERIFY(preview.family().isEmpty());
        QVERIFY(large->text().isEmpty());
        // The sample text is kept for the next font.
        FontPreview another;
        QCOMPARE(another.sampleText(), QStringLiteral("Plasma Opus 123"));
        sample->clear();
    }

    void reportsDamagedFonts()
    {
        const QString broken = m_folder.filePath(QStringLiteral("broken.ttf"));
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a font");
        file.close();
        FontPreview preview;
        QVERIFY(preview.open(broken, QUrl::fromLocalFile(broken), QStringLiteral("font/ttf")).contains(QStringLiteral("Could not load this font")));
        QVERIFY(preview.open(m_folder.filePath(QStringLiteral("missing.ttf")), {}, QStringLiteral("font/ttf")).contains(QStringLiteral("Could not open")));
    }

    void previewPanelShowsFontsAndNamesMissingViewers()
    {
        PreviewPanel panel;
        auto *content = panel.findChild<QStackedWidget *>();
        QVERIFY(content);
        const QString font = systemFont();
        if (!font.isEmpty()) {
            const QString copy = m_folder.filePath(QStringLiteral("panel") + font.mid(font.lastIndexOf(QLatin1Char('.'))));
            QVERIFY(QFile::copy(font, copy));
            panel.setItems({KFileItem(QUrl::fromLocalFile(copy))});
            QTRY_COMPARE(content->currentWidget()->objectName(), QStringLiteral("previewFont"));
            QVERIFY(!panel.isBusy());
        }

        const QString book = m_folder.filePath(QStringLiteral("book.epub"));
        if (!KParts::PartLoader::partsForMimeType(QStringLiteral("application/epub+zip")).isEmpty()) {
            return;
        }
        QFile file(book);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("PK");
        file.close();
        panel.setItems({KFileItem(QUrl::fromLocalFile(book))});
        auto *message = panel.findChild<QLabel *>(QStringLiteral("previewMessage"));
        QVERIFY(message);
        QTRY_COMPARE(content->currentWidget(), message);
        QVERIFY2(message->text().contains(QStringLiteral("Install Okular")), qPrintable(message->text()));
    }

private:
    QTemporaryDir m_folder;
    QByteArray m_path;
};

QTEST_MAIN(FontPreviewTest)

#include "fontpreviewtest.moc"
