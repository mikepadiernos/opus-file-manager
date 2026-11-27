// SPDX-License-Identifier: GPL-3.0-or-later

#include "mailmessage.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace MailMessage;

namespace
{
QByteArray crlf(QByteArray text)
{
    return text.replace("\n", "\r\n");
}

const QByteArray Mixed = crlf(
    "Return-Path: <alice@example.org>\n"
    "From: =?UTF-8?B?w4FsaWNl?= <alice@example.org>\n"
    "To: Bob <bob@example.org>,\n"
    " Carol <carol@example.org>\n"
    "Subject: =?ISO-8859-1?Q?Caf=E9_report?= =?UTF-8?Q?_=E2=9C=93?=\n"
    "Date: Tue, 24 Nov 2026 10:00:00 +0000\n"
    "MIME-Version: 1.0\n"
    "Content-Type: multipart/mixed; boundary=\"outer\"\n"
    "\n"
    "This is the preamble.\n"
    "--outer\n"
    "Content-Type: multipart/alternative; boundary=inner\n"
    "\n"
    "--inner\n"
    "Content-Type: text/plain; charset=utf-8\n"
    "Content-Transfer-Encoding: quoted-printable\n"
    "\n"
    "Hello Bob,=0A=\n"
    "the caf=C3=A9 report is attached.\n"
    "--inner\n"
    "Content-Type: text/html; charset=utf-8\n"
    "\n"
    "<p>Hello <b>Bob</b></p>\n"
    "--inner--\n"
    "--outer\n"
    "Content-Type: application/pdf; name=\"report.pdf\"\n"
    "Content-Disposition: attachment; filename=\"report.pdf\"\n"
    "Content-Transfer-Encoding: base64\n"
    "\n"
    "JVBERi0xLjQK\n"
    "--outer\n"
    "Content-Type: text/plain\n"
    "Content-Disposition: attachment;\n"
    " filename*0*=utf-8''%E2%82%AC%20;\n"
    " filename*1=\"costs.txt\"\n"
    "\n"
    "42\n"
    "--outer\n"
    "Content-Type: application/octet-stream\n"
    "Content-Disposition: attachment; filename=\"../../.bashrc\"\n"
    "\n"
    "evil\n"
    "--outer\n"
    "Content-Type: application/pdf\n"
    "Content-Disposition: attachment; filename=\"report.pdf\"\n"
    "\n"
    "second\n"
    "--outer--\n"
    "Epilogue.\n");
}

class MailMessageTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesHeadersBodyAndAttachments()
    {
        const Message message = parse(Mixed);
        QCOMPARE(message.headers.size(), 4);
        QCOMPARE(message.headers.at(0).name, QStringLiteral("From"));
        QCOMPARE(message.headers.at(0).value, QStringLiteral("Álice <alice@example.org>"));
        QCOMPARE(message.headers.at(1).value, QStringLiteral("Bob <bob@example.org>, Carol <carol@example.org>"));
        QCOMPARE(message.headers.at(2).name, QStringLiteral("Date"));
        QCOMPARE(message.subject, QStringLiteral("Café report ✓"));
        QCOMPARE(message.body, QStringLiteral("Hello Bob,\nthe café report is attached."));
        QVERIFY(!message.bodyFromHtml);
        QVERIFY(!message.encrypted);
        QVERIFY(!message.signedMessage);

        QCOMPARE(message.attachments.size(), 4);
        QCOMPARE(message.attachments.at(0).fileName, QStringLiteral("report.pdf"));
        QCOMPARE(message.attachments.at(0).mimeType, QStringLiteral("application/pdf"));
        QCOMPARE(message.attachments.at(0).size, 9);
        QCOMPARE(message.attachments.at(1).fileName, QStringLiteral("€ costs.txt"));
        // Paths and leading dots are neutralized.
        QCOMPARE(message.attachments.at(2).fileName, QStringLiteral("_.._.bashrc"));
        // Duplicate names are made unique.
        QCOMPARE(message.attachments.at(3).fileName, QStringLiteral("report (2).pdf"));
    }

    void extractsAttachmentData()
    {
        QCOMPARE(attachmentData(Mixed, 0, QStringLiteral("report.pdf")), std::optional<QByteArray>("%PDF-1.4\n"));
        QCOMPARE(attachmentData(Mixed, 1, QStringLiteral("€ costs.txt")), std::optional<QByteArray>("42"));
        QCOMPARE(attachmentData(Mixed, 3, QStringLiteral("report (2).pdf")), std::optional<QByteArray>("second"));
        // A changed message, or a stale index, extracts nothing.
        QVERIFY(!attachmentData(Mixed, 0, QStringLiteral("other.pdf")));
        QVERIFY(!attachmentData(Mixed, 9, QStringLiteral("report.pdf")));
    }

    void reducesHtmlOnlyBodiesToText()
    {
        const Message message =
            parse(crlf("From: a@example.org\n"
                       "Content-Type: text/html; charset=utf-8\n"
                       "\n"
                       "<html><head><title>T</title><style>p{}</style></head><body>"
                       "<script>alert(1)</script><p>One &amp; two</p><img src=\"https://tracker.example/p.gif\">"
                       "<ul><li>a</li><li>b</li></ul>&#x263A;&nbsp;<!-- hidden --></body></html>\n"));
        QVERIFY(message.bodyFromHtml);
        QCOMPARE(message.body, QStringLiteral("One & two\n\n• a\n• b\n☺"));
        QVERIFY(!message.body.contains(QStringLiteral("alert")));
        QVERIFY(!message.body.contains(QStringLiteral("tracker")));
    }

    void reportsEncryptedAndSignedMessages()
    {
        const Message encrypted =
            parse(crlf("Content-Type: multipart/encrypted; protocol=\"application/pgp-encrypted\"; boundary=b\n"
                       "\n"
                       "--b\n"
                       "Content-Type: application/pgp-encrypted\n"
                       "\n"
                       "Version: 1\n"
                       "--b\n"
                       "Content-Type: application/octet-stream; name=encrypted.asc\n"
                       "\n"
                       "-----BEGIN PGP MESSAGE-----\n"
                       "--b--\n"));
        QVERIFY(encrypted.encrypted);
        QVERIFY(encrypted.body.isEmpty());
        QVERIFY(encrypted.attachments.isEmpty());

        const Message smime = parse(crlf("Content-Type: application/pkcs7-mime; smime-type=enveloped-data; name=smime.p7m\n\nAAAA\n"));
        QVERIFY(smime.encrypted);
        QVERIFY(smime.attachments.isEmpty());

        const Message signedMessage =
            parse(crlf("Content-Type: multipart/signed; protocol=\"application/pgp-signature\"; boundary=s\n"
                       "\n"
                       "--s\n"
                       "Content-Type: text/plain\n"
                       "\n"
                       "Signed text\n"
                       "--s\n"
                       "Content-Type: application/pgp-signature; name=signature.asc\n"
                       "\n"
                       "-----BEGIN PGP SIGNATURE-----\n"
                       "--s--\n"));
        QVERIFY(signedMessage.signedMessage);
        QCOMPARE(signedMessage.body, QStringLiteral("Signed text"));
        QVERIFY(signedMessage.attachments.isEmpty());

        QVERIFY(parse("Subject: x\n\n-----BEGIN PGP MESSAGE-----\nabc\n").encrypted);
    }

    void listsForwardedMessages()
    {
        const Message message =
            parse(crlf("Content-Type: multipart/mixed; boundary=m\n"
                       "\n"
                       "--m\n"
                       "Content-Type: text/plain\n"
                       "\n"
                       "See below.\n"
                       "--m\n"
                       "Content-Type: message/rfc822\n"
                       "\n"
                       "Subject: Original/notes\n"
                       "\n"
                       "Inner body\n"
                       "--m--\n"));
        QCOMPARE(message.body, QStringLiteral("See below."));
        QCOMPARE(message.attachments.size(), 1);
        QCOMPARE(message.attachments.first().fileName, QStringLiteral("Original_notes.eml"));
        QCOMPARE(message.attachments.first().mimeType, QStringLiteral("message/rfc822"));
    }

    void handlesPlainAndMalformedMessages()
    {
        const Message plain = parse("From: a@example.org\nSubject: Hi\n\nLine 1\nLine 2\n");
        QCOMPARE(plain.subject, QStringLiteral("Hi"));
        QCOMPARE(plain.body, QStringLiteral("Line 1\nLine 2\n"));

        const Message latin1 = parse("Subject: caf\xe9\n\ncaf\xe9\n");
        QCOMPARE(latin1.subject, QStringLiteral("café"));
        QCOMPARE(latin1.body, QStringLiteral("café\n"));

        // A truncated message keeps its last part.
        const Message unterminated = parse("Content-Type: multipart/mixed; boundary=x\n\n--x\nContent-Type: text/plain\n\nno end\n");
        QCOMPARE(unterminated.body, QStringLiteral("no end\n"));
        QVERIFY(unterminated.attachments.isEmpty());

        QVERIFY(parse(QByteArray()).headers.isEmpty());

        QByteArray deep = "Content-Type: multipart/mixed; boundary=b0\n\n";
        for (int i = 1; i < 40; ++i) {
            deep += "--b" + QByteArray::number(i - 1) + "\nContent-Type: multipart/mixed; boundary=b" + QByteArray::number(i) + "\n\n";
        }
        parse(deep);

        const Message truncated = parse("Subject: x\n\n" + QByteArray(100, 'a'), 10);
        QCOMPARE(truncated.body.size(), 10);
        QVERIFY(truncated.bodyTruncated);
    }

    void loadsFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("message.eml"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(Mixed);
        file.close();
        QString error;
        const auto message = load(path, 1024, &error);
        QVERIFY(message);
        QCOMPARE(message->attachments.size(), 4);
        QVERIFY(!load(dir.filePath(QStringLiteral("missing.eml")), 1024, &error));
        QVERIFY(!error.isEmpty());
    }

    void decodesHeadersAndNames()
    {
        QCOMPARE(decodeHeader("=?utf-8?q?a?= =?utf-8?q?b?= c"), QStringLiteral("ab c"));
        QCOMPARE(decodeHeader("plain =?bogus-charset?B?aGk=?="), QStringLiteral("plain hi"));
        QCOMPARE(decodeHeader("=?UTF-8*en?Q?hi?="), QStringLiteral("hi"));
        QCOMPARE(safeFileName(QStringLiteral("..."), QStringLiteral("fallback.bin")), QStringLiteral("fallback.bin"));
        QCOMPARE(safeFileName(QStringLiteral("a\tb\u202E.txt"), QStringLiteral("x")), QStringLiteral("a_b_.txt"));
        const QString longName = QString(300, QLatin1Char('n')) + QStringLiteral(".pdf");
        QCOMPARE(safeFileName(longName, QStringLiteral("x")).size(), 200);
        QVERIFY(safeFileName(longName, QStringLiteral("x")).endsWith(QStringLiteral(".pdf")));
    }
};

QTEST_GUILESS_MAIN(MailMessageTest)
#include "mailmessagetest.moc"
