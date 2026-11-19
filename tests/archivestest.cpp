// SPDX-License-Identifier: GPL-3.0-or-later

#include "archives.h"

#include <QTest>

class ArchivesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void browseUrlUsesTheArchiveWorker()
    {
        const QUrl url = Archives::browseUrl(QUrl::fromLocalFile(QStringLiteral("/data/photos.zip")), QStringLiteral("application/zip"));
        // Whichever worker the system registers for zip, such as zip or krarc.
        QVERIFY(!url.scheme().isEmpty());
        QVERIFY(url.scheme() != QStringLiteral("file"));
        QCOMPARE(url.path(), QStringLiteral("/data/photos.zip/"));
    }

    void browseUrlIsEmptyForOtherFiles()
    {
        QVERIFY(Archives::browseUrl(QUrl::fromLocalFile(QStringLiteral("/data/notes.txt")), QStringLiteral("text/plain")).isEmpty());
        QVERIFY(Archives::browseUrl(QUrl(QStringLiteral("sftp://host/a.zip")), QStringLiteral("application/zip")).isEmpty());
    }

    void baseNameDropsArchiveExtensions()
    {
        QCOMPARE(Archives::baseName(QStringLiteral("photos.tar.gz")), QStringLiteral("photos"));
        QCOMPARE(Archives::baseName(QStringLiteral("backup.2026.zip")), QStringLiteral("backup.2026"));
        QCOMPARE(Archives::baseName(QStringLiteral("README")), QStringLiteral("README"));
    }
};

QTEST_GUILESS_MAIN(ArchivesTest)

#include "archivestest.moc"
