// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "settings.h"
#include "shortcuthintpopup.h"
#include "shortcutreference.h"
#include "windowsequencehints.h"
#include "yazicolumns.h"
#include "yazikeys.h"

#include <KActionCollection>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWheelEvent>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
void press(Qt::Key key, Qt::KeyboardModifiers modifiers = {})
{
    QTest::keyClick(QApplication::focusWidget(), key, modifiers);
}

bool hasHint(const QList<ShortcutHints::Hint> &hints, const QString &keys)
{
    return std::any_of(hints.cbegin(), hints.cend(), [&keys](const ShortcutHints::Hint &hint) {
        return hint.keys == keys;
    });
}

void wheel(QWidget *widget, int delta)
{
    const QPointF position = QRectF(widget->rect()).center();
    QWheelEvent event(position, widget->mapToGlobal(position), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(widget, &event);
}
}

class ShortcutHintsUiTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        const QDir root(m_root.path());
        QVERIFY(root.mkpath(u"a/sub"_s));
        QVERIFY(root.mkpath(u"b"_s));
        for (int i = 0; i < 20; ++i) {
            QFile file(m_root.filePath(u"a/file%1.txt"_s.arg(i, 2, 10, QLatin1Char('0'))));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        m_a = QUrl::fromLocalFile(m_root.filePath(u"a"_s));
        m_b = QUrl::fromLocalFile(m_root.filePath(u"b"_s));
    }

    void init()
    {
        Settings::self()->setDefaults();
        Settings::setConfirmClosingMultipleTabs(false);
        Settings::setStartupMode(Settings::EnumStartupMode::RestoreSession);
        Settings::self()->save();
        KSharedConfig::openStateConfig()->deleteGroup(u"Session"_s);
        KSharedConfig::openStateConfig()->sync();
    }

    void cleanup()
    {
        Settings::self()->setDefaults();
        Settings::self()->save();
    }

    void listsWhatFollowsAVimSequence()
    {
        Settings::setVimNavigation(true);
        MainWindow window;
        showWithFocusOnItems(window);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        items->setCurrentIndex(items->model()->index(5, 0, items->rootIndex()));

        press(Qt::Key_G);
        QVERIFY(pane->vimKeys()->isPending());
        ShortcutHintPopup *popup = pane->shortcutHintPopup();
        // Not at once: a quick sequence never shows it.
        QVERIFY(!popup->isVisible());
        QTRY_VERIFY(popup->isShowing(ShortcutHintPopup::Source::Keys));
        QCOMPARE(popup->title(), u"g…"_s);
        QVERIFY(hasHint(popup->hints(), u"g"_s));
        QVERIFY(hasHint(popup->hints(), u"h"_s));
        QVERIFY(hasHint(popup->hints(), u"T"_s));
        QVERIFY(!hasHint(popup->hints(), u"j"_s));
        QCOMPARE(popup->accessibleName(), u"Shortcut hints"_s);
        QVERIFY(popup->accessibleDescription().contains(u"Move to the first item"_s));
        // It never takes the focus, and it floats over the file view.
        QCOMPARE(popup->focusPolicy(), Qt::NoFocus);
        QVERIFY(items->hasFocus());
        QWidget *area = pane->shortcutHintArea();
        // It may still be sliding in.
        const QRect areaRect(area->mapTo(pane, QPoint()), area->size());
        QTRY_VERIFY2(areaRect.contains(popup->geometry().center()),
                     qPrintable(u"%1 %2"_s.arg(QDebug::toString(areaRect), QDebug::toString(popup->geometry()))));

        // While it shows, the sequence doesn't expire, and the paging keys keep it.
        QTest::qWait(YaziKeys::DefaultPendingTimeout + 200);
        QVERIFY(pane->vimKeys()->isPending());
        press(Qt::Key_PageDown);
        press(Qt::Key_Down);
        QVERIFY(pane->vimKeys()->isPending());
        QCOMPARE(items->currentIndex().row(), 5);
        QVERIFY(popup->isVisible());

        // Completing the sequence runs it and hides the hints.
        press(Qt::Key_G);
        QVERIFY(!pane->vimKeys()->isPending());
        QVERIFY(!popup->isVisible());
        QCOMPARE(items->currentIndex().row(), 0);

        // Escape cancels the sequence and hides them.
        press(Qt::Key_G);
        QTRY_VERIFY(popup->isVisible());
        press(Qt::Key_Escape);
        QVERIFY(!pane->vimKeys()->isPending());
        QVERIFY(!popup->isVisible());

        // A count lists only the commands it repeats.
        press(Qt::Key_5);
        QTRY_VERIFY(popup->isVisible());
        QCOMPARE(popup->title(), u"5…"_s);
        QVERIFY(hasHint(popup->hints(), u"j"_s));
        QVERIFY(!hasHint(popup->hints(), u"h"_s));
        // The next key updates them at once.
        press(Qt::Key_0);
        QCOMPARE(popup->title(), u"50…"_s);
        press(Qt::Key_Escape);
        QVERIFY(!popup->isVisible());

        // Moving the focus cancels the sequence and hides them.
        press(Qt::Key_G);
        QTRY_VERIFY(popup->isVisible());
        window.inactivePane()->view()->currentView()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(!popup->isVisible());
        QVERIFY(!pane->vimKeys()->isPending());
    }

    void pagesLongListsInYaziMode()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, QSize(500, 500));
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        pane->setYaziMode(true);
        QAbstractItemView *items = pane->view()->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        YaziKeys *keys = pane->yaziColumns()->keys();

        press(Qt::Key_Comma);
        QVERIFY(keys->isPending());
        ShortcutHintPopup *popup = pane->shortcutHintPopup();
        QTRY_VERIFY(popup->isShowing(ShortcutHintPopup::Source::Keys));
        QCOMPARE(popup->hints().size(), 5);
        QVERIFY(pane->shortcutHintArea() == pane->yaziColumns());
        press(Qt::Key_Escape);
        QVERIFY(!popup->isVisible());

        // A small pane pages the many keys of a count. A count takes Page Down,
        // so the wheel turns the pages.
        press(Qt::Key_3);
        QTRY_VERIFY(popup->isVisible());
        QVERIFY2(popup->pageCount() > 1, qPrintable(QString::number(popup->pageCount())));
        QVERIFY(popup->findChild<QLabel *>(u"shortcutHintFooter"_s)->text().contains(u"Scroll"_s));
        const QList<ShortcutHints::Hint> first = popup->pageHints();
        wheel(popup, -QWheelEvent::DefaultDeltasPerStep);
        QCOMPARE(popup->page(), 1);
        QVERIFY(popup->pageHints() != first);
        wheel(popup, QWheelEvent::DefaultDeltasPerStep);
        QCOMPARE(popup->page(), 0);
        QVERIFY(keys->isPending());
        press(Qt::Key_Escape);
        QVERIFY(!keys->isPending());
        QVERIFY(!popup->isVisible());
    }

    void listsWhatFollowsAWindowSequence()
    {
        MainWindow window;
        showWithFocusOnItems(window);
        if (QTest::currentTestFailed()) {
            return;
        }
        QAction *action = window.actionCollection()->addAction(u"test_sequence"_s);
        action->setText(u"Test Sequence"_s);
        action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_T));
        window.addAction(action);
        QSignalSpy triggered(action, &QAction::triggered);
        WindowSequenceHints *hints = window.windowSequenceHints();
        QVERIFY(hints);
        Pane *pane = window.activePane();

        press(Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(hints->isPending());
        ShortcutHintPopup *popup = pane->shortcutHintPopup();
        QTRY_VERIFY(popup->isShowing(ShortcutHintPopup::Source::Window));
        const QString ctrlT = QKeySequence(Qt::CTRL | Qt::Key_T).toString(QKeySequence::NativeText);
        QVERIFY(hasHint(popup->hints(), ctrlT));
        QVERIFY(popup->title().startsWith(QKeySequence(Qt::CTRL | Qt::Key_K).toString(QKeySequence::NativeText)));
        QVERIFY(pane->view()->currentView()->hasFocus());
        press(Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(triggered.size(), 1);
        QVERIFY(!hints->isPending());
        QVERIFY(!popup->isVisible());

        // A key that doesn't fit ends it, as it ends Qt's.
        press(Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(hints->isPending());
        press(Qt::Key_Escape);
        QVERIFY(!hints->isPending());
        press(Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(triggered.size(), 1);

        // Cancelling ends Qt's sequence too, so the next key doesn't complete it.
        press(Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(hints->isPending());
        hints->cancel();
        QVERIFY(!hints->isPending());
        press(Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(triggered.size(), 1);

        // A shortcut of one key begins nothing.
        action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
        press(Qt::Key_K, Qt::ControlModifier);
        QCOMPARE(triggered.size(), 2);
        QTest::qWait(20);
        QVERIFY(!hints->isPending());
    }

    void appliesTheHintSettings()
    {
        Settings::setVimNavigation(true);
        Settings::setShortcutHints(false);
        Settings::setKeySequenceTimeout(300);
        MainWindow window;
        showWithFocusOnItems(window);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        YaziKeys *keys = pane->vimKeys();
        QCOMPARE(keys->pendingTimeout(), 300);
        QCOMPARE(pane->yaziColumns()->keys()->pendingTimeout(), 300);
        QAbstractItemView *items = pane->view()->currentView();
        items->setCurrentIndex(items->model()->index(5, 0, items->rootIndex()));
        ShortcutHintPopup *popup = pane->shortcutHintPopup();

        // Without hints, an incomplete sequence expires after the timeout, and
        // the next key starts afresh rather than completing it.
        press(Qt::Key_G);
        QVERIFY(keys->isPending());
        QTRY_VERIFY_WITH_TIMEOUT(!keys->isPending(), 2000);
        QVERIFY(!popup->isVisible());
        press(Qt::Key_G);
        QVERIFY(keys->isPending());
        QCOMPARE(items->currentIndex().row(), 5);
        press(Qt::Key_Escape);
        QVERIFY(!keys->isPending());

        // With no timeout, a sequence waits for its next key.
        Settings::setKeySequenceTimeout(0);
        window.applySettings();
        QCOMPARE(keys->pendingTimeout(), 0);
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        press(Qt::Key_G);
        QTest::qWait(YaziKeys::DefaultPendingTimeout + 200);
        QVERIFY(keys->isPending());
        QVERIFY(!popup->isVisible());
        press(Qt::Key_G);
        QCOMPARE(items->currentIndex().row(), 0);

        // Hints shown at once.
        Settings::setShortcutHints(true);
        Settings::setShortcutHintDelay(0);
        window.applySettings();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        press(Qt::Key_G);
        QTRY_VERIFY_WITH_TIMEOUT(popup->isShowing(ShortcutHintPopup::Source::Keys), 200);
        // Turning them off hides them.
        Settings::setShortcutHints(false);
        window.applySettings();
        QVERIFY(!popup->isVisible());
        press(Qt::Key_Escape);
        QVERIFY(!keys->isPending());
    }

    void cancelsWindowSequencesThatTimeOut()
    {
        Settings::setShortcutHints(false);
        Settings::setKeySequenceTimeout(300);
        MainWindow window;
        showWithFocusOnItems(window);
        if (QTest::currentTestFailed()) {
            return;
        }
        QAction *action = window.actionCollection()->addAction(u"test_sequence"_s);
        action->setText(u"Test Sequence"_s);
        action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_T));
        window.addAction(action);
        QSignalSpy triggered(action, &QAction::triggered);
        WindowSequenceHints *hints = window.windowSequenceHints();
        ShortcutHintPopup *popup = window.activePane()->shortcutHintPopup();

        // Typed in time, the shortcut runs.
        press(Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(hints->isPending());
        press(Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(triggered.size(), 1);

        // Too late, the sequence is cancelled, in Qt's shortcut map too, and the next key doesn't complete it.
        press(Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(hints->isPending());
        QTRY_VERIFY_WITH_TIMEOUT(!hints->isPending(), 2000);
        QVERIFY(!popup->isVisible());
        press(Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(triggered.size(), 1);

        // While its hints show, a sequence waits.
        Settings::setShortcutHints(true);
        Settings::setShortcutHintDelay(0);
        window.applySettings();
        window.activePane()->view()->currentView()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(window.activePane()->view()->currentView()->hasFocus());
        press(Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(popup->isShowing(ShortcutHintPopup::Source::Window));
        QTest::qWait(600);
        QVERIFY(hints->isPending());
        press(Qt::Key_T, Qt::ControlModifier);
        QCOMPARE(triggered.size(), 2);
        QVERIFY(!popup->isVisible());
    }

    void showsTheShortcutReference()
    {
        MainWindow window;
        showWithFocusOnItems(window, QSize(700, 450));
        if (QTest::currentTestFailed()) {
            return;
        }
        QAction *reference = window.actionCollection()->action(u"show_shortcut_hints"_s);
        QVERIFY(reference);
        // No default shortcut, so that it takes no one's key.
        QVERIFY(reference->shortcuts().isEmpty());
        QCOMPARE(reference->text(), u"Shortcut Hints"_s);
        ShortcutReference *shortcutReference = window.shortcutReference();
        QVERIFY(shortcutReference);
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        const int rows = items->model()->rowCount(items->rootIndex());
        items->setCurrentIndex(items->model()->index(5, 0, items->rootIndex()));
        ShortcutHintPopup *popup = pane->shortcutHintPopup();

        // The window's shortcuts that work in a file view, in pages the paging keys turn without reaching the view.
        reference->trigger();
        QVERIFY(shortcutReference->isShowing());
        QVERIFY(popup->isShowing(ShortcutHintPopup::Source::Reference));
        QVERIFY(items->hasFocus());
        QCOMPARE(popup->title(), u"Shortcuts: File views and lists"_s);
        const QString quit = QKeySequence(Qt::CTRL | Qt::Key_Q).toString(QKeySequence::NativeText);
        QVERIFY(hasHint(popup->hints(), quit));
        QVERIFY2(popup->pageCount() > 1, qPrintable(QString::number(popup->pageCount())));
        press(Qt::Key_PageDown);
        QCOMPARE(popup->page(), 1);
        press(Qt::Key_Up);
        QCOMPARE(popup->page(), 0);
        QCOMPARE(items->currentIndex().row(), 5);
        // Escape closes it, and does nothing else.
        press(Qt::Key_Escape);
        QVERIFY(!popup->isVisible());
        QVERIFY(!shortcutReference->isShowing());

        // The action toggles it.
        reference->trigger();
        QVERIFY(popup->isVisible());
        reference->trigger();
        QVERIFY(!popup->isVisible());

        // Any other key closes it and does what it does.
        reference->trigger();
        QVERIFY(popup->isVisible());
        press(Qt::Key_End);
        QVERIFY(!popup->isVisible());
        QCOMPARE(items->currentIndex().row(), rows - 1);

        // With Vim-style navigation, its keys come first.
        Settings::setVimNavigation(true);
        window.applySettings();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        reference->trigger();
        QCOMPARE(popup->title(), u"Shortcuts: Vim-style navigation"_s);
        const auto indexOf = [popup](const QString &keys) {
            const QList<ShortcutHints::Hint> hints = popup->hints();
            const auto found = std::find_if(hints.cbegin(), hints.cend(), [&keys](const ShortcutHints::Hint &hint) {
                return hint.keys == keys;
            });
            return found == hints.cend() ? -1 : int(found - hints.cbegin());
        };
        QVERIFY(indexOf(u"gg"_s) >= 0);
        QVERIFY(indexOf(quit) > indexOf(u"gg"_s));
        // A Vim key closes it and begins its sequence.
        press(Qt::Key_G);
        QVERIFY(!popup->isShowing(ShortcutHintPopup::Source::Reference));
        QVERIFY(pane->vimKeys()->isPending());
        press(Qt::Key_Escape);

        // In a text field, every key keeps typing.
        auto *field = new QLineEdit(&window);
        field->show();
        field->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(field->hasFocus());
        reference->trigger();
        QVERIFY(popup->isShowing(ShortcutHintPopup::Source::Reference));
        QCOMPARE(popup->title(), u"Shortcuts: Text fields"_s);
        press(Qt::Key_A);
        QCOMPARE(field->text(), u"a"_s);
        QVERIFY(!popup->isVisible());
        reference->trigger();
        press(Qt::Key_Escape);
        QVERIFY(!popup->isVisible());

        // Moving the focus closes it.
        reference->trigger();
        QVERIFY(popup->isVisible());
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(!popup->isVisible());
    }

private:
    void showWithFocusOnItems(MainWindow &window, QSize size = QSize(1400, 600))
    {
        window.resize(size);
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        Pane *pane = window.activePane();
        QTRY_VERIFY(!pane->view()->isLoading());
        QAbstractItemView *items = pane->view()->currentView();
        QTRY_VERIFY(items->model()->rowCount(items->rootIndex()) > 5);
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
    }

    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;
};

QTEST_MAIN(ShortcutHintsUiTest)

#include "shortcuthintsuitest.moc"
