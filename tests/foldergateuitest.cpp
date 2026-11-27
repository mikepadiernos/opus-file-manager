// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldergates.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KMessageWidget>
#include <KSharedConfig>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class FolderGateUiTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_root;
    QString m_secret;

    static void writeFile(const QString &path)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("text\n");
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("FolderGates"));
        KSharedConfig::openConfig()->sync();
        QVERIFY(m_dir.isValid());
        m_root = QDir(m_dir.path()).canonicalPath();
        m_secret = m_root + QStringLiteral("/secret");
        QVERIFY(QDir().mkpath(m_secret));
        writeFile(m_root + QStringLiteral("/open.txt"));
        writeFile(m_secret + QStringLiteral("/inner.txt"));
        FolderGates::instance()->setIterations(10);
        QVERIFY(FolderGates::instance()->protect(m_secret, QStringLiteral("pw")).isEmpty());
    }

    void cleanupTestCase()
    {
        FolderGates::instance()->removeProtection(m_secret, QStringLiteral("pw"));
    }

    void lockedFolderIsNotListed()
    {
        MainWindow window;
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        auto *bar = pane->findChild<KMessageWidget *>(QStringLiteral("gateBar"));
        QVERIFY(bar);
        KActionCollection *actions = window.actionCollection();

        pane->setUrl(QUrl::fromLocalFile(m_root));
        QTRY_COMPARE(view->itemCount(), 2);
        QVERIFY(bar->isHidden());
        QVERIFY(!view->isGateLocked());

        pane->setUrl(QUrl::fromLocalFile(m_secret));
        QVERIFY(view->isGateLocked());
        QVERIFY(!bar->isHidden());
        QVERIFY(bar->findChild<QAction *>(QStringLiteral("unlockGate")));
        QTest::qWait(200);
        QCOMPARE(view->itemCount(), 0);
        QVERIFY(view->items().isEmpty());
        QVERIFY(actions->action(QStringLiteral("unlock_folder"))->isEnabled());
        QVERIFY(!actions->action(QStringLiteral("protect_folder"))->isEnabled());

        QCOMPARE(FolderGates::instance()->unlock(m_secret, QStringLiteral("pw")), FolderGates::Check::Unlocked);
        QTRY_COMPARE(view->itemCount(), 1);
        QVERIFY(bar->isHidden());
        QVERIFY(!view->isGateLocked());
        QVERIFY(!actions->action(QStringLiteral("unlock_folder"))->isEnabled());
        QVERIFY(actions->action(QStringLiteral("lock_protected_folders"))->isEnabled());

        actions->action(QStringLiteral("lock_protected_folders"))->trigger();
        QVERIFY(FolderGates::instance()->isGateLocked(m_secret));
        QTRY_COMPARE(view->itemCount(), 0);
        QVERIFY(!bar->isHidden());
    }

    void subfoldersOfLockedFoldersAreBlocked()
    {
        QVERIFY(QDir().mkpath(m_secret + QStringLiteral("/deeper")));
        MainWindow window;
        Pane *pane = window.activePane();
        auto *bar = pane->findChild<KMessageWidget *>(QStringLiteral("gateBar"));
        pane->setUrl(QUrl::fromLocalFile(m_secret + QStringLiteral("/deeper")));
        QVERIFY(pane->view()->isGateLocked());
        QVERIFY(!bar->isHidden());
        QVERIFY(bar->text().contains(QStringLiteral("inside")));
    }
};

QTEST_MAIN(FolderGateUiTest)

#include "foldergateuitest.moc"
