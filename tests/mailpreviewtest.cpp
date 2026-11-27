// SPDX-License-Identifier: GPL-3.0-or-later

#include "mailpreview.h"
#include "mailmessage.h"
#include "previewpanel.h"

#include <KFileItem>

#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

namespace
{
const QByteArray MessageData =
    "From: Ada <ada@example.org>\r\n"
    "To: Grace <grace@example.org>\r\n"
    "Date: Thu, 26 Nov 2026 10:00:00 +0000\r\n"
    "Subject: =?UTF-8?Q?Quarterly_notes_=E2=80=93_draft?=\r\n"
    "MIME-Version: 1.0\r\n"
    "Content-Type: multipart/mixed; boundary=\"b\"\r\n"
    "\r\n"
    "--b\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "\r\n"
    "Hello Grace,\r\nthe notes are attached.\r\n"
    "--b\r\n"
    "Content-Type: text/plain; name=\"notes.txt\"\r\n"
    "Content-Disposition: attachment; filename=\"notes.txt\"\r\n"
    "Content-Transfer-Encoding: base64\r\n"
    "\r\n"
    "Tm90ZXMgZm9yIHRoZSBxdWFydGVyLgo=\r\n"
    "--b--\r\n";

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}
}

class MailPreviewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_folder.isValid());
    }

    void previewPanelShowsMailFiles()
    {
        const QString path = m_folder.filePath(QStringLiteral("notes.eml"));
        QVERIFY(writeFile(path, MessageData));

        PreviewPanel panel;
        auto *content = panel.findChild<QStackedWidget *>();
        QVERIFY(content);
        panel.setItems({KFileItem(QUrl::fromLocalFile(path))});
        QTRY_VERIFY(content->currentWidget() && content->currentWidget()->objectName() == QLatin1String("previewMail"));
        QVERIFY(!panel.isBusy());

        auto *preview = qobject_cast<MailPreview *>(content->currentWidget());
        QVERIFY(preview);
        QCOMPARE(preview->source(), QUrl::fromLocalFile(path));
        auto *headers = preview->findChild<QFormLayout *>(QStringLiteral("mailPreviewHeaders"));
        QVERIFY(headers);
        QCOMPARE(headers->rowCount(), 4);
        QStringList values;
        for (int row = 0; row < headers->rowCount(); ++row) {
            auto *field = qobject_cast<QLabel *>(headers->itemAt(row, QFormLayout::FieldRole)->widget());
            QVERIFY(field);
            values << field->text();
        }
        QVERIFY(values.contains(QStringLiteral("Ada <ada@example.org>")));
        QVERIFY(values.contains(QStringLiteral("Quarterly notes – draft")));

        auto *body = preview->findChild<QPlainTextEdit *>(QStringLiteral("mailPreviewBody"));
        QVERIFY(body);
        QVERIFY2(body->toPlainText().startsWith(QStringLiteral("Hello Grace,")), qPrintable(body->toPlainText()));
        QVERIFY(body->isReadOnly());

        auto *attachments = preview->findChild<QListWidget *>(QStringLiteral("mailPreviewAttachments"));
        QVERIFY(attachments);
        QCOMPARE(attachments->count(), 1);
        QCOMPARE(attachments->item(0)->toolTip(), QStringLiteral("notes.txt"));
        auto *extract = preview->findChild<QToolButton *>(QStringLiteral("mailPreviewExtract"));
        QVERIFY(extract);
        QVERIFY(extract->isEnabled());
        QCOMPARE(extract->text(), QStringLiteral("Extract All"));
        attachments->item(0)->setSelected(true);
        QCOMPARE(extract->text(), QStringLiteral("Extract Selected"));
    }

    void extractsAttachmentsBesideTheMessage()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("notes.eml"));
        QVERIFY(writeFile(path, MessageData));
        QString error;
        const auto message = MailMessage::load(path, 1 << 20, &error);
        QVERIFY2(message, qPrintable(error));

        MailPreview preview;
        preview.open(*message, QUrl::fromLocalFile(path));
        QSignalSpy finished(&preview, &MailPreview::extractionFinished);
        preview.extract({});
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
        QVERIFY2(finished.first().first().toString().isEmpty(), qPrintable(finished.first().first().toString()));

        QFile extracted(folder.filePath(QStringLiteral("notes.txt")));
        QVERIFY(extracted.open(QIODevice::ReadOnly));
        QCOMPARE(extracted.readAll(), QByteArray("Notes for the quarter.\n"));
        auto *status = preview.findChild<QLabel *>(QStringLiteral("mailPreviewStatus"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QStringLiteral("Extracted one attachment")), qPrintable(status->text()));
    }

    void opensReadOnlyCopiesOfAttachments()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("notes.eml"));
        QVERIFY(writeFile(path, MessageData));
        QString error;
        const auto message = MailMessage::load(path, 1 << 20, &error);
        QVERIFY2(message, qPrintable(error));

        MailPreview preview;
        QList<QUrl> opened;
        preview.setOpener([&opened](const QUrl &file) {
            opened.append(file);
        });
        preview.open(*message, QUrl::fromLocalFile(path));
        auto *open = preview.findChild<QToolButton *>(QStringLiteral("mailPreviewOpen"));
        QVERIFY(open);
        QVERIFY(!open->isEnabled());
        auto *attachments = preview.findChild<QListWidget *>(QStringLiteral("mailPreviewAttachments"));
        attachments->item(0)->setSelected(true);
        QVERIFY(open->isEnabled());

        QSignalSpy finished(&preview, &MailPreview::openFinished);
        open->click();
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.first().first().toString().isEmpty(), qPrintable(finished.first().first().toString()));
        QCOMPARE(opened.size(), 1);
        const QString copy = opened.first().toLocalFile();
        QCOMPARE(QFileInfo(copy).fileName(), QStringLiteral("notes.txt"));
        QVERIFY(!copy.startsWith(folder.path()));
        QVERIFY(!(QFileInfo(copy).permissions() & QFileDevice::WriteOwner));
        QFile file(copy);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("Notes for the quarter.\n"));
        file.close();
        // Nothing is written beside the message.
        QVERIFY(!QFile::exists(folder.filePath(QStringLiteral("notes.txt"))));

        // Opening it again uses a separate copy.
        preview.openAttachment(0);
        QTRY_COMPARE(finished.count(), 2);
        QCOMPARE(opened.size(), 2);
        QVERIFY(opened.at(0) != opened.at(1));
    }

    void reportsMessagesThatChanged()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("notes.eml"));
        QVERIFY(writeFile(path, MessageData));
        QString error;
        const auto message = MailMessage::load(path, 1 << 20, &error);
        QVERIFY2(message, qPrintable(error));

        MailPreview preview;
        preview.open(*message, QUrl::fromLocalFile(path));
        QVERIFY(writeFile(path, "Subject: Replaced\r\n\r\nNo attachments now.\r\n"));
        QSignalSpy finished(&preview, &MailPreview::extractionFinished);
        preview.extract({});
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY2(finished.first().first().toString().contains(QStringLiteral("changed after it was previewed")),
                 qPrintable(finished.first().first().toString()));
        QVERIFY(!QFile::exists(folder.filePath(QStringLiteral("notes.txt"))));
    }

    void clearsThePreview()
    {
        MailPreview preview;
        preview.open(MailMessage::parse("Subject: Hi\n\nHello\n"), QUrl::fromLocalFile(m_folder.filePath(QStringLiteral("hi.eml"))));
        auto *extract = preview.findChild<QToolButton *>(QStringLiteral("mailPreviewExtract"));
        QVERIFY(extract);
        QVERIFY(extract->isHidden());
        preview.clear();
        QVERIFY(preview.source().isEmpty());
        QVERIFY(preview.findChild<QPlainTextEdit *>(QStringLiteral("mailPreviewBody"))->toPlainText().isEmpty());
    }

private:
    QTemporaryDir m_folder;
};

QTEST_MAIN(MailPreviewTest)

#include "mailpreviewtest.moc"
