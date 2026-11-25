// SPDX-License-Identifier: GPL-3.0-or-later

#include "opus_searchquery.h"
#include "searchquery.h"

#include <QStandardPaths>
#include <QTest>
#include <QtConcurrent>

using SearchQuery::Query;

namespace
{
/// Normalized form of a valid query, or "error: " and the message.
QString cppResult(const QString &text)
{
    const Query query = Query::parse(text);
    return query.isValid() ? query.toString() : QStringLiteral("error: ") + query.error();
}

QString rustResult(const QString &text)
{
    const OpusSearchQuery::Result result = OpusSearchQuery::check(text);
    switch (result.status) {
    case OPUS_QUERY_VALID:
        return result.text;
    case OPUS_QUERY_INVALID:
        return QStringLiteral("error: ") + result.text;
    default:
        return QStringLiteral("status %1").arg(result.status);
    }
}

/// Compares through UTF-8, with the C++ result encoded as the input to Rust is, so unpaired surrogates match U+FFFD.
void compareParsers(const QString &text)
{
    const QByteArray cpp = OpusSearchQuery::toUtf8(cppResult(text));
    const QByteArray rust = rustResult(text).toUtf8();
    if (cpp != rust) {
        QFAIL(qPrintable(QStringLiteral("Query %1\n   C++: %2\n  Rust: %3").arg(text, QString::fromUtf8(cpp), QString::fromUtf8(rust))));
    }
}

QStringList fixedCorpus()
{
    return {
        QStringLiteral("report"),
        QStringLiteral("a b"),
        QStringLiteral("a b OR c"),
        QStringLiteral("a OR b c"),
        QStringLiteral("a AND (b OR c)"),
        QStringLiteral("a AND b AND (c d)"),
        QStringLiteral("-a"),
        QStringLiteral("NOT -a"),
        QStringLiteral("-(a b)"),
        QStringLiteral("a-b"),
        QStringLiteral("-"),
        QStringLiteral("\"OR\""),
        QStringLiteral("a or b"),
        QStringLiteral("content:\"two words\""),
        QStringLiteral("FileName=x Date:2026 contents:y"),
        QStringLiteral("size:10M size>=1.5K"),
        QStringLiteral("size:\"2 KiB\" size<\"10\n\" size:10. size:9000000T"),
        QStringLiteral("type:.PDF type:*.txt type:Folders type:files"),
        QStringLiteral("type:Image/PNG"),
        QStringLiteral("type:Image/*"),
        QStringLiteral("path:\"C:x\""),
        QStringLiteral("modified:2024-02-29 modified:2000-02-29 modified:1900-02-29"),
        QStringLiteral("modified:0000"),
        QStringLiteral("modified:Today OR modified<yesterday"),
        QStringLiteral("café naïve Straße 名前"),
        QStringLiteral("名前:x"),
        QStringLiteral(""),
        QStringLiteral("   "),
        QStringLiteral("AND a"),
        QStringLiteral("a OR"),
        QStringLiteral("a AND OR b"),
        QStringLiteral("NOT"),
        QStringLiteral("(a"),
        QStringLiteral("a)"),
        QStringLiteral("()"),
        QStringLiteral("foo:bar"),
        QStringLiteral("\"abc"),
        QStringLiteral("\"\""),
        QStringLiteral("a\"b"),
        QStringLiteral("\"a\"b"),
        QStringLiteral("name:"),
        QStringLiteral("name=>x"),
        QStringLiteral("name>a"),
        QStringLiteral("content=x"),
        QStringLiteral("type>pdf"),
        QStringLiteral("type:*"),
        QStringLiteral("size:big"),
        QStringLiteral("size<x"),
        QStringLiteral("modified:2026-13-01"),
        QStringLiteral("modified:soon"),
        QStringLiteral("Tag:Cat tags=\"Big cat\" tag:a*"),
        QStringLiteral("tagged:YES OR tagged=false"),
        QStringLiteral("tag>a"),
        QStringLiteral("tagged:maybe"),
        QStringLiteral("tagged<yes"),
        QStringLiteral("tag:"),
    };
}

/// Builds queries from atoms with a fixed linear congruential generator, so failures reproduce.
QStringList generatedCorpus(int count)
{
    static const QStringList atoms = {
        QStringLiteral("AND"),
        QStringLiteral("OR"),
        QStringLiteral("NOT"),
        QStringLiteral("-"),
        QStringLiteral("-a"),
        QStringLiteral("("),
        QStringLiteral(")"),
        QStringLiteral("report"),
        QStringLiteral("*.pdf"),
        QStringLiteral("name:x?"),
        QStringLiteral("name="),
        QStringLiteral("path:/tmp"),
        QStringLiteral("\"two words\""),
        QStringLiteral("\"\""),
        QStringLiteral("type:pdf"),
        QStringLiteral("type:folder"),
        QStringLiteral("type:text/*"),
        QStringLiteral("type:text/plain"),
        QStringLiteral("type:inode/directory"),
        QStringLiteral("size>10K"),
        QStringLiteral("size<=1.5M"),
        QStringLiteral("size:big"),
        QStringLiteral("modified:2026-11"),
        QStringLiteral("modified>=2025-02-29"),
        QStringLiteral("modified:today"),
        QStringLiteral("content:invoice"),
        QStringLiteral("content=x"),
        QStringLiteral("foo:bar"),
        QStringLiteral("tag:cat"),
        QStringLiteral("tag=a*"),
        QStringLiteral("tagged:no"),
        QStringLiteral("tagged:x"),
        QStringLiteral("café"),
    };
    QStringList queries;
    quint32 state = 0x5eed;
    const auto next = [&state](quint32 bound) {
        state = state * 1664525u + 1013904223u;
        return (state >> 16) % bound;
    };
    for (int i = 0; i < count; ++i) {
        QString query;
        bool needsSpace = false;
        const quint32 length = 1 + next(6);
        for (quint32 j = 0; j < length; ++j) {
            if (j > 0 && (needsSpace || next(8) != 0)) {
                query += QLatin1Char(' ');
            }
            const QString &atom = atoms.at(qsizetype(next(quint32(atoms.size()))));
            // Text appended to a MIME type makes an unknown one, which only C++ can detect.
            needsSpace = atom.startsWith(QLatin1String("type:")) && atom.contains(QLatin1Char('/'));
            query += atom;
        }
        queries.append(query);
    }
    return queries;
}
}

class RustSearchQueryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void matchesFixedCorpus_data()
    {
        QTest::addColumn<QString>("text");
        for (const QString &text : fixedCorpus()) {
            QTest::newRow(qPrintable(text)) << text;
        }
    }

    void matchesFixedCorpus()
    {
        QFETCH(QString, text);
        compareParsers(text);
    }

    void matchesGeneratedQueries()
    {
        for (const QString &text : generatedCorpus(3000)) {
            compareParsers(text);
            if (QTest::currentTestFailed()) {
                return;
            }
        }
    }

    void handlesEmbeddedNulAndLoneSurrogates()
    {
        QCOMPARE(OpusSearchQuery::toUtf8(QString(QChar(0xD800)) + QStringLiteral("x")), QByteArray("\xef\xbf\xbdx"));
        QCOMPARE(OpusSearchQuery::toUtf8(QStringLiteral("\U0001F600")), QByteArray("\xf0\x9f\x98\x80"));
        compareParsers(QStringLiteral("a") + QChar(u'\0') + QStringLiteral("b"));
        compareParsers(QString(QChar(0xD800)) + QStringLiteral("x"));
        compareParsers(QStringLiteral("name:\"") + QChar(0xDC00) + QStringLiteral("\""));
    }

    void reportsBoundaryErrors()
    {
        uint8_t *out = reinterpret_cast<uint8_t *>(this);
        size_t outLength = 1;
        const QByteArray invalid("name:\xff", 6);
        QCOMPARE(opus_query_check(reinterpret_cast<const uint8_t *>(invalid.constData()), size_t(invalid.size()), &out, &outLength), OPUS_QUERY_BAD_ENCODING);
        QCOMPARE(out, nullptr);
        QCOMPARE(outLength, size_t(0));
        QCOMPARE(opus_query_check(nullptr, 1, &out, &outLength), OPUS_QUERY_BAD_ARGUMENT);
        QCOMPARE(opus_query_check(reinterpret_cast<const uint8_t *>("a"), 1, nullptr, &outLength), OPUS_QUERY_BAD_ARGUMENT);
        QCOMPARE(opus_query_check(reinterpret_cast<const uint8_t *>("a"), 1, &out, nullptr), OPUS_QUERY_BAD_ARGUMENT);
        QCOMPARE(opus_query_check(nullptr, 0, &out, &outLength), OPUS_QUERY_INVALID);
        QVERIFY(out);
        opus_query_free(out, outLength);
        opus_query_free(nullptr, 0);
    }

    void documentsKnownDivergences()
    {
        // Rust has no MIME database, so it accepts any well-formed MIME type.
        QVERIFY(!Query::parse(QStringLiteral("type:nothing/here")).isValid());
        QCOMPARE(OpusSearchQuery::check(QStringLiteral("type:nothing/here")).status, OPUS_QUERY_VALID);
        // Rust's Alphabetic property includes letter numbers such as U+216B, which QChar::isLetter() does not,
        // so Rust reads a field name where C++ reads a plain word.
        const QString romanField = QString(QChar(0x216B)) + QStringLiteral(":x");
        QVERIFY(Query::parse(romanField).isValid());
        QCOMPARE(OpusSearchQuery::check(romanField).status, OPUS_QUERY_INVALID);
    }

    void isThreadSafe()
    {
        const QStringList queries = generatedCorpus(2000);
        QStringList sequential;
        for (const QString &text : queries) {
            sequential.append(rustResult(text));
        }
        const QStringList parallel = QtConcurrent::blockingMapped<QStringList>(queries, rustResult);
        QCOMPARE(parallel, sequential);
    }
};

QTEST_GUILESS_MAIN(RustSearchQueryTest)

#include "rustsearchquerytest.moc"
