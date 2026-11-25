// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchquery.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using SearchQuery::Query;

class SearchQueryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void normalizesValidQueries_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("normalized");
        QTest::newRow("word") << QStringLiteral("report") << QStringLiteral("name:\"report\"");
        QTest::newRow("implicit and") << QStringLiteral("a b") << QStringLiteral("(name:\"a\" AND name:\"b\")");
        QTest::newRow("and binds tighter") << QStringLiteral("a b OR c") << QStringLiteral("((name:\"a\" AND name:\"b\") OR name:\"c\")");
        QTest::newRow("or then and") << QStringLiteral("a OR b c") << QStringLiteral("(name:\"a\" OR (name:\"b\" AND name:\"c\"))");
        QTest::newRow("parentheses") << QStringLiteral("a AND (b OR c)") << QStringLiteral("(name:\"a\" AND (name:\"b\" OR name:\"c\"))");
        QTest::newRow("flattened") << QStringLiteral("a AND b AND (c d)") << QStringLiteral("(name:\"a\" AND name:\"b\" AND name:\"c\" AND name:\"d\")");
        QTest::newRow("minus") << QStringLiteral("-a") << QStringLiteral("NOT name:\"a\"");
        QTest::newRow("double negation") << QStringLiteral("NOT -a") << QStringLiteral("name:\"a\"");
        QTest::newRow("hyphen inside word") << QStringLiteral("a-b") << QStringLiteral("name:\"a-b\"");
        QTest::newRow("lone hyphen") << QStringLiteral("-") << QStringLiteral("name:\"-\"");
        QTest::newRow("quoted operator") << QStringLiteral("\"OR\"") << QStringLiteral("name:\"OR\"");
        QTest::newRow("lower-case operator") << QStringLiteral("a or b") << QStringLiteral("(name:\"a\" AND name:\"or\" AND name:\"b\")");
        QTest::newRow("quoted phrase") << QStringLiteral("content:\"two words\"") << QStringLiteral("content:\"two words\"");
        QTest::newRow("field aliases") << QStringLiteral("FileName=x Date:2026 contents:y")
                                       << QStringLiteral("(name=\"x\" AND modified=\"2026\" AND content:\"y\")");
        QTest::newRow("size") << QStringLiteral("size:10M size>=1.5K") << QStringLiteral("(size=\"10M\" AND size>=\"1.5K\")");
        QTest::newRow("extension") << QStringLiteral("type:.PDF") << QStringLiteral("type:\"pdf\"");
        QTest::newRow("mime type") << QStringLiteral("type:Image/PNG") << QStringLiteral("type:\"image/png\"");
        QTest::newRow("mime prefix") << QStringLiteral("type:Image/*") << QStringLiteral("type:\"image/*\"");
        QTest::newRow("value with colon") << QStringLiteral("path:\"C:x\"") << QStringLiteral("path:\"C:x\"");
        QTest::newRow("tags") << QStringLiteral("Tag:Cat tags=\"Big cat\"") << QStringLiteral("(tag:\"Cat\" AND tag=\"Big cat\")");
        QTest::newRow("tagged") << QStringLiteral("tagged:YES OR tagged=false") << QStringLiteral("(tagged:\"yes\" OR tagged:\"no\")");
        QTest::newRow("metadata") << QStringLiteral("Meta:Canon metadata=Jazz") << QStringLiteral("(meta:\"Canon\" AND meta=\"Jazz\")");
    }

    void normalizesValidQueries()
    {
        QFETCH(QString, text);
        QFETCH(QString, normalized);
        const Query query = Query::parse(text);
        QVERIFY2(query.isValid(), qPrintable(query.error()));
        QCOMPARE(query.toString(), normalized);
    }

    void rejectsInvalidQueries_data()
    {
        QTest::addColumn<QString>("text");
        for (const char *text : {"",
                                 "   ",
                                 "AND a",
                                 "a OR",
                                 "a AND OR b",
                                 "NOT",
                                 "(a",
                                 "a)",
                                 "()",
                                 "foo:bar",
                                 "\"abc",
                                 "\"\"",
                                 "a\"b",
                                 "\"a\"b",
                                 "name:",
                                 "name=>x",
                                 "name>a",
                                 "content=x",
                                 "type>pdf",
                                 "type:*",
                                 "type:nothing/here",
                                 "size:big",
                                 "size<x",
                                 "modified:2026-13-01",
                                 "modified:soon",
                                 "tag:",
                                 "tag>a",
                                 "tagged:maybe",
                                 "tagged<=yes",
                                 "meta:",
                                 "meta>2020"}) {
            QTest::newRow(text) << QString::fromUtf8(text);
        }
    }

    void rejectsInvalidQueries()
    {
        QFETCH(QString, text);
        const Query query = Query::parse(text);
        QVERIFY(!query.isValid());
        QVERIFY(!query.error().isEmpty());
        QVERIFY(query.toString().isEmpty());
        QVERIFY(!query.matches(QFileInfo(QDir::tempPath())));
    }

    void reportsRequiredAndContentTerms()
    {
        const Query query = Query::parse(QStringLiteral("report type:pdf content:Invoice -name:old content:invoice (content:x OR y)"));
        QVERIFY(query.isValid());
        QVERIFY(query.usesContent());
        QCOMPARE(query.contentTerms(), (QStringList{QStringLiteral("Invoice"), QStringLiteral("x")}));
        const QList<SearchQuery::Term> required = query.requiredTerms();
        QCOMPARE(required.size(), 3);
        QCOMPARE(required.at(0).text, QStringLiteral("report"));
        QCOMPARE(required.at(1).field, SearchQuery::Field::Content);
        QCOMPARE(required.at(2).text, QStringLiteral("invoice"));
        QCOMPARE(Query::parse(QStringLiteral("solo")).requiredTerms().size(), 1);
        QVERIFY(Query::parse(QStringLiteral("a OR b")).requiredTerms().isEmpty());
        QVERIFY(Query::parse(QStringLiteral("name:*.pdf size>1K")).requiredTerms().isEmpty());
        QVERIFY(Query::parse(QStringLiteral("-report")).requiredTerms().isEmpty());
        QVERIFY(!Query::parse(QStringLiteral("report")).usesContent());
        QVERIFY(Query::parse(QStringLiteral("a OR NOT tagged:no")).usesTags());
        QVERIFY(Query::parse(QStringLiteral("tag:x")).requiredTerms().isEmpty());
        QVERIFY(!Query::parse(QStringLiteral("\"tag:x\"")).usesTags());
    }

    void evaluatesMetadata()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("archive")));
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("reports")));
        QFile report(root.filePath(QStringLiteral("Report.pdf")));
        QVERIFY(report.open(QIODevice::WriteOnly));
        QCOMPARE(report.write(QByteArray(2048, 'x')), 2048);
        QVERIFY(report.flush());
        QVERIFY(report.setFileTime(QDateTime(QDate(2025, 6, 15), QTime(12, 0)), QFileDevice::FileModificationTime));
        report.close();
        QFile notes(root.filePath(QStringLiteral("notes.txt")));
        QVERIFY(notes.open(QIODevice::WriteOnly));
        QCOMPARE(notes.write("hello"), 5);
        notes.close();
        QFile old(root.filePath(QStringLiteral("archive/old-report.pdf")));
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.close();
        const QFileInfo reportInfo(report.fileName());
        const QFileInfo notesInfo(notes.fileName());
        const QFileInfo oldInfo(old.fileName());
        const QFileInfo folderInfo(root.filePath(QStringLiteral("reports")));
        const auto matches = [](const QString &text, const QFileInfo &entry, const QDateTime &now = QDateTime::currentDateTime()) {
            const Query query = Query::parse(text, now);
            if (!query.isValid()) {
                qWarning() << text << query.error();
                return false;
            }
            return query.matches(entry);
        };

        QVERIFY(matches(QStringLiteral("report"), reportInfo));
        QVERIFY(matches(QStringLiteral("report"), oldInfo));
        QVERIFY(!matches(QStringLiteral("report"), notesInfo));
        QVERIFY(matches(QStringLiteral("report -path:archive"), reportInfo));
        QVERIFY(!matches(QStringLiteral("report -path:archive"), oldInfo));
        QVERIFY(matches(QStringLiteral("name=report.PDF"), reportInfo));
        QVERIFY(!matches(QStringLiteral("name=report"), reportInfo));
        QVERIFY(matches(QStringLiteral("name:*.PDF"), reportInfo));
        QVERIFY(!matches(QStringLiteral("name:\"*.pdf\""), reportInfo));
        QVERIFY(matches(QStringLiteral("R?port*"), reportInfo));
        QVERIFY(!matches(QStringLiteral("R?port"), reportInfo));
        QVERIFY(matches(QStringLiteral("notes OR old"), notesInfo));
        QVERIFY(matches(QStringLiteral("notes OR old"), oldInfo));
        QVERIFY(!matches(QStringLiteral("notes OR old"), reportInfo));

        QVERIFY(matches(QStringLiteral("type:file"), reportInfo));
        QVERIFY(!matches(QStringLiteral("type:file"), folderInfo));
        QVERIFY(matches(QStringLiteral("type:folder"), folderInfo));
        QVERIFY(matches(QStringLiteral("type:pdf"), reportInfo));
        QVERIFY(!matches(QStringLiteral("type:pdf"), notesInfo));
        QVERIFY(matches(QStringLiteral("type:text/plain"), notesInfo));
        QVERIFY(matches(QStringLiteral("type:text/*"), notesInfo));
        QVERIFY(!matches(QStringLiteral("type:image/*"), notesInfo));

        QVERIFY(matches(QStringLiteral("size:2K"), reportInfo));
        QVERIFY(matches(QStringLiteral("size>1K"), reportInfo));
        QVERIFY(matches(QStringLiteral("size>=2KiB"), reportInfo));
        QVERIFY(!matches(QStringLiteral("size<2K"), reportInfo));
        QVERIFY(matches(QStringLiteral("size<=2048"), reportInfo));
        QVERIFY(!matches(QStringLiteral("size<1G"), folderInfo));
        QVERIFY(matches(QStringLiteral("NOT size>1M"), folderInfo));

        QVERIFY(matches(QStringLiteral("modified:2025"), reportInfo));
        QVERIFY(matches(QStringLiteral("modified:2025-06"), reportInfo));
        QVERIFY(matches(QStringLiteral("modified=2025-06-15"), reportInfo));
        QVERIFY(!matches(QStringLiteral("modified:2025-06-14"), reportInfo));
        QVERIFY(!matches(QStringLiteral("modified<2025-06-15"), reportInfo));
        QVERIFY(matches(QStringLiteral("modified<=2025-06-15"), reportInfo));
        QVERIFY(matches(QStringLiteral("modified>2025-06-14"), reportInfo));
        QVERIFY(!matches(QStringLiteral("modified>2025-06-15"), reportInfo));
        QVERIFY(matches(QStringLiteral("modified>=2025-06-15"), reportInfo));
        QVERIFY(matches(QStringLiteral("modified<2026"), reportInfo));
        QVERIFY(matches(QStringLiteral("modified:today"), reportInfo, QDateTime(QDate(2025, 6, 15), QTime(18, 0))));
        QVERIFY(matches(QStringLiteral("modified:yesterday"), reportInfo, QDateTime(QDate(2025, 6, 16), QTime(1, 0))));
        QVERIFY(!matches(QStringLiteral("modified:today"), reportInfo, QDateTime(QDate(2025, 6, 16), QTime(1, 0))));
    }

    void evaluatesContentLazily()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile file(root.filePath(QStringLiteral("notes.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QFileInfo fileInfo(file.fileName());
        const QFileInfo folderInfo(root.path());
        QStringList needles;
        const SearchQuery::ContentMatcher content = [&needles](const QString &needle) {
            needles.append(needle);
            return needle == QLatin1String("hello");
        };

        QVERIFY(Query::parse(QStringLiteral("content:hello")).matches(fileInfo, content));
        QCOMPARE(needles, QStringList{QStringLiteral("hello")});
        needles.clear();
        QVERIFY(!Query::parse(QStringLiteral("content:hello zzz")).matches(fileInfo, content));
        QVERIFY(needles.isEmpty());
        QVERIFY(Query::parse(QStringLiteral("content:other OR notes")).matches(fileInfo, content));
        QVERIFY(needles.isEmpty());
        QVERIFY(!Query::parse(QStringLiteral("content:hello")).matches(folderInfo, content));
        QVERIFY(needles.isEmpty());
        QVERIFY(Query::parse(QStringLiteral("NOT content:other")).matches(fileInfo, content));
        QCOMPARE(needles, QStringList{QStringLiteral("other")});
        QVERIFY(!Query::parse(QStringLiteral("content:hello")).matches(fileInfo));
    }

    void evaluatesTagsThroughTheMatcher()
    {
        const QFileInfo entry(QDir::tempPath());
        QList<SearchQuery::Term> seen;
        const SearchQuery::TagMatcher tags = [&seen](const SearchQuery::Term &term) {
            seen.append(term);
            if (term.field == SearchQuery::Field::Tagged) {
                return term.tagged;
            }
            return term.text == QLatin1String("cat") && term.comparison == SearchQuery::Comparison::Contains;
        };
        QVERIFY(Query::parse(QStringLiteral("tag:cat")).matches(entry, {}, tags));
        QCOMPARE(seen.size(), 1);
        QVERIFY(seen.first().pattern.pattern().isEmpty());
        QVERIFY(!Query::parse(QStringLiteral("tag=cat")).matches(entry, {}, tags));
        QVERIFY(Query::parse(QStringLiteral("tagged:yes NOT tag:dog")).matches(entry, {}, tags));
        QVERIFY(!Query::parse(QStringLiteral("tagged:no")).matches(entry, {}, tags));
        seen.clear();
        QVERIFY(!Query::parse(QStringLiteral("tag:ca*")).matches(entry, {}, tags));
        QCOMPARE(seen.size(), 1);
        QVERIFY(seen.first().pattern.match(QStringLiteral("CAT")).hasMatch());
        QVERIFY(!seen.first().pattern.match(QStringLiteral("scat")).hasMatch());
        seen.clear();
        QVERIFY(!Query::parse(QStringLiteral("tag:\"ca*\"")).matches(entry, {}, tags));
        QVERIFY(seen.first().pattern.pattern().isEmpty());
        // Without a matcher, no tag term matches, not even tagged:no.
        QVERIFY(!Query::parse(QStringLiteral("tag:cat")).matches(entry));
        QVERIFY(!Query::parse(QStringLiteral("tagged:no")).matches(entry));
        QVERIFY(Query::parse(QStringLiteral("NOT tagged:yes")).matches(entry));
    }

    void evaluatesEmbeddedMetadataLazily()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile file(root.filePath(QStringLiteral("photo.jpg")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QFileInfo fileInfo(file.fileName());
        const QFileInfo folderInfo(root.path());
        int reads = 0;
        const SearchQuery::MetadataReader metadata = [&reads]() {
            ++reads;
            return QStringList{QStringLiteral("Canon"), QStringLiteral("Canon EOS R5"), QStringLiteral("2026-02-28")};
        };

        QVERIFY(Query::parse(QStringLiteral("meta:eos")).usesMetadata());
        QVERIFY(!Query::parse(QStringLiteral("\"meta:eos\"")).usesMetadata());
        QVERIFY(Query::parse(QStringLiteral("meta:eos")).matches(fileInfo, {}, {}, metadata));
        QVERIFY(Query::parse(QStringLiteral("meta=canon")).matches(fileInfo, {}, {}, metadata));
        QVERIFY(!Query::parse(QStringLiteral("meta=eos")).matches(fileInfo, {}, {}, metadata));
        QVERIFY(Query::parse(QStringLiteral("meta:2026-02*")).matches(fileInfo, {}, {}, metadata));
        QVERIFY(!Query::parse(QStringLiteral("meta:\"2026-02*\"")).matches(fileInfo, {}, {}, metadata));
        reads = 0;
        // Cheap criteria decide first, so the metadata isn't read.
        QVERIFY(!Query::parse(QStringLiteral("meta:canon zzz")).matches(fileInfo, {}, {}, metadata));
        QVERIFY(Query::parse(QStringLiteral("meta:nikon OR photo")).matches(fileInfo, {}, {}, metadata));
        QVERIFY(!Query::parse(QStringLiteral("meta:canon")).matches(folderInfo, {}, {}, metadata));
        QCOMPARE(reads, 0);
        QVERIFY(!Query::parse(QStringLiteral("meta:canon")).matches(fileInfo));
    }
};

QTEST_GUILESS_MAIN(SearchQueryTest)
#include "searchquerytest.moc"
