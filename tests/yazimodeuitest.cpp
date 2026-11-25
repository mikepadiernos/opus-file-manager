// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "session.h"
#include "settings.h"
#include "tabmodel.h"
#include "workspacedialog.h"
#include "yazicolumns.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KDirModel>
#include <KSharedConfig>

#include <QAbstractItemModel>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <algorithm>

namespace
{
constexpr int ManyFiles = 200;

// Clicks the button of the next message box once it opens.
void answerMessageBox(QMessageBox::StandardButton button, int attempts = 200)
{
    QTimer::singleShot(10, [button, attempts] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); box && box->button(button)) {
            box->button(button)->click();
        } else if (attempts > 0) {
            answerMessageBox(button, attempts - 1);
        }
    });
}

QModelIndex rowNamed(const QAbstractItemView *view, const QString &name)
{
    const QAbstractItemModel *model = view->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data().toString() == name) {
            return index;
        }
    }
    return {};
}

QList<QUrl> selectedUrls(const FolderView *view)
{
    QList<QUrl> urls;
    for (const KFileItem &item : view->selectedItems()) {
        urls.append(item.url());
    }
    std::sort(urls.begin(), urls.end());
    return urls;
}

QUrl stripped(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash);
}
}

class YaziModeUiTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        const QDir root(m_root.path());
        for (const char *path : {"a/sub", "a/other", "a/.hidden", "b", "many"}) {
            QVERIFY(root.mkpath(QLatin1String(path)));
        }
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        for (int i = 0; i < ManyFiles; ++i) {
            QFile entry(m_root.filePath(QStringLiteral("many/file%1.txt").arg(i, 3, 10, QLatin1Char('0'))));
            QVERIFY(entry.open(QIODevice::WriteOnly));
            entry.write(QByteArray(i + 1, 'x'));
            entry.close();
        }
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
        m_sub = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub")));
        m_other = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/other")));
        m_file = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        m_many = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("many")));
    }

    void init()
    {
        Settings::setConfirmClosingMultipleTabs(false);
        Settings::setStartupMode(Settings::EnumStartupMode::RestoreSession);
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
    }

    void isOffByDefaultAndTogglesEachPane()
    {
        MainWindow window;
        window.openUrls({m_sub, m_b});
        Pane *left = window.activePane();
        Pane *right = window.inactivePane();
        QVERIFY(!left->yaziMode());
        QVERIFY(!right->yaziMode());
        QVERIFY(!left->saveState().yaziMode);
        QVERIFY(left->yaziColumns());
        QVERIFY(left->yaziColumns()->isHidden());
        QCOMPARE(left->view()->parentWidget(), left);

        QSignalSpy changed(left, &Pane::yaziModeChanged);
        QSignalSpy layoutChanged(left, &Pane::workspaceLayoutChanged);
        left->setYaziMode(true);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().first().toBool(), true);
        QVERIFY(layoutChanged.count() >= 1);
        QVERIFY(left->yaziMode());
        QVERIFY(!left->yaziColumns()->isHidden());
        QCOMPARE(left->yaziColumns()->currentView(), left->view());
        QCOMPARE(left->yaziColumns()->splitter()->indexOf(left->view()), 1);
        QCOMPARE(left->yaziColumns()->splitter()->indexOf(left->yaziColumns()->parentColumn()), 0);
        QCOMPARE(left->yaziColumns()->splitter()->indexOf(left->yaziColumns()->previewSlot()), 2);

        QVERIFY(!right->yaziMode());
        QVERIFY(right->yaziColumns()->isHidden());
        QCOMPARE(right->view()->parentWidget(), right);

        left->setYaziMode(true);
        QCOMPARE(changed.count(), 1);
        left->setYaziMode(false);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.last().first().toBool(), false);
        QVERIFY(!left->yaziMode());
        QVERIFY(left->yaziColumns()->isHidden());
        QVERIFY(!left->yaziColumns()->currentView());
        QCOMPARE(left->view()->parentWidget(), left);
        QVERIFY(left->yaziColumns()->url().isEmpty());
    }

    void keepsThePaneStateWhenLeaving_data()
    {
        QTest::addColumn<FolderView::ViewMode>("mode");
        QTest::newRow("details") << FolderView::ViewMode::Details;
        QTest::newRow("compact") << FolderView::ViewMode::Compact;
        QTest::newRow("icons") << FolderView::ViewMode::Icons;
    }

    void keepsThePaneStateWhenLeaving()
    {
        QFETCH(FolderView::ViewMode, mode);
        MainWindow window;
        window.resize(900, 500);
        window.openUrls({m_many, m_b});
        Pane *pane = window.activePane();
        const int openTabs = pane->tabs()->count();
        pane->openInNewTab(m_a, false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = pane->view();
        view->setViewMode(mode);
        QTRY_COMPARE(view->itemCount(), ManyFiles);
        QTRY_VERIFY(!view->isLoading());
        view->setSorting(KDirModel::Size, Qt::DescendingOrder);
        PaneSession filtered = pane->saveState();
        filtered.filterText = QStringLiteral("file");
        pane->replaceState(filtered);
        const QUrl third = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("many/file003.txt")));
        const QUrl seventh = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("many/file007.txt")));
        view->selectUrls({third, seventh});
        QTRY_COMPARE(selectedUrls(view).size(), 2);
        QScrollBar *scroll = mode == FolderView::ViewMode::Compact ? view->currentView()->horizontalScrollBar() : view->currentView()->verticalScrollBar();
        QTRY_VERIFY(scroll->maximum() > 0);
        scroll->setValue(scroll->maximum() / 2);
        QAbstractItemView *presentation = view->currentView();

        const PaneSession before = pane->saveState();
        QCOMPARE(before.tabs.size(), openTabs + 1);
        QCOMPARE(before.filterText, QStringLiteral("file"));
        const QList<QUrl> selection = selectedUrls(view);
        const int scrolled = scroll->value();
        QVERIFY(scrolled > 0);
        const NameFilter filter = view->nameFilter();
        QSignalSpy viewModeChanged(view, &FolderView::viewModeChanged);
        QSignalSpy urlChanged(pane, &Pane::urlChanged);

        pane->setYaziMode(true);
        QVERIFY(pane->yaziColumns()->isVisible());
        QVERIFY(view->isVisible());
        QCOMPARE(pane->view(), view);
        QCOMPARE(view->currentView(), presentation);
        QCOMPARE(view->viewMode(), mode);
        QCOMPARE(selectedUrls(view), selection);
        PaneSession during = pane->saveState();
        QVERIFY(during.yaziMode);
        during.yaziMode = false;
        QCOMPARE(during, before);

        pane->setYaziMode(false);
        QVERIFY(view->isVisible());
        QVERIFY(!pane->yaziColumns()->isVisible());
        QCOMPARE(pane->saveState(), before);
        QCOMPARE(pane->url(), m_many);
        QCOMPARE(pane->tabs()->urls(), before.tabs);
        QCOMPARE(view->url(), m_many);
        QCOMPARE(view->viewMode(), mode);
        QCOMPARE(view->currentView(), presentation);
        QCOMPARE(view->sortColumn(), int(KDirModel::Size));
        QCOMPARE(view->sortOrder(), Qt::DescendingOrder);
        QCOMPARE(view->nameFilter().text(), filter.text());
        QCOMPARE(view->nameFilter().scope(), filter.scope());
        QCOMPARE(selectedUrls(view), selection);
        QCOMPARE(scroll->value(), scrolled);
        QCOMPARE(viewModeChanged.count(), 0);
        QCOMPARE(urlChanged.count(), 0);
    }

    void parentColumnListsAndNavigates()
    {
        MainWindow window;
        window.resize(900, 500);
        window.openUrls({m_sub, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Pane *pane = window.activePane();
        YaziColumns *columns = pane->yaziColumns();
        QListView *parent = columns->parentColumn();
        QVERIFY(!columns->modeIndicator()->isVisible());

        pane->setYaziMode(true);
        QCOMPARE(stripped(columns->url()), m_sub);
        QCOMPARE(stripped(columns->parentUrl()), m_a);
        QVERIFY(parent->isVisible());
        QCOMPARE(parent->accessibleName(), QStringLiteral("Parent folder"));
        QVERIFY(parent->editTriggers() == QAbstractItemView::NoEditTriggers);
        QTRY_COMPARE(parent->model()->rowCount(), 3);
        QCOMPARE(parent->model()->index(0, 0).data().toString(), QStringLiteral("other"));
        QCOMPARE(parent->model()->index(1, 0).data().toString(), QStringLiteral("sub"));
        QCOMPARE(parent->model()->index(2, 0).data().toString(), QStringLiteral("file.txt"));
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("sub"));
        QVERIFY(parent->selectionModel()->isSelected(parent->currentIndex()));

        QLabel *indicator = columns->modeIndicator();
        QVERIFY(indicator->isVisible());
        QCOMPARE(indicator->accessibleName(), QStringLiteral("Yazi mode"));
        QVERIFY(!indicator->text().isEmpty());
        QVERIFY(columns->previewSlot()->isHidden());

        pane->view()->setShowHiddenFiles(true);
        QTRY_COMPARE(parent->model()->rowCount(), 4);
        QVERIFY(rowNamed(parent, QStringLiteral(".hidden")).isValid());
        pane->view()->setShowHiddenFiles(false);
        QTRY_COMPARE(parent->model()->rowCount(), 3);

        // A click on a sibling folder opens it, once, in the pane.
        const int openTabs = pane->tabs()->count();
        QSignalSpy urlChanged(pane, &Pane::urlChanged);
        const QModelIndex other = rowNamed(parent, QStringLiteral("other"));
        QVERIFY(other.isValid());
        QTRY_VERIFY(!parent->visualRect(other).isEmpty());
        QTest::mouseClick(parent->viewport(), Qt::LeftButton, {}, parent->visualRect(other).center());
        QTRY_COMPARE(pane->url(), m_other);
        QTest::qWait(50);
        QCOMPARE(urlChanged.count(), 1);
        QCOMPARE(stripped(columns->url()), m_other);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("other"));
        QCOMPARE(pane->tabs()->count(), openTabs);

        // A click on a file opens the parent folder with the file selected.
        QTRY_VERIFY(rowNamed(parent, QStringLiteral("file.txt")).isValid());
        const QModelIndex file = rowNamed(parent, QStringLiteral("file.txt"));
        QTest::mouseClick(parent->viewport(), Qt::LeftButton, {}, parent->visualRect(file).center());
        QTRY_COMPARE(pane->url(), m_a);
        QTRY_COMPARE(selectedUrls(pane->view()), QList<QUrl>{m_file});
        QTRY_COMPARE(stripped(columns->parentUrl()), QUrl::fromLocalFile(m_root.path()));
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("a"));

        // Return on a folder in the parent column opens it.
        QTRY_VERIFY(rowNamed(parent, QStringLiteral("b")).isValid());
        parent->setFocus();
        parent->setCurrentIndex(rowNamed(parent, QStringLiteral("b")));
        QCOMPARE(pane->url(), m_a);
        QTest::keyClick(parent, Qt::Key_Return);
        QTRY_COMPARE(pane->url(), m_b);
        QCOMPARE(pane->yaziMode(), true);

        // Other navigation keeps the parent column in step.
        pane->setUrl(m_sub);
        QTRY_COMPARE(stripped(columns->parentUrl()), m_a);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("sub"));
        pane->goUp();
        QTRY_COMPARE(pane->url(), m_a);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("a"));

        // A root has no parent to list.
        pane->setUrl(QUrl::fromLocalFile(QStringLiteral("/")));
        QTRY_COMPARE(parent->model()->rowCount(), 0);
        QVERIFY(columns->parentUrl().isEmpty());

        pane->setYaziMode(false);
        QVERIFY(!indicator->isVisible());
        QCOMPARE(parent->model()->rowCount(), 0);
    }

    void previewSlotShowsAndDropsItsWidget()
    {
        YaziColumns columns;
        QVERIFY(columns.previewSlot()->isHidden());
        QVERIFY(!columns.previewWidget());
        QPointer<QLabel> first = new QLabel(QStringLiteral("first"));
        columns.setPreviewWidget(first);
        QCOMPARE(columns.previewWidget(), first.data());
        QVERIFY(!columns.previewSlot()->isHidden());
        QVERIFY(columns.previewSlot()->isAncestorOf(first));
        columns.setPreviewWidget(new QLabel(QStringLiteral("second")));
        QVERIFY(!first);
        columns.setPreviewWidget(nullptr);
        QVERIFY(!columns.previewWidget());
        QVERIFY(columns.previewSlot()->isHidden());

        auto *extra = new QLabel(QStringLiteral("extra"));
        columns.addStatusWidget(extra);
        QCOMPARE(extra->parentWidget(), columns.modeIndicator()->parentWidget());
    }

    void actionTracksTheActivePane()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        QAction *action = window.actionCollection()->action(QStringLiteral("toggle_yazi_mode"));
        QVERIFY(action);
        QVERIFY(action->isCheckable());
        QVERIFY(!action->isChecked());
        QVERIFY(action->shortcuts().isEmpty());
        QVERIFY(!action->icon().isNull() || QIcon::themeName().isEmpty());
        auto *viewMenu = window.findChild<QMenu *>(QStringLiteral("view"));
        QVERIFY(viewMenu);
        QVERIFY(viewMenu->actions().contains(action));

        Pane *first = window.activePane();
        Pane *second = window.inactivePane();
        action->trigger();
        QVERIFY(first->yaziMode());
        QVERIFY(!second->yaziMode());
        QVERIFY(action->isChecked());

        Q_EMIT second->activated();
        QCOMPARE(window.activePane(), second);
        QVERIFY(!action->isChecked());
        action->trigger();
        QVERIFY(second->yaziMode());
        QVERIFY(first->yaziMode());
        QVERIFY(action->isChecked());

        Q_EMIT first->activated();
        QVERIFY(action->isChecked());
        first->setYaziMode(false);
        QVERIFY(!action->isChecked());
        QVERIFY(second->yaziMode());
        second->setYaziMode(false);
        QVERIFY(!action->isChecked());
        Q_EMIT second->activated();
        QVERIFY(!action->isChecked());
        action->trigger();
        QVERIFY(second->yaziMode());
        QVERIFY(!first->yaziMode());
        action->trigger();
        QVERIFY(!second->yaziMode());
        QVERIFY(!action->isChecked());
    }

    void sessionRestoresTheMode()
    {
        {
            MainWindow window;
            window.openUrls({m_sub, m_b});
            window.inactivePane()->setYaziMode(true);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QVERIFY(window.close());
        }
        const WindowSession saved = Session::load(KSharedConfig::openStateConfig()->group(QStringLiteral("Session")));
        QVERIFY(!saved.left.yaziMode);
        QVERIFY(saved.right.yaziMode);

        MainWindow restored;
        restored.openUrls({});
        Pane *active = restored.activePane();
        Pane *inactive = restored.inactivePane();
        QVERIFY(!active->yaziMode());
        QVERIFY(inactive->yaziMode());
        QCOMPARE(inactive->yaziColumns()->currentView(), inactive->view());
        QCOMPARE(stripped(inactive->url()), m_b);
        QCOMPARE(stripped(inactive->yaziColumns()->url()), m_b);
        QVERIFY(!restored.actionCollection()->action(QStringLiteral("toggle_yazi_mode"))->isChecked());
        Q_EMIT inactive->activated();
        QVERIFY(restored.actionCollection()->action(QStringLiteral("toggle_yazi_mode"))->isChecked());
    }

    void replacingStateAppliesTheMode()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        Pane *pane = window.activePane();
        PaneSession state = pane->saveState();
        state.yaziMode = true;
        pane->replaceState(state);
        QVERIFY(pane->yaziMode());
        QCOMPARE(pane->saveState(), state);
        state.yaziMode = false;
        pane->replaceState(state);
        QVERIFY(!pane->yaziMode());
        QCOMPARE(pane->saveState(), state);
    }

    void workspaceLayoutRestoresTheMode()
    {
        const auto config = KSharedConfig::openConfig();
        KConfigGroup group = config->group(QStringLiteral("Workspaces"));
        const bool hadCatalog = group.hasKey("Catalog");
        const QByteArray previousCatalog = group.readEntry("Catalog", QByteArray());
        group.deleteEntry("Catalog");
        QVERIFY(group.sync());
        const bool restoreViews = Settings::workspaceRestoreViews();
        const auto restore = qScopeGuard([config, hadCatalog, previousCatalog, restoreViews] {
            Settings::setWorkspaceRestoreViews(restoreViews);
            config->reparseConfiguration();
            KConfigGroup saved = config->group(QStringLiteral("Workspaces"));
            if (hadCatalog) {
                saved.writeEntry("Catalog", previousCatalog);
            } else {
                saved.deleteEntry("Catalog");
            }
            saved.sync();
        });
        Settings::setWorkspaceRestoreViews(true);

        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Pane *pane = window.activePane();
        pane->setYaziMode(true);

        window.actionCollection()->action(QStringLiteral("manage_workspaces"))->trigger();
        auto *dialog = window.findChild<WorkspaceDialog *>(QStringLiteral("workspaceDialog"));
        QVERIFY(dialog);
        dialog->findChild<QLineEdit *>(QStringLiteral("workspaceName"))->setText(QStringLiteral("Columns"));
        dialog->findChild<QLineEdit *>(QStringLiteral("workspaceShortLabel"))->setText(QStringLiteral("C"));
        dialog->findChild<QPushButton *>(QStringLiteral("workspaceCreate"))->click();
        answerMessageBox(QMessageBox::Yes);
        dialog->findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();
        dialog->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QAction *selector = window.actionCollection()->action(QStringLiteral("workspace_selector"));
        QAction *restoreLayout = window.actionCollection()->action(QStringLiteral("restore_workspace_layout"));
        QVERIFY(selector && restoreLayout);
        QTRY_VERIFY(restoreLayout->isEnabled());
        QTRY_VERIFY(selector->text().contains(QStringLiteral("Columns")));
        QTRY_VERIFY(!selector->text().endsWith(QStringLiteral(" *")));
        QVERIFY(pane->yaziMode());

        window.actionCollection()->action(QStringLiteral("toggle_yazi_mode"))->trigger();
        QVERIFY(!pane->yaziMode());
        QTRY_VERIFY(selector->text().endsWith(QStringLiteral(" *")));

        answerMessageBox(QMessageBox::Yes);
        restoreLayout->trigger();
        QTRY_VERIFY(pane->yaziMode());
        QVERIFY(!window.inactivePane()->yaziMode());
        QVERIFY(window.actionCollection()->action(QStringLiteral("toggle_yazi_mode"))->isChecked());
        QTRY_VERIFY(!selector->text().endsWith(QStringLiteral(" *")));
    }

private:
    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;
    QUrl m_sub;
    QUrl m_other;
    QUrl m_file;
    QUrl m_many;
};

QTEST_MAIN(YaziModeUiTest)

#include "yazimodeuitest.moc"
