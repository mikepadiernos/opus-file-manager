// SPDX-License-Identifier: GPL-3.0-or-later

#include "filemanagerservice.h"
#include "filemanagerrequest.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"

#include <KPropertiesDialog>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>

class FileManagerServiceTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;

    QString path(const QString &relative) const
    {
        return m_root.filePath(relative);
    }

    QString uri(const QString &relative) const
    {
        return QUrl::fromLocalFile(path(relative)).toString();
    }

    static QStringList selectedNames(FolderView *view)
    {
        QStringList names;
        const KFileItemList items = view->selectedItems();
        for (const KFileItem &item : items) {
            names.append(item.name());
        }
        names.sort();
        return names;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("one")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("two")));
        for (const QString &name : {QStringLiteral("one/a.txt"), QStringLiteral("one/b.txt"), QStringLiteral("one/c.txt"), QStringLiteral("two/d.txt")}) {
            QFile file(path(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("x");
        }
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
    }

    void showFoldersOpensTabs()
    {
        MainWindow window;
        auto *service = new FileManagerService(&window);
        QSignalSpy handled(service, &FileManagerService::requestHandled);
        Pane *pane = window.activePane();

        service->ShowFolders({uri(QStringLiteral("one")), path(QStringLiteral("two"))}, QString());
        QCOMPARE(handled.size(), 1);
        QVERIFY(window.isVisible());
        QTRY_COMPARE(pane->url().adjusted(QUrl::StripTrailingSlash), QUrl::fromLocalFile(path(QStringLiteral("two"))));
        QCOMPARE(window.activePane(), pane);
    }

    void showItemsSelectsItemsPerFolder()
    {
        MainWindow window;
        auto *service = new FileManagerService(&window);
        Pane *pane = window.activePane();

        service->ShowItems({uri(QStringLiteral("two/d.txt")), uri(QStringLiteral("one/a.txt")), uri(QStringLiteral("one/c.txt"))}, QString());
        // One tab per folder; the last folder's tab is current.
        QTRY_COMPARE(pane->url().adjusted(QUrl::StripTrailingSlash), QUrl::fromLocalFile(path(QStringLiteral("one"))));
        QTRY_COMPARE(selectedNames(pane->view()), QStringList({QStringLiteral("a.txt"), QStringLiteral("c.txt")}));
    }

    void showItemsReportsMissingItems()
    {
        MainWindow window;
        auto *service = new FileManagerService(&window);
        Pane *pane = window.activePane();
        QSignalSpy status(pane, &Pane::statusMessage);

        service->ShowItems({uri(QStringLiteral("one/missing.txt")), uri(QStringLiteral("one/b.txt"))}, QString());
        QTRY_COMPARE(selectedNames(pane->view()), QStringList({QStringLiteral("b.txt")}));
        bool reported = false;
        for (const QList<QVariant> &arguments : std::as_const(status)) {
            reported |= arguments.first().toString().contains(QStringLiteral("missing.txt"));
        }
        QVERIFY(reported);
    }

    void invalidUrisAreIgnoredVisibly()
    {
        MainWindow window;
        auto *service = new FileManagerService(&window);
        service->ShowFolders({QStringLiteral("relative/folder")}, QString());
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("relative/folder")));
    }

    void showItemPropertiesOpensDialog()
    {
        MainWindow window;
        auto *service = new FileManagerService(&window);
        service->ShowItemProperties({uri(QStringLiteral("one/missing.txt"))}, QString());
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("missing.txt")));
        QVERIFY(window.findChildren<KPropertiesDialog *>().isEmpty());

        service->ShowItemProperties({uri(QStringLiteral("one/a.txt"))}, QString());
        QTRY_COMPARE(window.findChildren<KPropertiesDialog *>().size(), 1);
        window.findChildren<KPropertiesDialog *>().first()->reject();
    }

    void exportsTheInterfaceOnTheBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("No session bus.");
        }
        MainWindow window;
        auto *service = new FileManagerService(&window);
        // Never claim the well-known name in tests; call the object through
        // this connection's unique name instead.
        QVERIFY(!service->registerOnBus(false));
        QDBusInterface remote(bus.baseService(), FileManagerRequest::objectPath(), FileManagerRequest::serviceName(), bus);
        QVERIFY(remote.isValid());
        const QDBusMessage reply = remote.call(QStringLiteral("ShowFolders"), QStringList({uri(QStringLiteral("two"))}), QString());
        QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
        QTRY_COMPARE(window.activePane()->url().adjusted(QUrl::StripTrailingSlash), QUrl::fromLocalFile(path(QStringLiteral("two"))));
        bus.unregisterObject(FileManagerRequest::objectPath());
    }
};

QTEST_MAIN(FileManagerServiceTest)

#include "filemanagerservicetest.moc"
