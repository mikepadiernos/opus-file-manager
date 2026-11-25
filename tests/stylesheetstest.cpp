// SPDX-License-Identifier: GPL-3.0-or-later

#include "stylesheets.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class StyleSheetsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void acceptsQtStyleSheets_data()
    {
        QTest::addColumn<QString>("sheet");
        QTest::newRow("empty") << QString();
        QTest::newRow("rule") << QStringLiteral("QStatusBar { border-top: 1px solid palette(mid); }");
        QTest::newRow("declarations only") << QStringLiteral("color: #202020;");
        QTest::newRow("resource image") << QStringLiteral("QPushButton { image: url(:/icons/open.png); }");
        QTest::newRow("quoted resource") << QStringLiteral("QPushButton { border-image: url(\"qrc:/frame.png\") 4; }");
        QTest::newRow("braces in comment") << QStringLiteral("/* { url(/etc/passwd) @import */ QLabel { color: red; }");
        QTest::newRow("braces in string") << QStringLiteral("QLabel[text=\"{ @ }\"] { color: red; }");
        QTest::newRow("gradient") << QStringLiteral("QTabBar::tab:selected { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #fff, stop:1 #eee); }");
    }

    void acceptsQtStyleSheets()
    {
        QFETCH(QString, sheet);
        QCOMPARE(StyleSheets::check(sheet), QString());
    }

    void refusesUnsafeOrInvalidInput_data()
    {
        QTest::addColumn<QString>("sheet");
        QTest::addColumn<QString>("mentions");
        QTest::newRow("file image") << QStringLiteral("QLabel {\n image: url(/home/user/a.png); }") << QStringLiteral("Line 2");
        QTest::newRow("web image") << QStringLiteral("QLabel { image: url('https://example.com/a.png'); }") << QStringLiteral("https://example.com/a.png");
        QTest::newRow("relative image") << QStringLiteral("QLabel { image: URL(a.png); }") << QStringLiteral("a.png");
        QTest::newRow("import") << QStringLiteral("@import \"other.qss\";") << QStringLiteral("@import");
        QTest::newRow("qproperty") << QStringLiteral("QLineEdit { qproperty-readOnly: true; }") << QStringLiteral("qproperty-");
        QTest::newRow("nested") << QStringLiteral("QLabel { QPushButton { color: red; } }") << QStringLiteral("nested");
        QTest::newRow("unclosed rule") << QStringLiteral("\n\nQLabel { color: red;") << QStringLiteral("Line 3");
        QTest::newRow("extra brace") << QStringLiteral("QLabel { color: red; } }") << QStringLiteral("}");
        QTest::newRow("unclosed comment") << QStringLiteral("QLabel { color: red; } /* note") << QStringLiteral("*/");
        QTest::newRow("unclosed string") << QStringLiteral("QLabel[text=\"a] { color: red; }") << QStringLiteral("quoted");
        QTest::newRow("unclosed parenthesis") << QStringLiteral("QLabel { color: rgb(1, 2, 3; }") << QStringLiteral("(");
        QTest::newRow("control character") << QStringLiteral("QLabel { color: red; }\x01") << QStringLiteral("Control");
        QTest::newRow("too long") << QString(StyleSheets::MaximumSize + 1, QLatin1Char(' ')) << QString();
        QString many;
        for (int i = 0; i <= StyleSheets::MaximumRules; ++i) {
            many += QStringLiteral("QLabel{}");
        }
        QTest::newRow("too many rules") << many << QStringLiteral("rules");
    }

    void refusesUnsafeOrInvalidInput()
    {
        QFETCH(QString, sheet);
        QFETCH(QString, mentions);
        const QString error = StyleSheets::check(sheet);
        QVERIFY(!error.isEmpty());
        QVERIFY2(error.contains(mentions, Qt::CaseInsensitive), qPrintable(error));
    }

    void readsAndWritesBoundedFiles()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("opus.qss"));
        const QString sheet = QStringLiteral("QLabel { color: #ff0000; } /* café */");
        QCOMPARE(StyleSheets::writeFile(path, sheet), QString());
        QString error;
        QCOMPARE(StyleSheets::readFile(path, &error).value_or(QString()), sheet);

        QFile large(dir.filePath(QStringLiteral("large.qss")));
        QVERIFY(large.open(QIODevice::WriteOnly));
        large.write(QByteArray(StyleSheets::MaximumSize + 1, ' '));
        large.close();
        QVERIFY(!StyleSheets::readFile(large.fileName(), &error));
        QVERIFY(!error.isEmpty());

        QFile binary(dir.filePath(QStringLiteral("binary.qss")));
        QVERIFY(binary.open(QIODevice::WriteOnly));
        binary.write("QLabel { color: \xff\xfe; }");
        binary.close();
        QVERIFY(!StyleSheets::readFile(binary.fileName(), &error));
        QVERIFY(!StyleSheets::readFile(dir.filePath(QStringLiteral("missing.qss")), &error));
    }
};

QTEST_GUILESS_MAIN(StyleSheetsTest)

#include "stylesheetstest.moc"
