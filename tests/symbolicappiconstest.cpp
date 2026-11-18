// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"
#include "mainwindow.h"
#include "symbolicactionicons.h"
#include "symbolicicon.h"
#include "testicontheme.h"

#include <KFilePlacesModel>
#include <KFilePlacesView>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QMenu>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace TestIconTheme;

class SymbolicAppIconsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_themeDir.isValid());
        QVERIFY(
            install(m_themeDir.path(), {}, {QStringLiteral("foo"), QStringLiteral("folder"), QStringLiteral("inode-directory"), QStringLiteral("user-home")}));
        qApp->installEventFilter(new SymbolicActionIcons(qApp));
    }

    void menuActionsUseSymbolicIcons()
    {
        QMenu menu;
        QAction *action = menu.addAction(QIcon::fromTheme(QStringLiteral("foo")), QStringLiteral("Foo"));
        QCOMPARE(colorAt(action->icon(), 16), SymbolicColor);
        QCOMPARE(colorAt(action->icon(), 48), RegularColor);
    }

    void changedActionIconsUseSymbolicIcons()
    {
        QMenu menu;
        QAction *action = menu.addAction(QStringLiteral("Foo"));
        action->setIcon(QIcon::fromTheme(QStringLiteral("foo")));
        QCOMPARE(colorAt(action->icon(), 16), SymbolicColor);
    }

    void folderViewDecorationsUseSymbolicIcons()
    {
        QTemporaryDir root;
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("sub")));

        FolderView view;
        view.setPreviewsEnabled(false);
        view.setUrl(QUrl::fromLocalFile(root.path()));
        QTRY_COMPARE(view.itemCount(), 1);

        const QAbstractItemModel *model = view.currentView()->model();
        const QIcon icon = model->data(model->index(0, 0), Qt::DecorationRole).value<QIcon>();
        QCOMPARE(colorAt(icon, 16), SymbolicColor);
        QVERIFY(colorAt(icon, 64) != SymbolicColor);
    }

    void placesUseSymbolicIcons()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        MainWindow window;
        auto *places = window.findChild<KFilePlacesView *>();
        QVERIFY(places);
        const auto *model = qobject_cast<KFilePlacesModel *>(places->model());
        QVERIFY(model);

        const QModelIndex home = model->closestItem(QUrl::fromLocalFile(QDir::homePath()));
        QVERIFY(home.isValid());
        QCOMPARE(model->icon(home).name(), QStringLiteral("user-home"));
        QCOMPARE(colorAt(model->icon(home), 16), SymbolicColor);
    }

private:
    QTemporaryDir m_themeDir;
};

QTEST_MAIN(SymbolicAppIconsTest)

#include "symbolicappiconstest.moc"
