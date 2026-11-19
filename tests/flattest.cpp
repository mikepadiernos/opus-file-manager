// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KDirModel>
#include <KSharedConfig>

#include <QAbstractItemView>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QMimeData>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class FlatTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QUrl m_flat;

    void touch(const QString &relative)
    {
        QFile file(m_root.filePath(relative));
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    static QStringList visibleNames(FolderView *view)
    {
        QStringList names;
        const QAbstractItemModel *model = view->currentView()->model();
        for (int row = 0; row < model->rowCount(); ++row) {
            names.append(model->index(row, KDirModel::Name).data().toString());
        }
        names.sort();
        return names;
    }

    static QString relativePathOf(FolderView *view, const QString &name)
    {
        const QAbstractItemModel *model = view->currentView()->model();
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->index(row, KDirModel::Name).data().toString() == name) {
                return model->index(row, KDirModel::Type).data().toString();
            }
        }
        return QStringLiteral("<missing>");
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("flat/x/y")));
        touch(QStringLiteral("flat/top.txt"));
        touch(QStringLiteral("flat/x/mid.txt"));
        touch(QStringLiteral("flat/x/y/deep.txt"));
        m_flat = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("flat")));
    }

    void init()
    {
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Flat"));
        KSharedConfig::openStateConfig()->sync();
    }

    void flatListsEveryFileWithoutFolders()
    {
        FolderView view;
        view.setUrl(m_flat);
        QTRY_COMPARE(view.itemCount(), 2);
        view.setFlat(true);
        QVERIFY(view.isFlat());
        QCOMPARE(view.url(), m_flat);
        QTRY_COMPARE(visibleNames(&view), QStringList({QStringLiteral("deep.txt"), QStringLiteral("mid.txt"), QStringLiteral("top.txt")}));

        QCOMPARE(relativePathOf(&view, QStringLiteral("deep.txt")), QStringLiteral("x/y"));
        QCOMPARE(relativePathOf(&view, QStringLiteral("top.txt")), QString());
        QCOMPARE(view.currentView()->model()->headerData(KDirModel::Type, Qt::Horizontal).toString(), QStringLiteral("Relative Path"));

        view.selectUrls({QUrl::fromLocalFile(m_root.filePath(QStringLiteral("flat/x/y/deep.txt")))});
        QCOMPARE(view.selectedItems().size(), 1);
        QCOMPARE(view.selectedItems().first().url(), QUrl::fromLocalFile(m_root.filePath(QStringLiteral("flat/x/y/deep.txt"))));

        view.clearSelection();
        PathFilter filter;
        filter.maxDepth = 1;
        filter.excludedFolders = {m_root.filePath(QStringLiteral("flat/x"))};
        QString error;
        QCOMPARE(view.applySelectionPathFilter(filter, true, &error), 1);
        QVERIFY(error.isEmpty());
        QCOMPARE(view.selectedItems().first().name(), QStringLiteral("top.txt"));
        filter.excludedFolders.clear();
        filter.parentText = QStringLiteral("/x");
        QCOMPARE(view.applySelectionPathFilter(filter, true, &error), 1);
        QCOMPARE(view.selectedItems().size(), 2);
        QCOMPARE(view.applySelectionPathFilter(filter, false, &error), 1);
        QCOMPARE(view.selectedItems().size(), 1);
        filter.excludedFolders = {QStringLiteral("relative")};
        QCOMPARE(view.applySelectionPathFilter(filter, false, &error), -1);
        QVERIFY(!error.isEmpty());
        QCOMPARE(view.selectedItems().size(), 1);

        view.setFlat(false);
        QTRY_COMPARE(visibleNames(&view), QStringList({QStringLiteral("top.txt"), QStringLiteral("x")}));
        QCOMPARE(view.currentView()->model()->headerData(KDirModel::Type, Qt::Horizontal).toString(), QStringLiteral("Type"));
    }

    void flatListRefusesDrops()
    {
        FolderView view;
        view.setUrl(m_flat);
        view.setFlat(true);
        view.resize(500, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(m_root.filePath(QStringLiteral("flat/top.txt")))});
        QDragEnterEvent enter(QPoint(10, 250), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view.currentView()->viewport(), &enter);
        QVERIFY(!enter.isAccepted());
    }

    void showFlatIsRememberedPerLocationAndBlocksCreating()
    {
        MainWindow window;
        window.openUrls({m_flat});
        QAction *showFlat = window.actionCollection()->action(QStringLiteral("show_flat"));
        QVERIFY(showFlat);
        showFlat->trigger();
        FolderView *view = window.activePane()->view();
        QVERIFY(view->isFlat());
        QVERIFY(!window.actionCollection()->action(QStringLiteral("new_folder"))->isEnabled());

        window.activePane()->setUrl(QUrl::fromLocalFile(m_root.path()));
        QVERIFY(!view->isFlat());
        QVERIFY(!showFlat->isChecked());
        QVERIFY(window.actionCollection()->action(QStringLiteral("new_folder"))->isEnabled());

        window.activePane()->goBack();
        QCOMPARE(window.activePane()->url(), m_flat);
        QVERIFY(view->isFlat());
        QVERIFY(showFlat->isChecked());
    }
};

QTEST_MAIN(FlatTest)

#include "flattest.moc"
