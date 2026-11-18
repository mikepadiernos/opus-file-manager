// SPDX-License-Identifier: GPL-3.0-or-later

#include "symlinks.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class SymlinksTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resolvesLinkedFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("real/sub")));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("real")), dir.filePath(QStringLiteral("link"))));

        const QUrl resolved = Symlinks::resolve(QUrl::fromLocalFile(dir.filePath(QStringLiteral("link/sub"))));
        QCOMPARE(resolved, QUrl::fromLocalFile(QDir(dir.filePath(QStringLiteral("real/sub"))).canonicalPath()));
    }

    void leavesOtherUrlsAlone()
    {
        const QUrl remote(QStringLiteral("sftp://host/link"));
        QCOMPARE(Symlinks::resolve(remote), remote);
        const QUrl missing = QUrl::fromLocalFile(QStringLiteral("/no/such/folder"));
        QCOMPARE(Symlinks::resolve(missing), missing);
    }
};

QTEST_GUILESS_MAIN(SymlinksTest)

#include "symlinkstest.moc"
