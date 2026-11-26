// SPDX-License-Identifier: GPL-3.0-or-later

#include "kdeconnectshare.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace KdeConnectShare;

class KdeConnectShareTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void classifiesLocalFilesAndRejectsTheRest()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("photo.jpg"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("x");
        out.close();
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("folder")));
        const QString link = dir.filePath(QStringLiteral("link.jpg"));
        QVERIFY(QFile::link(file, link));

        const Selection selection = classify({
            QUrl::fromLocalFile(file),
            QUrl::fromLocalFile(dir.filePath(QStringLiteral("folder"))),
            QUrl(QStringLiteral("sftp://host/file.txt")),
            QUrl::fromLocalFile(dir.filePath(QStringLiteral("gone.txt"))),
            QUrl::fromLocalFile(file),
            QUrl::fromLocalFile(link),
        });
        QCOMPARE(selection.files, (QList<QUrl>{QUrl::fromLocalFile(file), QUrl::fromLocalFile(link)}));
        QCOMPARE(selection.rejected.size(), 3);
        QCOMPARE(selection.rejected.at(0).reason, Reason::Folder);
        QCOMPARE(selection.rejected.at(1).reason, Reason::Remote);
        QCOMPARE(selection.rejected.at(2).reason, Reason::Missing);
        for (const Rejected &rejected : selection.rejected) {
            QVERIFY(!reasonText(rejected.reason).isEmpty());
        }
    }

    void rejectsSpecialFiles()
    {
#ifdef Q_OS_LINUX
        const Selection selection = classify({QUrl::fromLocalFile(QStringLiteral("/dev/null"))});
        QVERIFY(selection.files.isEmpty());
        QCOMPARE(selection.rejected.size(), 1);
        QCOMPARE(selection.rejected.first().reason, Reason::NotRegularFile);
#endif
    }

    void eligibleDevicesArePairedReachableAndShareable()
    {
        const Device phone{QStringLiteral("a"), QStringLiteral("Phone"), true, true, true};
        const Device tablet{QStringLiteral("b"), QStringLiteral("Tablet"), false, true, false};
        const Device laptop{QStringLiteral("c"), QStringLiteral("Laptop"), true, true, false};
        const Device stranger{QStringLiteral("d"), QStringLiteral("Stranger"), true, false, true};
        const Device anotherPhone{QStringLiteral("e"), QStringLiteral("Another Phone"), true, true, true};

        QCOMPARE(eligible({phone, tablet, laptop, stranger, anotherPhone}), (QList<Device>{anotherPhone, phone}));
        QVERIFY(unavailableReason(phone).isEmpty());
        QVERIFY(!unavailableReason(tablet).isEmpty());
        QVERIFY(!unavailableReason(laptop).isEmpty());
        QVERIFY(!unavailableReason(stranger).isEmpty());
        QVERIFY(unavailableReason(tablet) != unavailableReason(laptop));
    }
};

QTEST_GUILESS_MAIN(KdeConnectShareTest)
#include "kdeconnectsharetest.moc"
