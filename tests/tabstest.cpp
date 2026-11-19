// SPDX-License-Identifier: GPL-3.0-or-later

#include "labelcolor.h"
#include "mainwindow.h"
#include "pane.h"
#include "tabcolors.h"
#include "tabmodel.h"

#include <KSharedConfig>

#include <QDir>
#include <QStandardPaths>
#include <QStyle>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>

class TabsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;
    QUrl m_c;

    /// The active pane, showing exactly @p urls as tabs.
    static Pane *paneWith(MainWindow &window, const QList<QUrl> &urls)
    {
        window.openUrls({urls.first()});
        Pane *pane = window.activePane();
        while (pane->tabs()->count() > 1) {
            pane->tabs()->closeTab(0);
        }
        for (int i = 1; i < urls.size(); ++i) {
            pane->openInNewTab(urls.at(i));
        }
        return pane;
    }

    static QTabBar *tabBarOf(Pane *pane)
    {
        return pane->findChild<QTabBar *>();
    }

    static QWidget *closeButton(QTabBar *bar, int index)
    {
        const auto side = static_cast<QTabBar::ButtonPosition>(bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
        return bar->tabButton(index, side);
    }

    static QStringList tabTexts(QTabBar *bar)
    {
        QStringList texts;
        for (int i = 0; i < bar->count(); ++i) {
            texts.append(bar->tabText(i).remove(u'&'));
        }
        return texts;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        for (const char *name : {"a", "b", "c"}) {
            QVERIFY(QDir(m_root.path()).mkpath(QLatin1String(name)));
        }
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
        m_c = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("c")));
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("TabColors"));
        KSharedConfig::openConfig()->sync();
    }

    void pinnedAndLockedTabsShowStateAndNoCloseButton()
    {
        MainWindow window;
        Pane *pane = paneWith(window, {m_a, m_b, m_c});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        pane->tabs()->setPinned(0, true);
        pane->tabs()->setLocked(1, true);

        QTabBar *bar = tabBarOf(pane);
        QCOMPARE(bar->tabIcon(0).name(), QStringLiteral("pin"));
        QCOMPARE(bar->tabIcon(1).name(), QStringLiteral("object-locked"));
        QVERIFY(!closeButton(bar, 0)->isVisible());
        QVERIFY(!closeButton(bar, 1)->isVisible());
        QVERIFY(closeButton(bar, 2)->isVisible());

        pane->tabs()->setLocked(1, false);
        QVERIFY(closeButton(bar, 1)->isVisible());
    }

    void lockedTabOpensFoldersInNewTabs()
    {
        MainWindow window;
        Pane *pane = paneWith(window, {m_a});
        pane->tabs()->setLocked(0, true);
        pane->setUrl(m_b);
        QCOMPARE(pane->tabs()->urls(), (QList<QUrl>{m_a, m_b}));
        QCOMPARE(pane->url(), m_b);
    }

    void customNameReplacesTheFolderName()
    {
        MainWindow window;
        Pane *pane = paneWith(window, {m_a});
        pane->tabs()->setName(0, QStringLiteral("Work"));
        QCOMPARE(tabBarOf(pane)->tabText(0), QStringLiteral("Work"));
        pane->tabs()->setName(0, QString());
        QCOMPARE(tabBarOf(pane)->tabText(0), QStringLiteral("a"));
    }

    void colourAppliesToEveryTabOfTheFolder()
    {
        MainWindow window;
        window.openUrls({m_a, m_a});
        const int active = window.activePane()->tabs()->urls().indexOf(m_a);
        const int inactive = window.inactivePane()->tabs()->urls().indexOf(m_a);
        window.tabColors()->setColor(m_a, LabelColor::Green);
        QCOMPARE(tabBarOf(window.activePane())->tabTextColor(active), LabelColors::rgb(LabelColor::Green));
        QCOMPARE(tabBarOf(window.inactivePane())->tabTextColor(inactive), LabelColors::rgb(LabelColor::Green));
        window.tabColors()->setColor(m_a, LabelColor::None);
        QVERIFY(!tabBarOf(window.activePane())->tabTextColor(active).isValid());
    }

    void draggingAcrossThePinnedBoundaryIsUndone()
    {
        MainWindow window;
        Pane *pane = paneWith(window, {m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        pane->tabs()->setPinned(0, true);
        QTabBar *bar = tabBarOf(pane);
        bar->moveTab(1, 0);
        QCOMPARE(pane->tabs()->urls(), (QList<QUrl>{m_a, m_b}));
        QCOMPARE(tabTexts(bar), QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    }

    void sessionKeepsPinsLocksAndNames()
    {
        {
            MainWindow window;
            Pane *pane = paneWith(window, {m_a, m_b, m_c});
            pane->tabs()->setPinned(2, true);
            pane->tabs()->setLocked(1, true);
            pane->tabs()->setName(2, QStringLiteral("Bee"));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QVERIFY(window.close());
        }
        MainWindow window;
        window.openUrls({});
        TabModel *tabs = window.activePane()->tabs();
        QCOMPARE(tabs->urls(), (QList<QUrl>{m_c, m_a, m_b}));
        QCOMPARE(tabs->pinnedCount(), 1);
        QVERIFY(tabs->isLocked(1));
        QCOMPARE(tabs->nameAt(2), QStringLiteral("Bee"));
    }
};

QTEST_MAIN(TabsTest)

#include "tabstest.moc"
