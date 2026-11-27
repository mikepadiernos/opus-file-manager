// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalpanel.h"
#include "atuin.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "settings.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KConfigDialog>
#include <KConfigGroup>
#include <KParts/ReadOnlyPart>
#include <KSharedConfig>
#include <kde_terminal_interface.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

class TerminalPanelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/konsole");
        QVERIFY(QDir().mkpath(path));
        const auto write = [&path](const QString &name, const QByteArray &contents) {
            QFile file(path + QLatin1Char('/') + name);
            return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
        };
        QVERIFY(write(QStringLiteral("BlurOn.profile"), "[General]\nName=Blur On\nTerminalMargin=8\n[Appearance]\nColorScheme=BlurOn\n"));
        QVERIFY(write(QStringLiteral("BlurOff.profile"), "[General]\nName=Blur Off\n[Appearance]\nColorScheme=BlurOff\n"));
        QVERIFY(write(QStringLiteral("BlurOn.colorscheme"),
                      "[Background]\nColor=0,0,0\n[Foreground]\nColor=255,255,255\n[General]\nDescription=Blur On\nOpacity=0.7\nBlur=true\n"));
        QVERIFY(write(QStringLiteral("BlurOff.colorscheme"), "[General]\nDescription=Blur Off\nOpacity=0.5\nBlur=false\n"));
        auto config = KSharedConfig::openConfig();
        config->group(QStringLiteral("Desktop Entry")).writeEntry("DefaultProfile", "BlurOn.profile");
        config->sync();
    }

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

    void componentChoiceAppliesToNewSessions()
    {
        const auto reset = qScopeGuard([] {
            Settings::setEmbeddedTerminal(Settings::EnumEmbeddedTerminal::Konsole);
        });
        if (TerminalProviders::isPartInstalled(TerminalProviders::Provider::Kwi)) {
            QSKIP("The Kwi terminal component is installed in this environment");
        }
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        TerminalPanel panel;
        auto *status = panel.findChild<QLabel *>(QStringLiteral("terminalPanelStatus"));
        auto *components = panel.findChild<QMenu *>(QStringLiteral("terminalComponentMenu"));
        auto *konsole = panel.findChild<QAction *>(QStringLiteral("terminalComponent_konsole"));
        auto *kwi = panel.findChild<QAction *>(QStringLiteral("terminalComponent_kwi"));
        QVERIFY(status && components && konsole && kwi);
        QCOMPARE(TerminalPanel::embeddedProvider(), TerminalProviders::Provider::Konsole);

        Q_EMIT components->aboutToShow();
        QVERIFY(!kwi->isEnabled());
        QVERIFY(kwi->toolTip().contains(QStringLiteral("not installed")));
        QCOMPARE(konsole->isChecked(), true);

        panel.setEmbeddedProvider(TerminalProviders::Provider::Kwi);
        QCOMPARE(Settings::embeddedTerminal(), Settings::EnumEmbeddedTerminal::Kwi);
        QVERIFY(status->text().contains(QStringLiteral("Kwi")));
        Q_EMIT components->aboutToShow();
        QVERIFY(kwi->isChecked() && kwi->isEnabled());

        panel.start(QUrl::fromLocalFile(folder.path()));
        QVERIFY(!panel.hasSessions());
        QVERIFY(panel.findChildren<KParts::ReadOnlyPart *>().isEmpty());
        QVERIFY(status->text().contains(QStringLiteral("Kwi")));
        QVERIFY(status->text().contains(QStringLiteral("not installed")));

        konsole->trigger();
        QCOMPARE(Settings::embeddedTerminal(), Settings::EnumEmbeddedTerminal::Konsole);
        QVERIFY(status->text().contains(QStringLiteral("New terminal sessions use Konsole")));
        panel.start(QUrl::fromLocalFile(folder.path()));
        if (!TerminalProviders::isPartInstalled(TerminalProviders::Provider::Konsole)) {
            QSKIP("KonsolePart is not available in this environment");
        }
        QVERIFY(panel.hasSessions());
        auto *tabs = panel.findChild<QTabWidget *>(QStringLiteral("terminalSessions"));
        QVERIFY(tabs->tabToolTip(0).startsWith(QStringLiteral("Konsole session started in ")));
    }

    void historySearchRunsOnlyInIdleSessions()
    {
        const QString home = QDir::homePath();
        const QString atuin = home + QStringLiteral("/.atuin/bin/atuin");
        const QString marker = home + QStringLiteral("/atuin-arguments");
        const auto cleanup = qScopeGuard([&home, &marker] {
            QDir(home + QStringLiteral("/.atuin")).removeRecursively();
            QFile::remove(Atuin::databasePath());
            QFile::remove(marker);
        });
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        TerminalPanel panel;
        panel.resize(600, 300);
        panel.show();
        auto *status = panel.findChild<QLabel *>(QStringLiteral("terminalPanelStatus"));
        auto *action = panel.findChild<QAction *>(QStringLiteral("terminalPanelAtuinAction"));
        QVERIFY(status && action);

        action->trigger();
        QVERIFY(status->text().contains(QStringLiteral("Atuin is not installed")));
        QVERIFY(QDir().mkpath(QFileInfo(atuin).absolutePath()));
        QFile script(atuin);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > '" + marker.toLocal8Bit() + "'\n");
        script.close();
        QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
        action->trigger();
        QVERIFY(status->text().contains(QStringLiteral("atuin init")));
        QVERIFY(!QFile::exists(Atuin::databasePath()));
        QVERIFY(QDir().mkpath(QFileInfo(Atuin::databasePath()).absolutePath()));
        QFile database(Atuin::databasePath());
        QVERIFY(database.open(QIODevice::WriteOnly));
        database.close();
        action->trigger();
        QVERIFY(status->text().contains(QStringLiteral("Open a terminal session")));

        panel.start(QUrl::fromLocalFile(folder.path()));
        auto *part = panel.findChild<KParts::ReadOnlyPart *>();
        if (!part) {
            QSKIP("KonsolePart is not available in this environment");
        }
        auto *terminal = qobject_cast<TerminalInterface *>(part);
        QVERIFY(terminal);
        panel.searchHistory();
        QTRY_VERIFY2(QFile::exists(marker), qPrintable(status->text()));
        QFile arguments(marker);
        QTRY_VERIFY(arguments.size() > 0);
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        QCOMPARE(arguments.readAll(), QByteArray("search\n--interactive\n"));
        QVERIFY(status->text().contains(QStringLiteral("without running it")));

        const auto busy = [terminal] {
            const int foreground = terminal->foregroundProcessId();
            return foreground > 0 && foreground != terminal->terminalProcessId();
        };
        QTRY_VERIFY(!busy());
        terminal->sendInput(QStringLiteral("sleep 30\n"));
        QTRY_VERIFY(busy());
        QVERIFY(QFile::remove(marker));
        panel.searchHistory();
        QVERIFY(status->text().contains(QStringLiteral("running sleep")));
        QTest::qWait(300);
        QVERIFY(!QFile::exists(marker));
    }

    void terminalEditorsRunInTheirOwnSession()
    {
        QTemporaryDir bin;
        QTemporaryDir folder;
        QVERIFY(bin.isValid() && folder.isValid());
        const QString marker = bin.filePath(QStringLiteral("arguments"));
        QFile script(bin.filePath(QStringLiteral("nano")));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nprintf '%s\\n' \"$0\" \"$@\" \"$PWD\" > '" + marker.toLocal8Bit() + "'\n");
        script.close();
        QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path] {
            qputenv("PATH", path);
        });
        qputenv("PATH", QByteArray(bin.path().toLocal8Bit() + ':' + path));
        const QString file = folder.filePath(QStringLiteral("-n notes.txt"));
        QFile note(file);
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.close();

        TerminalPanel panel;
        auto *status = panel.findChild<QLabel *>(QStringLiteral("terminalPanelStatus"));
        QVERIFY(!panel.openProgram(script.fileName(), {file}, folder.filePath(QStringLiteral("missing")), QStringLiteral("nano")));
        QVERIFY(status->text().contains(QStringLiteral("not an existing local folder")));
        if (!TerminalProviders::isPartInstalled(TerminalProviders::Provider::Konsole)) {
            QSKIP("KonsolePart is not available in this environment");
        }

        MainWindow window;
        const QString processDirectory = QDir::currentPath();
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(file)});
        auto *editWith = qobject_cast<KActionMenu *>(window.actionCollection()->action(QStringLiteral("edit_with")));
        QVERIFY(editWith && editWith->isEnabled());
        QAction *nano = editWith->menu()->findChild<QAction *>(QStringLiteral("editWith_nano"));
        QVERIFY(nano);
        nano->trigger();
        QVERIFY(window.actionCollection()->action(QStringLiteral("show_embedded_terminal"))->isChecked());
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("terminalSessions"));
        QVERIFY(tabs);
        // The editor opens without a shell session beside it.
        QCOMPARE(tabs->count(), 1);
        QCOMPARE(tabs->tabText(0).remove(QLatin1Char('&')), QStringLiteral("nano"));
        QFile arguments(marker);
        QTRY_VERIFY(arguments.exists() && arguments.size() > 0);
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromLocal8Bit(arguments.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines.at(0), script.fileName());
        QCOMPARE(lines.at(1), file);
        QCOMPARE(QFileInfo(lines.at(2)).canonicalFilePath(), QFileInfo(folder.path()).canonicalFilePath());
        QCOMPARE(QDir::currentPath(), processDirectory);
        // The tab closes when the editor exits.
        QTRY_COMPARE(tabs->count(), 0);
    }

    void headerHasHorizontalPadding()
    {
        TerminalPanel panel;
        auto *header = panel.findChild<QWidget *>(QStringLiteral("terminalPanelHeader"));
        QVERIFY(header);
        QVERIFY(header->layout());
        const QMargins margins = header->layout()->contentsMargins();
        QVERIFY(margins.left() >= 8);
        QVERIFY(margins.right() >= 8);
        auto *status = panel.findChild<QLabel *>(QStringLiteral("terminalPanelStatus"));
        QVERIFY(status);
        const QMargins statusMargins = status->parentWidget()->layout()->contentsMargins();
        QVERIFY(statusMargins.left() >= 8);
        QVERIFY(statusMargins.right() >= 8);
    }

    void terminalRetainsBothPanesAndMovesWithLayout()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path()), QUrl::fromLocalFile(folder.path())});
        window.setStackedLayout(false);
        Settings::setTerminalFollowActiveFolder(false);
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

    void panelControlsPreserveSessionAndExposeMessages()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        Settings::setTerminalFollowActiveFolder(true);
        const auto reset = qScopeGuard([] {
            Settings::setTerminalFollowActiveFolder(false);
            Settings::setEmbeddedPanelTitlesVisible(true);
        });
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        window.show();
        auto *show = window.actionCollection()->action(QStringLiteral("show_embedded_terminal"));
        show->setChecked(true);
        auto *panel = window.findChild<TerminalPanel *>(QStringLiteral("terminalPanel"));
        auto *tabs = panel->findChild<QTabWidget *>(QStringLiteral("terminalSessions"));
        auto *part = panel->findChild<KParts::ReadOnlyPart *>();
        if (!part) {
            QSKIP("KonsolePart is not available in this environment");
        }
        auto *terminal = qobject_cast<TerminalInterface *>(part);
        QVERIFY(terminal);
        QTRY_VERIFY(terminal->terminalProcessId() > 0);
        const int pid = terminal->terminalProcessId();
        auto *status = panel->findChild<QLabel *>(QStringLiteral("terminalPanelStatus"));
        auto *dismiss = panel->findChild<QToolButton *>(QStringLiteral("terminalPanelDismissStatus"));
        auto *close = panel->findChild<QToolButton *>(QStringLiteral("terminalPanelClose"));
        auto *menuButton = panel->findChild<QToolButton *>(QStringLiteral("terminalPanelMenuButton"));
        QVERIFY(status && dismiss && close && menuButton && menuButton->menu());
        auto *title = panel->findChild<QLabel *>(QStringLiteral("terminalPanelTitle"));
        QVERIFY(title && !title->isHidden());
        Settings::setEmbeddedPanelTitlesVisible(false);
        window.applySettings();
        QVERIFY(title->isHidden());
        QVERIFY(menuButton->isVisible() && close->isVisible());
        QCOMPARE(terminal->terminalProcessId(), pid);
        if (part->metaObject()->indexOfMethod("setContextMenuAdditionalActions(QList<QAction*>)") >= 0) {
            QWidget *display = nullptr;
            for (auto *widget : part->widget()->findChildren<QWidget *>()) {
                if (widget->inherits("Konsole::TerminalDisplay")) {
                    display = widget;
                    break;
                }
            }
            QVERIFY(display);
            bool panelMenuFound = false;
            QTimer::singleShot(0, [&] {
                auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                if (popup) {
                    panelMenuFound = popup->actions().contains(menuButton->menu()->menuAction());
                    popup->close();
                }
            });
            QContextMenuEvent event(QContextMenuEvent::Mouse, display->rect().center(), display->mapToGlobal(display->rect().center()));
            QApplication::sendEvent(display, &event);
            QVERIFY(panelMenuFound);
        }
        QVERIFY(status->isVisible());
        dismiss->click();
        QVERIFY(!status->isVisible());
        panel->follow(QUrl::fromLocalFile(folder.path()));
        QVERIFY(!status->isVisible());
        panel->follow(QUrl(QStringLiteral("sftp://example.invalid/path")));
        QVERIFY(status->isVisible());
        QVERIFY(status->text().contains(QStringLiteral("not an existing local folder")));
        dismiss->click();
        panel->follow(QUrl(QStringLiteral("sftp://example.invalid/path")));
        QVERIFY(status->isVisible());
        panel->follow(QUrl::fromLocalFile(folder.path()));
        QVERIFY(!status->isVisible());

        close->click();
        QVERIFY(panel->isHidden());
        QVERIFY(!show->isChecked());
        QCOMPARE(terminal->terminalProcessId(), pid);
        show->setChecked(true);
        QCOMPARE(panel->findChild<KParts::ReadOnlyPart *>(), part);
        QCOMPARE(terminal->terminalProcessId(), pid);
        QVERIFY(title->isHidden());
        Q_EMIT tabs->tabBar()->customContextMenuRequested(QPoint(2, 2));
        QTRY_VERIFY(menuButton->menu()->isVisible());
        auto *hideAction = menuButton->menu()->findChild<QAction *>(QStringLiteral("terminalPanelHideAction"));
        QVERIFY(hideAction);
        menuButton->menu()->hide();
        hideAction->trigger();
        QVERIFY(panel->isHidden());
        QVERIFY(!show->isChecked());
        QCOMPARE(terminal->terminalProcessId(), pid);
        Settings::setEmbeddedPanelTitlesVisible(true);
        window.applySettings();
        QVERIFY(!title->isHidden());
        QCOMPARE(terminal->terminalProcessId(), pid);
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

    void blurTracksProfilePlacementAndVisibility()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(first.isValid() && second.isValid());
        Settings::setTerminalFollowActiveFolder(true);
        const auto reset = qScopeGuard([] {
            Settings::setTerminalFollowActiveFolder(false);
            Settings::setTerminalPlacement(Settings::EnumTerminalPlacement::Automatic);
        });
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(first.path())});
        window.resize(900, 600);
        window.show();
        auto *panel = window.findChild<TerminalPanel *>(QStringLiteral("terminalPanel"));
        auto *tabs = panel->findChild<QTabWidget *>(QStringLiteral("terminalSessions"));
        QSignalSpy regions(panel, &TerminalPanel::blurRegionChanged);
        auto *show = window.actionCollection()->action(QStringLiteral("show_embedded_terminal"));
        show->setChecked(true);
        auto *workspace = window.findChild<QSplitter *>(QStringLiteral("workspaceSplitter"));
        QVERIFY(workspace);
        workspace->setSizes({350, 160});
        auto *part = panel->findChild<KParts::ReadOnlyPart *>();
        if (!part) {
            QSKIP("KonsolePart is not available in this environment");
        }
        auto *terminal = qobject_cast<TerminalInterface *>(part);
        QVERIFY(terminal);
        QCOMPARE(terminal->currentProfileName(), QStringLiteral("Blur On"));
        bool blurEnabled = false;
        QVERIFY(QMetaObject::invokeMethod(part, "isBlurEnabled", Qt::DirectConnection, Q_RETURN_ARG(bool, blurEnabled)));
        QVERIFY(blurEnabled);
        QVERIFY(window.testAttribute(Qt::WA_TranslucentBackground));
        QTRY_VERIFY(part->widget()->isVisible());
        const auto currentRegion = [&regions] {
            return regions.isEmpty() ? QRegion() : regions.last().first().value<QRegion>();
        };
        const auto expectedRegion = [&window, &tabs] {
            QWidget *widget = tabs->currentWidget();
            return QRegion(QRect(widget->mapTo(&window, QPoint()), widget->size()));
        };
        QTRY_VERIFY(!expectedRegion().isEmpty());
        QTRY_VERIFY(!currentRegion().isEmpty());
        QTRY_COMPARE(currentRegion(), expectedRegion());
        regions.clear();
        QEvent repaint(QEvent::UpdateRequest);
        QApplication::sendEvent(&window, &repaint);
        QVERIFY(!regions.isEmpty());
        QCOMPARE(currentRegion(), expectedRegion());
        QImage snapshot(window.size(), QImage::Format_ARGB32_Premultiplied);
        snapshot.fill(Qt::transparent);
        window.render(&snapshot);
        const QRect terminalRect = expectedRegion().boundingRect().intersected(snapshot.rect());
        int translucentPixels = 0;
        for (int y = terminalRect.top(); y <= terminalRect.bottom(); ++y) {
            for (int x = terminalRect.left(); x <= terminalRect.right(); ++x) {
                const int alpha = snapshot.pixelColor(x, y).alpha();
                if (alpha > 0 && alpha < 255) {
                    ++translucentPixels;
                }
            }
        }
        QVERIFY(translucentPixels > 100);
        const QRegion below = currentRegion();
        Settings::setTerminalPlacement(Settings::EnumTerminalPlacement::Right);
        window.applySettings();
        workspace->setSizes({500, 250});
        QTRY_COMPARE(currentRegion(), expectedRegion());
        QVERIFY(currentRegion() != below);
        const QRegion right = currentRegion();
        workspace->setSizes({400, 350});
        QTRY_COMPARE(currentRegion(), expectedRegion());
        QVERIFY(currentRegion() != right);
        show->setChecked(false);
        QTRY_VERIFY(currentRegion().isEmpty());
        show->setChecked(true);
        QTRY_VERIFY(!currentRegion().isEmpty());
        QTRY_COMPARE(currentRegion(), expectedRegion());

        QVERIFY(QMetaObject::invokeMethod(part, "showManageProfilesDialog", Qt::DirectConnection, Q_ARG(QWidget *, &window)));
        auto *dialog = window.findChild<KConfigDialog *>(QStringLiteral("konsolepartmanageprofiles"));
        QVERIFY(dialog);
        auto *profiles = dialog->findChild<QTreeView *>(QStringLiteral("profileListView"));
        auto *setDefault = dialog->findChild<QPushButton *>(QStringLiteral("setAsDefaultButton"));
        QVERIFY(profiles && setDefault);
        const auto matches = profiles->model()->match(profiles->model()->index(0, 0), Qt::DisplayRole, QStringLiteral("Blur Off"), 1, Qt::MatchExactly);
        QCOMPARE(matches.size(), 1);
        profiles->selectionModel()->select(matches.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QVERIFY(setDefault->isEnabled());
        setDefault->click();
        dialog->close();

        panel->follow(QUrl::fromLocalFile(second.path()));
        QCOMPARE(tabs->count(), 2);
        auto *newPart = panel->findChildren<KParts::ReadOnlyPart *>().last();
        auto *newTerminal = qobject_cast<TerminalInterface *>(newPart);
        QVERIFY(newTerminal);
        QCOMPARE(newTerminal->currentProfileName(), QStringLiteral("Blur Off"));
        QTRY_VERIFY(currentRegion().isEmpty());
        tabs->setCurrentWidget(part->widget());
        QTRY_VERIFY(!currentRegion().isEmpty());
        QTRY_COMPARE(currentRegion(), expectedRegion());
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            box->button(QMessageBox::Yes)->click();
        });
        Q_EMIT tabs->tabCloseRequested(tabs->currentIndex());
        QCOMPARE(tabs->count(), 1);
        QCOMPARE(tabs->currentWidget(), newPart->widget());
        QTRY_VERIFY(currentRegion().isEmpty());
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
