// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardmap.h"

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using KeyboardMap::Entry;

namespace
{
Entry entry(const QString &name, const QString &text, const QList<QKeySequence> &shortcuts, const QString &context = QStringLiteral("Window"))
{
    return {name, text, {}, shortcuts, {}, context};
}
}

class KeyboardMapTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void descriptionSaysMoreThanTheText()
    {
        QCOMPARE(KeyboardMap::description(QStringLiteral("&Find Action…"), QStringLiteral("Find Action"), {}, {}), QString());
        QCOMPARE(KeyboardMap::description(QStringLiteral("Copy"), QStringLiteral("copy."), QStringLiteral("Copy the selection"), {}),
                 QStringLiteral("Copy the selection"));
        QCOMPARE(KeyboardMap::description(QStringLiteral("Search"), QStringLiteral("Search the  current folder"), QStringLiteral("Other"), {}),
                 QStringLiteral("Search the current folder"));
        QCOMPARE(KeyboardMap::description(QStringLiteral("Tag"), {}, {}, QStringLiteral("<p>Assign <b>tags</b></p>")), QStringLiteral("Assign tags"));
        QCOMPARE(KeyboardMap::description(QStringLiteral("Tag"), {}, {}, {}), QString());
    }

    void matchesNamesWordsAndShortcutText()
    {
        Entry search = entry(QStringLiteral("search_folder"), QStringLiteral("Search Current Folder…"), {QKeySequence(QStringLiteral("Ctrl+Alt+F"))});
        search.description = QStringLiteral("Find files by name or content");
        QVERIFY(KeyboardMap::matchesText(search, {}));
        QVERIFY(KeyboardMap::matchesText(search, QStringLiteral("  ")));
        QVERIFY(KeyboardMap::matchesText(search, QStringLiteral("current SEARCH")));
        QVERIFY(KeyboardMap::matchesText(search, QStringLiteral("search_folder")));
        QVERIFY(KeyboardMap::matchesText(search, QStringLiteral("content")));
        QVERIFY(KeyboardMap::matchesText(search, QStringLiteral("ctrl+alt+f")));
        QVERIFY(KeyboardMap::matchesText(search, QStringLiteral("window")));
        QVERIFY(!KeyboardMap::matchesText(search, QStringLiteral("search tabs")));

        Entry yazi = entry(QStringLiteral("move_last"), QStringLiteral("Last item"), {QKeySequence(Qt::ShiftModifier | Qt::Key_G)});
        yazi.keysText = QStringLiteral("G");
        QCOMPARE(KeyboardMap::shortcutsText(yazi), QStringLiteral("G"));
        QVERIFY(KeyboardMap::matchesText(yazi, QStringLiteral("shift+g")));
    }

    void matchesPressedKeysAndTheSequencesTheyBegin()
    {
        const Entry chord = entry(QStringLiteral("copy_path"),
                                  QStringLiteral("Copy Path"),
                                  {QKeySequence(QStringLiteral("F9")), QKeySequence(QStringLiteral("Ctrl+K, Ctrl+C"))});
        QVERIFY(KeyboardMap::matchesKeys(chord, {}));
        QVERIFY(KeyboardMap::matchesKeys(chord, QKeySequence(QStringLiteral("F9"))));
        QVERIFY(KeyboardMap::matchesKeys(chord, QKeySequence(QStringLiteral("Ctrl+K"))));
        QVERIFY(KeyboardMap::matchesKeys(chord, QKeySequence(QStringLiteral("Ctrl+K, Ctrl+C"))));
        QVERIFY(!KeyboardMap::matchesKeys(chord, QKeySequence(QStringLiteral("Ctrl+C"))));
        QVERIFY(!KeyboardMap::matchesKeys(chord, QKeySequence(QStringLiteral("Ctrl+K, Ctrl+C, Ctrl+D"))));
        QCOMPARE(KeyboardMap::shortcutsText(chord),
                 QKeySequence(QStringLiteral("F9")).toString(QKeySequence::NativeText) + QStringLiteral("; ")
                     + QKeySequence(QStringLiteral("Ctrl+K, Ctrl+C")).toString(QKeySequence::NativeText));
    }

    void filtersByTextKeysAndShortcuts()
    {
        const QList<Entry> entries{entry(QStringLiteral("a"), QStringLiteral("Open Tab"), {QKeySequence(QStringLiteral("Ctrl+T"))}),
                                   entry(QStringLiteral("b"), QStringLiteral("Close Tab"), {}),
                                   entry(QStringLiteral("c"), QStringLiteral("Next item"), {QKeySequence(Qt::Key_J)}, QStringLiteral("Yazi"))};
        QCOMPARE(KeyboardMap::filter(entries, {}, {}, false), entries);
        QCOMPARE(KeyboardMap::filter(entries, {}, {}, true), (QList<Entry>{entries.at(0), entries.at(2)}));
        QCOMPARE(KeyboardMap::filter(entries, QStringLiteral("tab"), {}, false), (QList<Entry>{entries.at(0), entries.at(1)}));
        QCOMPARE(KeyboardMap::filter(entries, QStringLiteral("tab"), QKeySequence(QStringLiteral("Ctrl+T")), false), QList<Entry>{entries.at(0)});
        QCOMPARE(KeyboardMap::filter(entries, {}, QKeySequence(Qt::Key_J), true), QList<Entry>{entries.at(2)});
        QVERIFY(KeyboardMap::filter(entries, QStringLiteral("nothing"), {}, false).isEmpty());
    }

    void htmlIsEscapedAndGroupedByContext()
    {
        Entry risky = entry(QStringLiteral("x"), QStringLiteral("<script>alert(1)</script>"), {QKeySequence(QStringLiteral("Ctrl+<"))});
        risky.description = QStringLiteral("A & B");
        const QList<Entry> entries{risky,
                                   entry(QStringLiteral("y"), QStringLiteral("Next"), {QKeySequence(Qt::Key_Right)}, QStringLiteral("Image gallery")),
                                   entry(QStringLiteral("z"), QStringLiteral("Paste"), {})};
        const QString html = KeyboardMap::toHtml(entries, QStringLiteral("Keys & <more>"));
        QVERIFY(!html.contains(QLatin1String("<script>")));
        QVERIFY(html.contains(QLatin1String("&lt;script&gt;")));
        QVERIFY(html.contains(QLatin1String("A &amp; B")));
        QVERIFY(html.contains(QLatin1String("<title>Keys &amp; &lt;more&gt;</title>")));
        QVERIFY(html.contains(QLatin1String("<kbd>Ctrl+&lt;</kbd>")));
        QCOMPARE(html.count(QLatin1String("<h2>")), 2);
        QVERIFY(html.indexOf(QLatin1String("<h2>Window</h2>")) < html.indexOf(QLatin1String("<h2>Image gallery</h2>")));
        // Both window entries are in the first table, before the gallery's.
        QVERIFY(html.indexOf(QLatin1String("Paste")) < html.indexOf(QLatin1String("<h2>Image gallery</h2>")));
        QVERIFY(KeyboardMap::toHtml({}, QStringLiteral("Empty")).contains(QLatin1String("<p>")));
    }

    void exportsHtmlAndPdf()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QList<Entry> entries{entry(QStringLiteral("a"), QStringLiteral("Open Tab"), {QKeySequence(QStringLiteral("Ctrl+T"))})};
        const QString html = dir.filePath(QStringLiteral("keys.html"));
        QVERIFY(KeyboardMap::exportReference(entries, QStringLiteral("Keys"), html).isEmpty());
        QFile htmlFile(html);
        QVERIFY(htmlFile.open(QIODevice::ReadOnly));
        const QByteArray htmlData = htmlFile.readAll();
        QVERIFY(htmlData.startsWith("<!DOCTYPE html>"));
        QVERIFY(htmlData.contains("Open Tab"));

        const QString pdf = dir.filePath(QStringLiteral("keys.PDF"));
        QVERIFY(KeyboardMap::exportReference(entries, QStringLiteral("Keys"), pdf).isEmpty());
        QFile pdfFile(pdf);
        QVERIFY(pdfFile.open(QIODevice::ReadOnly));
        QVERIFY(pdfFile.readAll().startsWith("%PDF"));

        QVERIFY(!KeyboardMap::exportReference(entries, QStringLiteral("Keys"), dir.filePath(QStringLiteral("missing/keys.html"))).isEmpty());
        QVERIFY(!KeyboardMap::exportReference(entries, QStringLiteral("Keys"), {}).isEmpty());
    }
};

QTEST_MAIN(KeyboardMapTest)
#include "keyboardmaptest.moc"
