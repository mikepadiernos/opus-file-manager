// SPDX-License-Identifier: GPL-3.0-or-later

#include "namefilter.h"

#include <QTest>

class NameFilterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyFilterShowsEverything()
    {
        const NameFilter filter(QStringLiteral("   "));
        QVERIFY(filter.isEmpty());
        QVERIFY(filter.matches(QStringLiteral("anything"), false));
    }

    void plainTextMatchesAnywhereIgnoringCase()
    {
        const NameFilter filter(QStringLiteral("Port"));
        QVERIFY(!filter.isEmpty());
        QVERIFY(filter.matches(QStringLiteral("report.pdf"), false));
        QVERIFY(!filter.matches(QStringLiteral("notes.txt"), false));
    }

    void plainTextKeepsSpaces()
    {
        const NameFilter filter(QStringLiteral("my file"));
        QVERIFY(filter.matches(QStringLiteral("My File.txt"), false));
        QVERIFY(!filter.matches(QStringLiteral("my.txt"), false));
    }

    void globPatternsMatchWholeNames()
    {
        const NameFilter filter(QStringLiteral("*.jpg, *.PNG;photo?.gif"));
        QVERIFY(filter.matches(QStringLiteral("a.JPG"), false));
        QVERIFY(filter.matches(QStringLiteral("b.png"), false));
        QVERIFY(filter.matches(QStringLiteral("photo1.gif"), false));
        QVERIFY(!filter.matches(QStringLiteral("a.jpg.bak"), false));
        QVERIFY(!filter.matches(QStringLiteral("photo12.gif"), false));
    }

    void questionMarkStandsForASpace()
    {
        const NameFilter filter(QStringLiteral("my?file*"));
        QVERIFY(filter.matches(QStringLiteral("my file.txt"), false));
    }

    void scopeDecidesWhatIsFiltered()
    {
        const NameFilter files(QStringLiteral("*.jpg"));
        QVERIFY(files.matches(QStringLiteral("Pictures"), true));
        QVERIFY(!files.matches(QStringLiteral("notes.txt"), false));

        const NameFilter folders(QStringLiteral("pic"), NameFilter::Scope::Folders);
        QVERIFY(!folders.matches(QStringLiteral("Music"), true));
        QVERIFY(folders.matches(QStringLiteral("notes.txt"), false));

        const NameFilter both(QStringLiteral("pic"), NameFilter::Scope::FilesAndFolders);
        QVERIFY(!both.matches(QStringLiteral("Music"), true));
        QVERIFY(!both.matches(QStringLiteral("notes.txt"), false));
        QVERIFY(both.matches(QStringLiteral("picture.png"), false));

        QVERIFY(files.appliesTo(false));
        QVERIFY(!files.appliesTo(true));
        QVERIFY(folders.appliesTo(true));
        QVERIFY(!NameFilter().appliesTo(false));
    }
};

QTEST_GUILESS_MAIN(NameFilterTest)

#include "namefiltertest.moc"
