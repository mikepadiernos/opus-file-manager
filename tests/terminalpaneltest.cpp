// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalpanel.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KParts/ReadOnlyPart>
#include <kde_terminal_interface.h>

#include <QApplication>
#include <QLabel>
#include <QPointer>
#include <QSplitter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TerminalPanelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void missingPluginIsReported()
    {
        const QStringList paths = QCoreApplication::libraryPaths();
        QTemporaryDir empty;
        QVERIFY(empty.isValid());
        QCoreApplication::setLibraryPaths({empty.path()});
        TerminalPanel panel;
        panel.start(QUrl::fromLocalFile(empty.path()));
        const QString status = panel.findChild<QLabel *>(QStringLiteral("terminalPanelStatus"))->text();
        QCoreApplication::setLibraryPaths(paths);
        QVERIFY(status.contains(QStringLiteral("not installed")));
    }

    void terminalRetainsBothPanesAndMovesWithLayout()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path()), QUrl::fromLocalFile(folder.path())});
        window.show();
        auto *dual = window.actionCollection()->action(QStringLiteral("dual_layout"));
        auto *preview = window.actionCollection()->action(QStringLiteral("show_preview_panel"));
        auto *showTerminal = window.actionCollection()->action(QStringLiteral("show_embedded_terminal"));
        QVERIFY(dual && preview && showTerminal);
        dual->setChecked(true);
        preview->setChecked(true);
        showTerminal->setChecked(true);
        auto *panel = window.findChild<TerminalPanel *>(QStringLiteral("terminalPanel"));
        auto *workspace = window.findChild<QSplitter *>(QStringLiteral("workspaceSplitter"));
        QVERIFY(panel && workspace);
        QVERIFY(!panel->isHidden());
        QVERIFY(!window.activePane()->isHidden());
        QVERIFY(!window.inactivePane()->isHidden());
        QCOMPARE(workspace->orientation(), Qt::Vertical);
        QCOMPARE(workspace->widget(0)->objectName(), QStringLiteral("paneSplitter"));
        QCOMPARE(workspace->widget(1), panel);

        auto *part = panel->findChild<KParts::ReadOnlyPart *>();
        if (!part) {
            QSKIP("KonsolePart is not available in this environment");
        }
        auto *terminal = qobject_cast<TerminalInterface *>(part);
        QVERIFY(terminal);
        QTRY_VERIFY(terminal->terminalProcessId() > 0);
        QTRY_COMPARE(terminal->currentWorkingDirectory(), folder.path());
        QTemporaryDir otherFolder;
        QVERIFY(otherFolder.isValid());
        window.activePane()->setUrl(QUrl::fromLocalFile(otherFolder.path()));
        QCOMPARE(terminal->currentWorkingDirectory(), folder.path());
        window.setStackedLayout(true);
        QCOMPARE(workspace->orientation(), Qt::Horizontal);
        QVERIFY(!window.activePane()->isHidden());
        QVERIFY(!window.inactivePane()->isHidden());
        showTerminal->setChecked(false);
        QVERIFY(panel->isHidden());
        showTerminal->setChecked(true);
        QCOMPARE(panel->findChild<KParts::ReadOnlyPart *>(), part);
        const QPointer<KParts::ReadOnlyPart> session(part);
        terminal->sendInput(QStringLiteral("exit\n"));
        QTRY_VERIFY(session.isNull());
        QVERIFY(panel->findChild<QLabel *>(QStringLiteral("terminalPanelStatus"))->text().contains(QStringLiteral("session ended")));
        showTerminal->setChecked(false);
        showTerminal->setChecked(true);
        auto *restarted = panel->findChild<KParts::ReadOnlyPart *>();
        QVERIFY(restarted);
        auto *newTerminal = qobject_cast<TerminalInterface *>(restarted);
        QVERIFY(newTerminal);
        QTRY_VERIFY(newTerminal->terminalProcessId() > 0);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid()) {
        return 1;
    }
    const QByteArray path = home.path().toLocal8Bit();
    qputenv("HOME", path);
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toLocal8Bit());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toLocal8Bit());
    qputenv("XDG_CACHE_HOME", home.filePath(QStringLiteral("cache")).toLocal8Bit());
    qputenv("XDG_STATE_HOME", home.filePath(QStringLiteral("state")).toLocal8Bit());
    qputenv("SHELL", "/bin/sh");
    qunsetenv("ENV");
    qunsetenv("BASH_ENV");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    TerminalPanelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "terminalpaneltest.moc"
