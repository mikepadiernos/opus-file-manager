// SPDX-License-Identifier: GPL-3.0-or-later

#include "filterbar.h"
#include "folderview.h"
#include "keyboardmapdialog.h"
#include "mainwindow.h"
#include "pane.h"
#include "settings.h"
#include "tabmodel.h"
#include "yazicolumns.h"
#include "yazikeys.h"

#include <KActionCollection>
#include <KFilePlacesView>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <optional>

namespace
{
constexpr int ManyFiles = 120;

QModelIndex rowNamed(const QAbstractItemView *view, const QString &name)
{
    const QAbstractItemModel *model = view->model();
    for (int row = 0; row < model->rowCount(view->rootIndex()); ++row) {
        const QModelIndex index = model->index(row, 0, view->rootIndex());
        if (index.data().toString() == name) {
            return index;
        }
    }
    return {};
}

// Presses @p key on the focused widget and returns the widget focused afterwards.
QWidget *pressOnFocus(Qt::Key key, Qt::KeyboardModifiers modifiers = {})
{
    QTest::keyClick(QApplication::focusWidget(), key, modifiers);
    return QApplication::focusWidget();
}

QUrl stripped(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash);
}
}

class VimNavigationUiTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        const QDir root(m_root.path());
        for (const char *path : {"a/sub", "a/other", "b", "many"}) {
            QVERIFY(root.mkpath(QLatin1String(path)));
        }
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        for (int i = 0; i < ManyFiles; ++i) {
            QFile entry(m_root.filePath(QStringLiteral("many/file%1.txt").arg(i, 3, 10, QLatin1Char('0'))));
            QVERIFY(entry.open(QIODevice::WriteOnly));
            entry.close();
        }
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
        m_sub = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub")));
        m_many = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("many")));
    }

    void init()
    {
        Settings::self()->setDefaults();
        Settings::setConfirmClosingMultipleTabs(false);
        Settings::setStartupMode(Settings::EnumStartupMode::RestoreSession);
        Settings::self()->save();
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
    }

    void cleanup()
    {
        Settings::self()->setDefaults();
        Settings::self()->save();
    }

    void isOffByDefault()
    {
        QVERIFY(!Settings::vimNavigation());
        MainWindow window;
        showWithFocusOnItems(window, m_a);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QVERIFY(!pane->vimNavigationActive());
        QAction *toggle = window.actionCollection()->action(QStringLiteral("toggle_vim_navigation"));
        QVERIFY(toggle);
        QVERIFY(toggle->isCheckable());
        QVERIFY(!toggle->isChecked());
        QVERIFY(toggle->shortcuts().isEmpty());

        // Letters filter as you type, as before.
        Settings::setFilterTypeAhead(true);
        QAbstractItemView *items = pane->view()->currentView();
        QTRY_VERIFY(rowNamed(items, QStringLiteral("sub")).isValid());
        items->setCurrentIndex(rowNamed(items, QStringLiteral("sub")));
        auto *filterBar = pane->findChild<FilterBar *>();
        QVERIFY(filterBar);
        pressOnFocus(Qt::Key_J);
        QTRY_VERIFY(filterBar->isVisible());
        QCOMPARE(filterBar->filter().text(), QStringLiteral("j"));
        QCOMPARE(pane->url(), m_a);
    }

    void toggleActionTurnsItOnInBothPanes()
    {
        MainWindow window;
        showWithFocusOnItems(window, m_a);
        if (QTest::currentTestFailed()) {
            return;
        }
        QAction *toggle = window.actionCollection()->action(QStringLiteral("toggle_vim_navigation"));
        QSignalSpy messages(window.activePane(), &Pane::statusMessage);
        toggle->trigger();
        QVERIFY(Settings::vimNavigation());
        QVERIFY(window.activePane()->vimNavigationActive());
        QVERIFY(window.inactivePane()->vimNavigationActive());
        toggle->trigger();
        QVERIFY(!Settings::vimNavigation());
        QVERIFY(!window.activePane()->vimNavigationActive());
        QVERIFY(!window.inactivePane()->vimNavigationActive());

        // Applying settings, as Preferences does, follows the setting.
        Settings::setVimNavigation(true);
        window.applySettings();
        QVERIFY(toggle->isChecked());
        QVERIFY(window.activePane()->vimNavigationActive());
    }

    void movesTheCurrentItem()
    {
        Settings::setVimNavigation(true);
        MainWindow window;
        showWithFocusOnItems(window, m_many);
        if (QTest::currentTestFailed()) {
            return;
        }
        FolderView *view = window.activePane()->view();
        view->setViewMode(FolderView::ViewMode::Details);
        QAbstractItemView *items = view->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        QTRY_COMPARE(items->model()->rowCount(items->rootIndex()), ManyFiles);
        items->setCurrentIndex(items->model()->index(0, 0, items->rootIndex()));
        const auto row = [items] {
            return items->currentIndex().row();
        };

        pressOnFocus(Qt::Key_J);
        QCOMPARE(row(), 1);
        QVERIFY(items->selectionModel()->isSelected(items->currentIndex()));
        pressOnFocus(Qt::Key_K);
        QCOMPARE(row(), 0);
        pressOnFocus(Qt::Key_5);
        QVERIFY(window.activePane()->vimKeys()->isPending());
        QCOMPARE(window.activePane()->vimKeys()->pendingText(), QStringLiteral("5"));
        pressOnFocus(Qt::Key_J);
        QCOMPARE(row(), 5);
        pressOnFocus(Qt::Key_G, Qt::ShiftModifier);
        QCOMPARE(row(), ManyFiles - 1);
        pressOnFocus(Qt::Key_G);
        QCOMPARE(row(), ManyFiles - 1);
        pressOnFocus(Qt::Key_G);
        QCOMPARE(row(), 0);

        // J and K move half a page, as in ranger.
        pressOnFocus(Qt::Key_J, Qt::ShiftModifier);
        const int halfPage = row();
        QVERIFY2(halfPage > 1, qPrintable(QString::number(halfPage)));
        pressOnFocus(Qt::Key_K, Qt::ShiftModifier);
        QCOMPARE(row(), 0);

        // Ctrl+U, which no window shortcut uses, moves half a page up.
        pressOnFocus(Qt::Key_J, Qt::ShiftModifier);
        pressOnFocus(Qt::Key_U, Qt::ControlModifier);
        QCOMPARE(row(), 0);

        // Ctrl+D keeps running Add to Favorites rather than moving.
        QAction *favorite = window.actionCollection()->action(QStringLiteral("add_to_favorites"));
        QVERIFY(favorite);
        QObject::disconnect(favorite, &QAction::triggered, &window, nullptr);
        QSignalSpy favorited(favorite, &QAction::triggered);
        pressOnFocus(Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(favorited.count(), 1);
        QCOMPARE(row(), 0);

        // The view's own keys keep working.
        pressOnFocus(Qt::Key_Down);
        QCOMPARE(row(), 1);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(items));

        // In the other view modes too.
        view->setViewMode(FolderView::ViewMode::Compact);
        items = view->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        items->setCurrentIndex(items->model()->index(0, 0, items->rootIndex()));
        pressOnFocus(Qt::Key_G, Qt::ShiftModifier);
        QCOMPARE(items->currentIndex().row(), ManyFiles - 1);
    }

    void goesToTheParentOpensAndMovesThroughHistoryAndTabs()
    {
        Settings::setVimNavigation(true);
        MainWindow window;
        showWithFocusOnItems(window, m_sub);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();

        // h opens the parent folder with the folder just left focused.
        pressOnFocus(Qt::Key_H);
        QTRY_COMPARE(stripped(pane->url()), stripped(m_a));
        QTRY_COMPARE(items->currentIndex().data().toString(), QStringLiteral("sub"));

        // l opens the focused folder.
        items->setCurrentIndex(rowNamed(items, QStringLiteral("other")));
        pressOnFocus(Qt::Key_L);
        QTRY_COMPARE(stripped(pane->url()).fileName(), QStringLiteral("other"));

        // H and L go back and forward.
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());
        pressOnFocus(Qt::Key_H, Qt::ShiftModifier);
        QTRY_COMPARE(stripped(pane->url()), stripped(m_a));
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());
        pressOnFocus(Qt::Key_L, Qt::ShiftModifier);
        QTRY_COMPARE(stripped(pane->url()).fileName(), QStringLiteral("other"));

        // Without later history, L says so.
        QSignalSpy messages(pane, &Pane::statusMessage);
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());
        QVERIFY(!pane->tabs()->canGoForward());
        pressOnFocus(Qt::Key_L, Qt::ShiftModifier);
        QVERIFY(!messages.isEmpty());
        QVERIFY(messages.last().first().toString().contains(QStringLiteral("no later folder")));

        // gt and gT switch tabs, or say there is only one.
        messages.clear();
        QCOMPARE(pane->tabs()->count(), 2);
        pane->tabs()->setCurrentIndex(0);
        pane->view()->currentView()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());
        pressOnFocus(Qt::Key_G);
        QVERIFY(pane->vimKeys()->isPending());
        QCOMPARE(pane->vimKeys()->pendingText(), QStringLiteral("g"));
        pressOnFocus(Qt::Key_T);
        QTRY_COMPARE(pane->tabs()->currentIndex(), 1);
        pane->view()->currentView()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_T, Qt::ShiftModifier);
        QTRY_COMPARE(pane->tabs()->currentIndex(), 0);
        pane->tabs()->setCurrentIndex(1);
        pane->closeCurrentTab();
        QCOMPARE(pane->tabs()->count(), 1);
        pane->view()->currentView()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_T);
        QVERIFY(!messages.isEmpty());
        QVERIFY(messages.last().first().toString().contains(QStringLiteral("only one tab")));
    }

    void slashFiltersAndBackslashClears()
    {
        Settings::setVimNavigation(true);
        MainWindow window;
        showWithFocusOnItems(window, m_a);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        auto *filterBar = pane->findChild<FilterBar *>();
        QVERIFY(filterBar);
        QSignalSpy messages(pane, &Pane::statusMessage);

        // \ says when there is nothing to clear.
        pressOnFocus(Qt::Key_Backslash);
        QVERIFY(!messages.isEmpty());
        QVERIFY(messages.last().first().toString().contains(QStringLiteral("no filter to clear")));

        // / opens the filter bar, which takes the navigation keys as text.
        pressOnFocus(Qt::Key_Slash);
        QTRY_VERIFY(filterBar->isVisible());
        QVERIFY(qobject_cast<QLineEdit *>(QApplication::focusWidget()));
        QVERIFY(filterBar->isAncestorOf(QApplication::focusWidget()));
        for (Qt::Key key : {Qt::Key_S, Qt::Key_U, Qt::Key_B, Qt::Key_J, Qt::Key_K, Qt::Key_G}) {
            pressOnFocus(key);
        }
        QCOMPARE(filterBar->filter().text(), QStringLiteral("subjkg"));
        QCOMPARE(pane->url(), m_a);
        QVERIFY(!pane->vimKeys()->isPending());

        // \ in the file view clears it.
        filterBar->setFilter(NameFilter(QStringLiteral("sub"), filterBar->filter().scope()));
        pane->view()->currentView()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());
        pressOnFocus(Qt::Key_Backslash);
        QTRY_VERIFY(filterBar->filter().text().isEmpty());
        QVERIFY(!filterBar->isVisible());
    }

    void keysAreLeftToTypingAndOtherWidgets()
    {
        Settings::setVimNavigation(true);
        MainWindow window;
        showWithFocusOnItems(window, m_a);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QAbstractItemView *items = view->currentView();
        QTRY_VERIFY(rowNamed(items, QStringLiteral("sub")).isValid());
        items->setCurrentIndex(rowNamed(items, QStringLiteral("sub")));

        // The location bar.
        pane->editUrl();
        QTRY_VERIFY(YaziKeys::isTypingWidget(QApplication::focusWidget()));
        QWidget *locationWidget = QApplication::focusWidget();
        auto *combo = qobject_cast<QComboBox *>(locationWidget);
        QLineEdit *location = combo ? combo->lineEdit() : qobject_cast<QLineEdit *>(locationWidget);
        QVERIFY(location);
        pressOnFocus(Qt::Key_End);
        for (Qt::Key key : {Qt::Key_H, Qt::Key_J, Qt::Key_Slash}) {
            pressOnFocus(key);
        }
        QVERIFY2(location->text().endsWith(QStringLiteral("hj/")), qPrintable(location->text()));
        QTest::qWait(50);
        QCOMPARE(pane->url(), m_a);
        pressOnFocus(Qt::Key_Escape);

        // Another widget, such as Places, keeps its keys.
        if (auto *places = window.findChild<KFilePlacesView *>(); places && places->isVisible()) {
            places->setFocus(Qt::OtherFocusReason);
            QTRY_VERIFY(places->hasFocus());
            pressOnFocus(Qt::Key_H);
            pressOnFocus(Qt::Key_G);
            QVERIFY(!pane->vimKeys()->isPending());
            QTest::qWait(50);
            QCOMPARE(pane->url(), m_a);
        }

        // An inline rename editor, which the cards view mode offers.
        view->setViewMode(FolderView::ViewMode::Cards);
        items = view->currentView();
        QTRY_VERIFY(rowNamed(items, QStringLiteral("sub")).isValid());
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        items->setCurrentIndex(rowNamed(items, QStringLiteral("sub")));
        items->selectionModel()->select(rowNamed(items, QStringLiteral("sub")), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        // The keys apply in the new view until the editor opens.
        pressOnFocus(Qt::Key_G);
        QVERIFY(pane->vimKeys()->isPending());
        pressOnFocus(Qt::Key_Escape);
        QVERIFY(!pane->vimKeys()->isPending());
        QVERIFY(view->renameInline());
        QTRY_VERIFY(YaziKeys::isTypingWidget(QApplication::focusWidget()));
        QWidget *editor = QApplication::focusWidget();
        QVERIFY(items->isAncestorOf(editor));
        for (Qt::Key key : {Qt::Key_End, Qt::Key_H, Qt::Key_L, Qt::Key_G}) {
            pressOnFocus(key);
        }
        QVERIFY(!pane->vimKeys()->isPending());
        auto *editorText = qobject_cast<QLineEdit *>(editor);
        if (editorText) {
            QVERIFY2(editorText->text().endsWith(QStringLiteral("hlg")), qPrintable(editorText->text()));
        }
        pressOnFocus(Qt::Key_Escape);
        QCOMPARE(pane->url(), m_a);
    }

    void yaziModeUsesItsOwnPreset()
    {
        Settings::setVimNavigation(true);
        Settings::setFilterTypeAhead(true);
        MainWindow window;
        showWithFocusOnItems(window, m_a);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QVERIFY(pane->vimNavigationActive());
        pane->setYaziMode(true);
        QVERIFY(!pane->vimNavigationActive());
        QAbstractItemView *items = pane->view()->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        QTRY_VERIFY(rowNamed(items, QStringLiteral("sub")).isValid());
        items->setCurrentIndex(rowNamed(items, QStringLiteral("sub")));

        // The standard preset doesn't bind j, so it filters as you type.
        auto *filterBar = pane->findChild<FilterBar *>();
        pressOnFocus(Qt::Key_J);
        QTRY_VERIFY(filterBar->isVisible());
        QCOMPARE(filterBar->filter().text(), QStringLiteral("j"));
        filterBar->close();
        QTRY_VERIFY(items->hasFocus());

        // The window's setting doesn't turn the keys on in the Yazi pane.
        window.applySettings();
        QVERIFY(!pane->vimNavigationActive());
        pane->setYaziMode(false);
        QVERIFY(pane->vimNavigationActive());
    }

    void keyboardMapListsTheKeys()
    {
        MainWindow window;
        const auto vimEntry = [&window](const QString &name) {
            const QList<KeyboardMap::Entry> entries = KeyboardMapDialog::entries(&window);
            const auto found = std::find_if(entries.cbegin(), entries.cend(), [&name](const KeyboardMap::Entry &entry) {
                return entry.name == name;
            });
            return found == entries.cend() ? std::optional<KeyboardMap::Entry>() : *found;
        };
        std::optional<KeyboardMap::Entry> down = vimEntry(QStringLiteral("vim-move-down"));
        QVERIFY(down);
        QVERIFY(down->shortcuts.contains(QKeySequence(Qt::Key_J)));
        QVERIFY(down->context.contains(QStringLiteral("Vim Mode")));
        std::optional<KeyboardMap::Entry> first = vimEntry(QStringLiteral("vim-move-first"));
        QVERIFY(first);
        QVERIFY(first->shortcuts.contains(QKeySequence(Qt::Key_G, Qt::Key_G)));

        Settings::setVimNavigation(true);
        down = vimEntry(QStringLiteral("vim-move-down"));
        QVERIFY(down);
        QCOMPARE(down->context, QStringLiteral("File views with Vim-style navigation"));
        // The Yazi preset's entries keep their names.
        QVERIFY(vimEntry(QStringLiteral("move-down")));
    }

private:
    // Shows @p window with the active pane at @p url and focuses its items.
    void showWithFocusOnItems(MainWindow &window, const QUrl &url)
    {
        window.resize(1400, 600);
        window.openUrls({url, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        Pane *pane = window.activePane();
        QTRY_VERIFY(!pane->view()->isLoading());
        QAbstractItemView *items = pane->view()->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
    }

    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;
    QUrl m_sub;
    QUrl m_many;
};

QTEST_MAIN(VimNavigationUiTest)

#include "vimnavigationuitest.moc"
