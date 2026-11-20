// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalpanel.h"
#include "mainwindow.h"
#include "pane.h"
#include "settings.h"

#include <KActionCollection>
#include <KParts/ReadOnlyPart>
#include <kde_terminal_interface.h>

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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

    void configuredPlacementAppliesWithoutReplacingPanes()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Settings::setTerminalPlacement(Settings::EnumTerminalPlacement::Automatic);
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path()), QUrl::fromLocalFile(folder.path())});
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        window.actionCollection()->action(QStringLiteral("show_embedded_terminal"))->setChecked(true);
        auto *workspace = window.findChild<QSplitter *>(QStringLiteral("workspaceSplitter"));
        QVERIFY(workspace);
        window.setStackedLayout(false);
        QCOMPARE(workspace->orientation(), Qt::Vertical);
        window.setStackedLayout(true);
        QCOMPARE(workspace->orientation(), Qt::Horizontal);
        Settings::setTerminalPlacement(Settings::EnumTerminalPlacement::Below);
        window.applySettings();
        QCOMPARE(workspace->orientation(), Qt::Vertical);
        window.setStackedLayout(false);
        QCOMPARE(workspace->orientation(), Qt::Vertical);
        Settings::setTerminalPlacement(Settings::EnumTerminalPlacement::Right);
        window.applySettings();
        QCOMPARE(workspace->orientation(), Qt::Horizontal);
        window.setStackedLayout(true);
        QCOMPARE(workspace->orientation(), Qt::Horizontal);
        QVERIFY(!window.activePane()->isHidden());
        QVERIFY(!window.inactivePane()->isHidden());
        Settings::setTerminalPlacement(Settings::EnumTerminalPlacement::Automatic);
    }

    void followingPreservesSessionsAndReportsUnsupportedFolders()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QTemporaryDir third;
        QVERIFY(first.isValid() && second.isValid() && third.isValid());
        Settings::setTerminalFollowActiveFolder(false);
        const auto reset = qScopeGuard([] {
            Settings::setTerminalFollowActiveFolder(false);
        });
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(first.path()), QUrl::fromLocalFile(second.path())});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        auto *show = window.actionCollection()->action(QStringLiteral("show_embedded_terminal"));
        show->setChecked(true);
        auto *panel = window.findChild<TerminalPanel *>(QStringLiteral("terminalPanel"));
        auto *tabs = panel->findChild<QTabWidget *>(QStringLiteral("terminalSessions"));
        auto *original = panel->findChild<KParts::ReadOnlyPart *>();
        if (!original) {
            QSKIP("KonsolePart is not available in this environment");
        }
        auto *terminal = qobject_cast<TerminalInterface *>(original);
        QVERIFY(terminal);
        QTRY_VERIFY(terminal->terminalProcessId() > 0);
        const QString originalFolder = window.activePane()->url().toLocalFile();
        QTRY_COMPARE(terminal->currentWorkingDirectory(), originalFolder);
        const QString output = first.filePath(QStringLiteral("retained"));
        terminal->sendInput(QStringLiteral("printf retained > '%1'").arg(output));

        Settings::setTerminalFollowActiveFolder(true);
        window.applySettings();
        QCOMPARE(tabs->count(), 1);
        auto *other = window.inactivePane();
        Q_EMIT other->activated();
        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->currentWidget() != original->widget());
        auto *secondPart = panel->findChildren<KParts::ReadOnlyPart *>().last();
        auto *secondTerminal = qobject_cast<TerminalInterface *>(secondPart);
        QVERIFY(secondTerminal);
        QTRY_COMPARE(secondTerminal->currentWorkingDirectory(), other->url().toLocalFile());
        other->setUrl(QUrl::fromLocalFile(third.path()));
        QCOMPARE(tabs->count(), 3);
        Q_EMIT window.inactivePane()->activated();
        QCOMPARE(tabs->currentWidget(), original->widget());
        QCOMPARE(tabs->count(), 3);
        terminal->sendInput(QStringLiteral("\n"));
        QTRY_VERIFY(QFile::exists(output));
        const QString completed = first.filePath(QStringLiteral("completed"));
        const int originalPid = terminal->terminalProcessId();
        terminal->sendInput(QStringLiteral("sleep 1; printf completed > '%1'\n").arg(completed));
        Q_EMIT window.inactivePane()->activated();
        QTRY_VERIFY(QFile::exists(completed));
        QCOMPARE(terminal->terminalProcessId(), originalPid);
        Q_EMIT window.inactivePane()->activated();
        QCOMPARE(tabs->currentWidget(), original->widget());

        panel->follow(QUrl(QStringLiteral("sftp://example.invalid/path")));
        QCOMPARE(tabs->currentWidget(), original->widget());
        QCOMPARE(tabs->count(), 3);
        QVERIFY(panel->findChild<QLabel *>(QStringLiteral("terminalPanelStatus"))->text().contains(QStringLiteral("not an existing local folder")));
        panel->follow(QUrl::fromLocalFile(first.filePath(QStringLiteral("missing"))));
        QCOMPARE(tabs->count(), 3);

        terminal->sendInput(QStringLiteral("cd '%1'\n").arg(third.path()));
        QTRY_COMPARE(terminal->currentWorkingDirectory(), third.path());
        panel->follow(QUrl::fromLocalFile(originalFolder));
        QCOMPARE(tabs->count(), 4);
        QVERIFY(tabs->currentWidget() != original->widget());
        panel->follow(QUrl::fromLocalFile(third.path()));
        QCOMPARE(tabs->currentWidget(), original->widget());

        QPointer<KParts::ReadOnlyPart> retained(original);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            box->button(QMessageBox::No)->click();
        });
        Q_EMIT tabs->tabCloseRequested(tabs->currentIndex());
        QVERIFY(retained);
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            box->button(QMessageBox::Yes)->click();
        });
        Q_EMIT tabs->tabCloseRequested(tabs->currentIndex());
        QVERIFY(retained.isNull());
        QCOMPARE(tabs->count(), 3);
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
