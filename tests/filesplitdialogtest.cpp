// SPDX-License-Identifier: GPL-3.0-or-later

#include "filesplitdialog.h"

#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace
{
bool put(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class FileSplitDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void splitAndCombineInChosenOrder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = dir.filePath(QStringLiteral("source"));
        QVERIFY(put(source, "abcdef"));
        FileSplitDialog split(FileSplitDialog::Mode::Split, {source});
        split.findChild<QLineEdit *>(QStringLiteral("fileSplitPartSize"))->setText(QStringLiteral("3"));
        split.findChild<QPushButton *>(QStringLiteral("fileSplitStart"))->click();
        auto *splitStatus = split.findChild<QLabel *>(QStringLiteral("fileSplitStatus"));
        QTRY_COMPARE(splitStatus->text(), QStringLiteral("Operation finished."));
        auto *outputs = split.findChild<QListWidget *>(QStringLiteral("fileSplitOutputs"));
        QCOMPARE(outputs->count(), 2);

        FileSplitDialog combine(FileSplitDialog::Mode::Combine, {outputs->item(1)->text(), outputs->item(0)->text()});
        auto *inputs = combine.findChild<QListWidget *>(QStringLiteral("fileSplitInputs"));
        QCOMPARE(inputs->item(0)->text(), source + QStringLiteral(".001"));
        inputs->setCurrentRow(1);
        combine.findChild<QPushButton *>(QStringLiteral("fileSplitUp"))->click();
        QCOMPARE(inputs->item(0)->text(), source + QStringLiteral(".002"));
        const QString destination = dir.filePath(QStringLiteral("joined"));
        combine.findChild<QLineEdit *>(QStringLiteral("fileSplitDestination"))->setText(destination);
        combine.findChild<QPushButton *>(QStringLiteral("fileSplitStart"))->click();
        QTRY_COMPARE(combine.findChild<QLabel *>(QStringLiteral("fileSplitStatus"))->text(), QStringLiteral("Operation finished."));
        QCOMPARE(read(destination), QByteArray("defabc"));
    }

    void invalidAndExistingDestinationsAreReported()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = dir.filePath(QStringLiteral("source"));
        QVERIFY(put(source, "abc"));
        FileSplitDialog split(FileSplitDialog::Mode::Split, {source});
        auto *start = split.findChild<QPushButton *>(QStringLiteral("fileSplitStart"));
        auto *status = split.findChild<QLabel *>(QStringLiteral("fileSplitStatus"));
        split.findChild<QLineEdit *>(QStringLiteral("fileSplitPartSize"))->setText(QStringLiteral("0"));
        start->click();
        QVERIFY(status->text().contains(QStringLiteral("positive part size")));
        QVERIFY(put(source + QStringLiteral(".002"), "keep"));
        split.findChild<QLineEdit *>(QStringLiteral("fileSplitPartSize"))->setText(QStringLiteral("2"));
        start->click();
        QTRY_VERIFY(start->isEnabled());
        QVERIFY(status->text().contains(QStringLiteral("Could not create")));
        auto *outputs = split.findChild<QListWidget *>(QStringLiteral("fileSplitOutputs"));
        QCOMPARE(outputs->count(), 1);
        QCOMPARE(outputs->item(0)->text(), source + QStringLiteral(".001"));
        QCOMPARE(read(source + QStringLiteral(".001")), QByteArray("ab"));
        QCOMPARE(read(source + QStringLiteral(".002")), QByteArray("keep"));
    }
};

QTEST_MAIN(FileSplitDialogTest)
#include "filesplitdialogtest.moc"
