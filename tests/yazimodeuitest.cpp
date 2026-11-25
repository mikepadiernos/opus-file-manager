// SPDX-License-Identifier: GPL-3.0-or-later

#include "filterbar.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "previewpanel.h"
#include "session.h"
#include "settings.h"
#include "tabmodel.h"
#include "workspacedialog.h"
#include "yazicolumns.h"
#include "yazikeys.h"
#include "yazipreview.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KDirModel>
#include <KIO/FileUndoManager>
#include <KSharedConfig>

#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
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
constexpr int ManyFolders = 60;

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

// The names of the selected rows of @p view, sorted.
QStringList selectedNames(const QAbstractItemView *view)
{
    QStringList names;
    const QModelIndexList rows = view->selectionModel()->selectedRows();
    for (const QModelIndex &row : rows) {
        names.append(row.data().toString());
    }
    names.sort();
    return names;
}

// A visible dialog other than @p window, such as a confirmation or a conflict dialog.
QDialog *openDialog(const QWidget *window)
{
    const QList<QWidget *> widgets = QApplication::topLevelWidgets();
    for (QWidget *widget : widgets) {
        auto *dialog = qobject_cast<QDialog *>(widget);
        if (dialog && dialog != window && dialog->isVisible()) {
            return dialog;
        }
    }
    return nullptr;
}

QList<QUrl> clipboardUrls()
{
    const QMimeData *data = QApplication::clipboard()->mimeData();
    return data ? data->urls() : QList<QUrl>();
}

QByteArray clipboardData(const QString &format)
{
    const QMimeData *data = QApplication::clipboard()->mimeData();
    return data ? data->data(format) : QByteArray();
}

QByteArray contents(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}

// Makes the item named @p name current without changing the selection.
void focusRow(QAbstractItemView *view, const QString &name)
{
    view->selectionModel()->setCurrentIndex(rowNamed(view, name), QItemSelectionModel::NoUpdate);
}

// Presses @p key on the focused widget and returns the widget focused afterwards.
QWidget *pressOnFocus(Qt::Key key, Qt::KeyboardModifiers modifiers = {})
{
    QTest::keyClick(QApplication::focusWidget(), key, modifiers);
    return QApplication::focusWidget();
}

// QSplitter rounds the sizes it is given, so allow a pixel either way.
bool nearlyEqual(const QList<int> &actual, const QList<int> &expected)
{
    if (actual.size() != expected.size()) {
        return false;
    }
    for (qsizetype i = 0; i < actual.size(); ++i) {
        if (qAbs(actual.at(i) - expected.at(i)) > 1 || (actual.at(i) == 0) != (expected.at(i) == 0)) {
            return false;
        }
    }
    return true;
}

QString describe(const QList<int> &actual, const QList<int> &expected)
{
    QStringList actualText;
    QStringList expectedText;
    for (int size : actual) {
        actualText.append(QString::number(size));
    }
    for (int size : expected) {
        expectedText.append(QString::number(size));
    }
    return QStringLiteral("actual %1, expected %2").arg(actualText.join(QLatin1Char(' ')), expectedText.join(QLatin1Char(' ')));
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
        for (int i = 0; i < ManyFolders; ++i) {
            QVERIFY(root.mkpath(QStringLiteral("folders/f%1").arg(i, 3, 10, QLatin1Char('0'))));
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
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Standard);
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
        QVERIFY(columns->previewSlot()->isVisible());
        QVERIFY(columns->preview());

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

    void previewFollowsTheFocusedItem()
    {
        MainWindow window;
        window.resize(1400, 600);
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QTRY_VERIFY(!view->isLoading());
        pane->setYaziMode(true);
        YaziColumns *columns = pane->yaziColumns();
        YaziPreview *preview = columns->preview();
        QVERIFY(preview);
        QVERIFY(preview->isVisible());
        QCOMPARE(preview->view(), view);
        QVERIFY(preview->panel()->isCompact());
        QVERIFY(columns->previewSlot()->isAncestorOf(preview));
        auto *title = preview->findChild<QLabel *>(QStringLiteral("previewTitle"));
        QVERIFY(title);

        // The focused item is previewed, even when nothing is selected.
        QAbstractItemView *items = view->currentView();
        QTRY_VERIFY(rowNamed(items, QStringLiteral("file.txt")).isValid());
        items->clearSelection();
        focusRow(items, QStringLiteral("file.txt"));
        QVERIFY(view->selectedItems().isEmpty());
        QVERIFY(preview->isUpdatePending());
        QTRY_COMPARE(preview->item().url(), m_file);
        QVERIFY(!preview->isUpdatePending());
        QCOMPARE(title->text(), QStringLiteral("file.txt"));
        QCOMPARE(preview->accessibleDescription(), QStringLiteral("Preview of file.txt"));

        // Quick focus changes preview only the item the focus rests on.
        QSignalSpy changed(preview, &YaziPreview::itemChanged);
        focusRow(items, QStringLiteral("other"));
        focusRow(items, QStringLiteral("sub"));
        QVERIFY(preview->isUpdatePending());
        QTRY_COMPARE(preview->item().url(), m_sub);
        QTest::qWait(YaziPreview::UpdateDelay * 2);
        QCOMPARE(changed.count(), 1);

        // A folder gets a summary.
        QCOMPARE(title->text(), QStringLiteral("sub"));
        auto *message = preview->findChild<QLabel *>(QStringLiteral("previewMessage"));
        QVERIFY(message);
        QVERIFY(message->isVisible());
        QVERIFY(!message->text().isEmpty());

        // Selecting another item moves the focus and the preview with it.
        view->selectUrls({m_file});
        QTRY_COMPARE(preview->item().url(), m_file);

        // Navigating away previews the new folder's focused item, if any.
        pane->setUrl(m_b);
        QTRY_VERIFY(preview->item().isNull());
        QCOMPARE(preview->accessibleDescription(), QStringLiteral("Nothing to preview"));

        pane->setYaziMode(false);
        QVERIFY(!columns->preview());
        QVERIFY(!columns->previewWidget());
    }

    void previewToggleTracksTheActivePane()
    {
        MainWindow window;
        window.resize(1400, 600);
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QAction *action = window.actionCollection()->action(QStringLiteral("toggle_yazi_preview"));
        QVERIFY(action);
        QVERIFY(action->isCheckable());
        QVERIFY(action->shortcuts().isEmpty());
        auto *viewMenu = window.findChild<QMenu *>(QStringLiteral("view"));
        QVERIFY(viewMenu);
        QVERIFY(viewMenu->actions().contains(action));

        Pane *first = window.activePane();
        Pane *second = window.inactivePane();
        QVERIFY(first->yaziPreview());
        QVERIFY(second->yaziPreview());
        QVERIFY(action->isChecked());
        QVERIFY(!action->isEnabled());

        first->setYaziMode(true);
        QVERIFY(action->isEnabled());
        QVERIFY(action->isChecked());
        QVERIFY(first->yaziColumns()->preview());
        QSignalSpy changed(first, &Pane::yaziPreviewChanged);
        QSignalSpy layoutChanged(first, &Pane::workspaceLayoutChanged);
        action->trigger();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().first().toBool(), false);
        QVERIFY(layoutChanged.count() >= 1);
        QVERIFY(!first->yaziPreview());
        QVERIFY(!action->isChecked());
        QVERIFY(!first->yaziColumns()->previewEnabled());
        QVERIFY(first->yaziColumns()->previewSlot()->isHidden());
        QCOMPARE(first->saveState().yaziPreview, 0);
        QVERIFY(second->yaziPreview());
        QCOMPARE(second->saveState().yaziPreview, -1);

        Q_EMIT second->activated();
        QVERIFY(!action->isEnabled());
        QVERIFY(action->isChecked());
        Q_EMIT first->activated();
        QVERIFY(action->isEnabled());
        QVERIFY(!action->isChecked());

        // The choice is kept while the mode is off.
        first->setYaziMode(false);
        QVERIFY(!action->isEnabled());
        QVERIFY(!action->isChecked());
        first->setYaziMode(true);
        QVERIFY(!first->yaziColumns()->previewEnabled());
        action->trigger();
        QVERIFY(first->yaziPreview());
        QVERIFY(first->yaziColumns()->preview());
        QTRY_VERIFY(first->yaziColumns()->previewSlot()->isVisible());
        QVERIFY(!second->yaziMode());
    }

    void previewDefaultFollowsTheSetting()
    {
        const bool previous = Settings::yaziPreviewVisible();
        const auto restore = qScopeGuard([previous] {
            Settings::setYaziPreviewVisible(previous);
        });
        Settings::setYaziPreviewVisible(false);
        MainWindow window;
        window.openUrls({m_a, m_b});
        Pane *pane = window.activePane();
        QVERIFY(!pane->yaziPreview());
        pane->setYaziMode(true);
        QVERIFY(!pane->yaziColumns()->previewEnabled());
        QAction *action = window.actionCollection()->action(QStringLiteral("toggle_yazi_preview"));
        QVERIFY(!action->isChecked());
        QCOMPARE(pane->saveState().yaziPreview, -1);

        // Without a choice of its own, a pane follows changes to the setting.
        Settings::setYaziPreviewVisible(true);
        window.applySettings();
        QVERIFY(pane->yaziPreview());
        QVERIFY(pane->yaziColumns()->preview());
        QVERIFY(action->isChecked());

        // A choice outlasts the setting and is saved.
        pane->setYaziPreview(true);
        QCOMPARE(pane->saveState().yaziPreview, 1);
        Settings::setYaziPreviewVisible(false);
        window.applySettings();
        QVERIFY(pane->yaziPreview());
        QCOMPARE(window.inactivePane()->saveState().yaziPreview, -1);
        QVERIFY(!window.inactivePane()->yaziPreview());

        // Restored states without a choice use the setting.
        PaneSession state = pane->saveState();
        state.yaziPreview = -1;
        pane->replaceState(state);
        QVERIFY(!pane->yaziPreview());
        QVERIFY(!pane->yaziColumns()->previewEnabled());
        QCOMPARE(pane->saveState(), state);
        state.yaziPreview = 1;
        pane->replaceState(state);
        QVERIFY(pane->yaziPreview());
        QVERIFY(pane->yaziColumns()->preview());
        QCOMPARE(pane->saveState(), state);
    }

    void narrowColumnsHideThePreviewThenTheParent()
    {
        YaziColumns columns;
        auto *view = new FolderView;
        view->setUrl(m_a);
        columns.setCurrentView(view);
        columns.setUrl(m_a);
        columns.setPreviewEnabled(true);
        QVERIFY(columns.preview());
        columns.resize(800, 400);
        columns.show();
        QVERIFY(QTest::qWaitForWindowExposed(&columns));
        columns.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&columns));
        QSplitter *splitter = columns.splitter();
        QListView *parent = columns.parentColumn();
        QWidget *slot = columns.previewSlot();
        const int handle = splitter->handleWidth();
        const auto expected = [&columns, handle](int width) {
            const YaziMode::ColumnWidths widths = YaziMode::columnWidths(width, true, columns.ratios(), handle);
            return QList<int>{widths.parent, widths.current, widths.preview};
        };
        QTRY_COMPARE(splitter->width(), 800);
        QTRY_VERIFY2(nearlyEqual(splitter->sizes(), expected(800)), qPrintable(describe(splitter->sizes(), expected(800))));
        QVERIFY(parent->isVisible());
        QVERIFY(slot->isVisible());

        // The preview column hides first and takes the focus it had to the current column.
        columns.preview()->setFocus();
        QTRY_VERIFY(columns.preview()->hasFocus());
        const int twoColumns = YaziMode::ThreeColumnWidth + 2 * handle - 1;
        columns.resize(twoColumns, 400);
        QTRY_COMPARE(splitter->width(), twoColumns);
        QVERIFY(slot->isHidden());
        QVERIFY(parent->isVisible());
        QVERIFY2(nearlyEqual(splitter->sizes(), expected(twoColumns)), qPrintable(describe(splitter->sizes(), expected(twoColumns))));
        QVERIFY(view->width() >= YaziMode::MinimumCurrentWidth);
        QVERIFY(view->currentView()->hasFocus());
        QVERIFY(columns.previewEnabled());

        // Then the parent column, leaving the current column the whole width.
        parent->setFocus();
        QTRY_VERIFY(parent->hasFocus());
        const int oneColumn = YaziMode::TwoColumnWidth + handle - 1;
        columns.resize(oneColumn, 400);
        QTRY_COMPARE(splitter->width(), oneColumn);
        QVERIFY(parent->isHidden());
        QVERIFY(slot->isHidden());
        QCOMPARE(view->width(), oneColumn);
        QVERIFY(view->currentView()->hasFocus());
        columns.resize(200, 400);
        QTRY_COMPARE(splitter->width(), 200);
        QCOMPARE(view->width(), 200);

        // Both come back when the pane widens.
        columns.resize(800, 400);
        QTRY_COMPARE(splitter->width(), 800);
        QVERIFY(parent->isVisible());
        QVERIFY(slot->isVisible());
        QVERIFY2(nearlyEqual(splitter->sizes(), expected(800)), qPrintable(describe(splitter->sizes(), expected(800))));
        QTRY_COMPARE(columns.preview()->view(), view);

        // Dragging a handle changes the ratios, which outlast hiding the columns.
        QSplitterHandle *first = splitter->handle(1);
        const int parentWidth = parent->width();
        const QPoint grab = first->rect().center();
        QTest::mousePress(first, Qt::LeftButton, {}, grab);
        QTest::mouseMove(first, grab + QPoint(60, 0));
        QTest::mouseRelease(first, Qt::LeftButton, {}, grab + QPoint(60, 0));
        QTRY_VERIFY(parent->width() > parentWidth);
        const YaziMode::Ratios dragged = columns.ratios();
        QVERIFY(dragged != YaziMode::Ratios());
        QVERIFY2(nearlyEqual(splitter->sizes(), expected(800)), qPrintable(describe(splitter->sizes(), expected(800))));
        const QList<int> draggedSizes = splitter->sizes();
        columns.resize(400, 400);
        QTRY_VERIFY(slot->isHidden());
        columns.resize(250, 400);
        QTRY_VERIFY(parent->isHidden());
        columns.resize(800, 400);
        QTRY_VERIFY(slot->isVisible());
        QCOMPARE(columns.ratios(), dragged);
        QVERIFY2(nearlyEqual(splitter->sizes(), draggedSizes), qPrintable(describe(splitter->sizes(), draggedSizes)));

        columns.setRatios({1, 1, 1});
        QVERIFY2(nearlyEqual(splitter->sizes(), expected(800)), qPrintable(describe(splitter->sizes(), expected(800))));
        QVERIFY(qAbs(parent->width() - slot->width()) <= 1);

        // Without a preview, the parent column and the current one share the width.
        columns.setPreviewEnabled(false);
        QVERIFY(slot->isHidden());
        QVERIFY(parent->isVisible());
        QCOMPARE(parent->width() + splitter->handleWidth() + view->width(), 800);
    }

    void parentColumnKeepsTheCurrentFolderInView()
    {
        YaziColumns columns;
        auto *view = new FolderView;
        const QUrl deep = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("folders/f055")));
        view->setUrl(deep);
        columns.setCurrentView(view);
        columns.setUrl(deep);
        columns.resize(600, 240);
        columns.show();
        QVERIFY(QTest::qWaitForWindowExposed(&columns));
        QListView *parent = columns.parentColumn();
        QTRY_COMPARE(parent->model()->rowCount(), ManyFolders);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("f055"));
        QVERIFY(parent->selectionModel()->isSelected(parent->currentIndex()));
        QTRY_VERIFY(parent->viewport()->rect().contains(parent->visualRect(parent->currentIndex())));

        // The parent column follows the font of the folder view.
        QFont font = view->font();
        font.setPointSize(font.pointSize() + 3);
        view->setFont(font);
        QCOMPARE(parent->font().pointSize(), font.pointSize());
        QTRY_VERIFY(parent->viewport()->rect().contains(parent->visualRect(parent->currentIndex())));

        const QUrl early = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("folders/f002")));
        view->setUrl(early);
        columns.setUrl(early);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("f002"));
        QTRY_VERIFY(parent->viewport()->rect().contains(parent->visualRect(parent->currentIndex())));
    }

    void columnsAreLabelledAndReachableByKeyboard()
    {
        const bool tabSwitchesPanes = Settings::tabSwitchesPanes();
        const auto restore = qScopeGuard([tabSwitchesPanes] {
            Settings::setTabSwitchesPanes(tabSwitchesPanes);
        });
        Settings::setTabSwitchesPanes(false);
        MainWindow window;
        window.resize(1400, 600);
        window.openUrls({m_a, m_b});
        window.applySettings();
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QAbstractItemView *items = view->currentView();
        const QString viewName = view->accessibleName();
        const QString itemsName = items->accessibleName();
        QTRY_VERIFY(!view->isLoading());

        pane->setYaziMode(true);
        YaziColumns *columns = pane->yaziColumns();
        QListView *parent = columns->parentColumn();
        YaziPreview *preview = columns->preview();
        QVERIFY(preview);
        QCOMPARE(parent->accessibleName(), QStringLiteral("Parent folder"));
        QVERIFY(!parent->accessibleDescription().isEmpty());
        QCOMPARE(view->accessibleName(), QStringLiteral("Current folder"));
        QCOMPARE(items->accessibleName(), QStringLiteral("Current folder"));
        QVERIFY(!items->accessibleDescription().isEmpty());
        QCOMPARE(columns->previewSlot()->accessibleName(), QStringLiteral("Preview"));
        QVERIFY(!columns->previewSlot()->accessibleDescription().isEmpty());
        QCOMPARE(preview->accessibleName(), QStringLiteral("Preview"));

        view->setViewMode(FolderView::ViewMode::Icons);
        QCOMPARE(view->currentView()->accessibleName(), QStringLiteral("Current folder"));
        QCOMPARE(items->accessibleName(), itemsName);
        view->setViewMode(FolderView::ViewMode::Details);
        QCOMPARE(items->accessibleName(), QStringLiteral("Current folder"));

        // Tab moves from the parent column through the current column to the preview and on out.
        parent->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(parent->hasFocus());
        QCOMPARE(pressOnFocus(Qt::Key_Tab), static_cast<QWidget *>(items));
        QCOMPARE(pressOnFocus(Qt::Key_Tab), static_cast<QWidget *>(preview));
        QWidget *after = preview;
        for (int presses = 0; presses < 10 && columns->isAncestorOf(after); ++presses) {
            after = pressOnFocus(Qt::Key_Tab);
        }
        QVERIFY(after);
        QVERIFY(!columns->isAncestorOf(after));

        // Shift+Tab goes back, also out of the parent column.
        preview->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(preview->hasFocus());
        QCOMPARE(pressOnFocus(Qt::Key_Backtab, Qt::ShiftModifier), static_cast<QWidget *>(items));
        QCOMPARE(pressOnFocus(Qt::Key_Backtab, Qt::ShiftModifier), static_cast<QWidget *>(parent));
        QWidget *before = pressOnFocus(Qt::Key_Backtab, Qt::ShiftModifier);
        QVERIFY(before);
        QVERIFY(!columns->isAncestorOf(before));

        // Escape in the preview returns to the current column.
        preview->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(preview->hasFocus());
        QCOMPARE(window.activePane(), pane);
        QTest::keyClick(preview, Qt::Key_Escape);
        QVERIFY(items->hasFocus());

        // Focusing the other columns of the inactive pane activates it.
        Pane *other = window.inactivePane();
        Q_EMIT other->activated();
        QCOMPARE(window.activePane(), other);
        preview->setFocus(Qt::OtherFocusReason);
        QTRY_COMPARE(window.activePane(), pane);
        QVERIFY(preview->hasFocus());

        // While Tab switches panes, Shift+Tab still reaches the parent column.
        Settings::setTabSwitchesPanes(true);
        window.applySettings();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        QCOMPARE(pressOnFocus(Qt::Key_Backtab, Qt::ShiftModifier), static_cast<QWidget *>(parent));
        QCOMPARE(window.activePane(), pane);

        pane->setYaziMode(false);
        QCOMPARE(view->accessibleName(), viewName);
        QCOMPARE(items->accessibleName(), itemsName);
    }

    void narrowColumnsLeaveTheOtherPaneAlone()
    {
        MainWindow window;
        window.resize(1400, 600);
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        window.setStackedLayout(false);
        Pane *pane = window.activePane();
        Pane *other = window.inactivePane();
        auto *paneSplitter = window.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        QVERIFY(paneSplitter);
        QTRY_VERIFY(pane->isVisible() && other->isVisible());
        const int paneIndex = paneSplitter->indexOf(pane);
        const int otherIndex = paneSplitter->indexOf(other);
        QList<int> sizes = paneSplitter->sizes();
        const int both = sizes.at(paneIndex) + sizes.at(otherIndex);
        sizes[paneIndex] = 400;
        sizes[otherIndex] = both - 400;
        paneSplitter->setSizes(sizes);
        QTest::qWait(50);
        const QList<int> narrow = paneSplitter->sizes();
        const QRect otherGeometry = other->geometry();
        QWidget *otherViewParent = other->view()->parentWidget();
        const QSize windowSize = window.size();

        pane->setYaziMode(true);
        YaziColumns *columns = pane->yaziColumns();
        QTest::qWait(50);
        QVERIFY(columns->previewEnabled());
        QVERIFY(columns->previewSlot()->isHidden());
        QCOMPARE(paneSplitter->sizes(), narrow);
        QCOMPARE(other->geometry(), otherGeometry);
        QCOMPARE(window.size(), windowSize);
        QVERIFY(!other->yaziMode());
        QCOMPARE(other->view()->parentWidget(), otherViewParent);
        QVERIFY(other->yaziColumns()->isHidden());

        // Narrower still, the parent column hides too, and the other pane keeps its width.
        sizes = narrow;
        sizes[paneIndex] = 250;
        sizes[otherIndex] = both - 250;
        paneSplitter->setSizes(sizes);
        QTRY_VERIFY(columns->parentColumn()->isHidden());
        QVERIFY(pane->view()->isVisible());
        const QList<int> narrower = paneSplitter->sizes();
        QTest::qWait(50);
        QCOMPARE(paneSplitter->sizes(), narrower);
        QVERIFY(!other->yaziMode());

        // Widening the pane brings both columns back.
        sizes[paneIndex] = both - 300;
        sizes[otherIndex] = 300;
        paneSplitter->setSizes(sizes);
        QTRY_VERIFY(columns->previewSlot()->isVisible());
        QVERIFY(columns->parentColumn()->isVisible());
        QCOMPARE(other->view()->parentWidget(), otherViewParent);
        QCOMPARE(window.size(), windowSize);
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
            window.inactivePane()->setYaziPreview(false);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QVERIFY(window.close());
        }
        const WindowSession saved = Session::load(KSharedConfig::openStateConfig()->group(QStringLiteral("Session")));
        QVERIFY(!saved.left.yaziMode);
        QVERIFY(saved.right.yaziMode);
        QCOMPARE(saved.left.yaziPreview, -1);
        QCOMPARE(saved.right.yaziPreview, 0);

        MainWindow restored;
        restored.openUrls({});
        Pane *active = restored.activePane();
        Pane *inactive = restored.inactivePane();
        QVERIFY(!active->yaziMode());
        QVERIFY(inactive->yaziMode());
        QCOMPARE(inactive->yaziColumns()->currentView(), inactive->view());
        QVERIFY(active->yaziPreview());
        QVERIFY(!inactive->yaziPreview());
        QVERIFY(!inactive->yaziColumns()->previewEnabled());
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

        state.yaziMode = true;
        state.yaziPreview = 0;
        pane->replaceState(state);
        QVERIFY(!pane->yaziPreview());
        QVERIFY(!pane->yaziColumns()->previewEnabled());
        QCOMPARE(pane->saveState(), state);
        state.yaziPreview = 1;
        pane->replaceState(state);
        QVERIFY(pane->yaziColumns()->preview());
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
        pane->setYaziPreview(false);

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
        pane->setYaziPreview(true);
        QTRY_VERIFY(selector->text().endsWith(QStringLiteral(" *")));

        answerMessageBox(QMessageBox::Yes);
        restoreLayout->trigger();
        QTRY_VERIFY(pane->yaziMode());
        QVERIFY(!pane->yaziPreview());
        QVERIFY(!pane->yaziColumns()->previewEnabled());
        QVERIFY(!window.inactivePane()->yaziMode());
        QVERIFY(window.actionCollection()->action(QStringLiteral("toggle_yazi_mode"))->isChecked());
        QTRY_VERIFY(!selector->text().endsWith(QStringLiteral(" *")));
    }

    void vimKeysMoveTheCurrentItem()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_many, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        // In grid views, j moves a row of items as Down does; use a list to count rows.
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
        QCOMPARE(row(), 0);

        pressOnFocus(Qt::Key_J);
        QCOMPARE(row(), 1);
        QVERIFY(items->selectionModel()->isSelected(items->currentIndex()));
        QCOMPARE(items->selectionModel()->selectedRows().size(), 1);
        pressOnFocus(Qt::Key_K);
        QCOMPARE(row(), 0);
        pressOnFocus(Qt::Key_5);
        pressOnFocus(Qt::Key_J);
        QCOMPARE(row(), 5);
        pressOnFocus(Qt::Key_G, Qt::ShiftModifier);
        QCOMPARE(row(), ManyFiles - 1);
        pressOnFocus(Qt::Key_G);
        QCOMPARE(row(), ManyFiles - 1);
        pressOnFocus(Qt::Key_G);
        QCOMPARE(row(), 0);
        QVERIFY(items->selectionModel()->isSelected(items->currentIndex()));

        // Ctrl+D overrides the window's shortcut while the columns have the focus.
        pressOnFocus(Qt::Key_D, Qt::ControlModifier);
        const int halfPage = row();
        QVERIFY2(halfPage > 1, qPrintable(QString::number(halfPage)));
        pressOnFocus(Qt::Key_U, Qt::ControlModifier);
        QCOMPARE(row(), 0);

        // The standard keys keep working.
        pressOnFocus(Qt::Key_Down);
        QCOMPARE(row(), 1);
        QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(items));
    }

    void vimKeysGoToTheParentAndOpenFolders()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_sub, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QAbstractItemView *items = view->currentView();
        YaziColumns *columns = pane->yaziColumns();
        QListView *parent = columns->parentColumn();

        // h opens the parent folder with the folder just left focused and selected.
        pressOnFocus(Qt::Key_H);
        QTRY_COMPARE(pane->url(), m_a);
        QTRY_COMPARE(selectedUrls(view), QList<QUrl>{m_sub});
        QCOMPARE(items->currentIndex().data().toString(), QStringLiteral("sub"));
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("a"));
        QVERIFY(items->hasFocus());

        // l opens the focused folder; H and L go back and forward.
        pressOnFocus(Qt::Key_L);
        QTRY_COMPARE(pane->url(), m_sub);
        pressOnFocus(Qt::Key_H, Qt::ShiftModifier);
        QTRY_COMPARE(pane->url(), m_a);
        pressOnFocus(Qt::Key_L, Qt::ShiftModifier);
        QTRY_COMPARE(pane->url(), m_sub);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("sub"));

        // gp focuses the parent column, where the keys move and open its items.
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_P);
        QVERIFY(parent->hasFocus());
        pressOnFocus(Qt::Key_K);
        QCOMPARE(parent->currentIndex().data().toString(), QStringLiteral("other"));
        QCOMPARE(pane->url(), m_sub);
        pressOnFocus(Qt::Key_L);
        QTRY_COMPARE(pane->url(), m_other);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("other"));

        // gc and gv focus the current and preview columns; zp toggles the preview.
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_C);
        QVERIFY(items->hasFocus());
        QVERIFY(pane->yaziPreview());
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_V);
        QVERIFY(columns->preview());
        QVERIFY(columns->preview()->hasFocus());
        pressOnFocus(Qt::Key_Z);
        pressOnFocus(Qt::Key_P);
        QTRY_VERIFY(!pane->yaziPreview());
        QVERIFY(items->hasFocus());
        pressOnFocus(Qt::Key_Z);
        pressOnFocus(Qt::Key_P);
        QTRY_VERIFY(pane->yaziPreview());

        // gh opens the home folder.
        const QUrl home = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
        const QUrl previousHome = Settings::homeUrl();
        const auto restoreHome = qScopeGuard([previousHome] {
            Settings::setHomeUrl(previousHome);
        });
        Settings::setHomeUrl(home);
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_H);
        QTRY_COMPARE(stripped(pane->url()), m_b);

        // q leaves the mode.
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        pressOnFocus(Qt::Key_Q);
        QTRY_VERIFY(!pane->yaziMode());
    }

    void standardKeysGoToTheParentAndOpenFolders()
    {
        MainWindow window;
        showWithFocusOnItems(window, m_sub, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        QCOMPARE(pane->yaziColumns()->modeIndicator()->text(), QStringLiteral("Yazi"));

        pressOnFocus(Qt::Key_Left);
        QTRY_COMPARE(pane->url(), m_a);
        QTRY_COMPARE(selectedUrls(pane->view()), QList<QUrl>{m_sub});
        pressOnFocus(Qt::Key_Right);
        QTRY_COMPARE(pane->url(), m_sub);

        // Vim-style letters don't apply; they filter as usual.
        pressOnFocus(Qt::Key_Left);
        QTRY_COMPARE(pane->url(), m_a);
        QTRY_COMPARE(items->currentIndex().data().toString(), QStringLiteral("sub"));
        const bool typeAhead = Settings::filterTypeAhead();
        const auto restore = qScopeGuard([typeAhead] {
            Settings::setFilterTypeAhead(typeAhead);
        });
        Settings::setFilterTypeAhead(true);
        pressOnFocus(Qt::Key_J);
        auto *filterBar = pane->findChild<FilterBar *>();
        QVERIFY(filterBar);
        QTRY_VERIFY(filterBar->isVisible());
        QCOMPARE(filterBar->filter().text(), QStringLiteral("j"));
        QCOMPARE(pane->url(), m_a);
    }

    void keysAreLeftToTypingAndToPanesOutsideTheMode()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        const bool typeAhead = Settings::filterTypeAhead();
        const auto restore = qScopeGuard([typeAhead] {
            Settings::setFilterTypeAhead(typeAhead);
        });
        Settings::setFilterTypeAhead(false);
        MainWindow window;
        showWithFocusOnItems(window, m_a, false);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QAbstractItemView *items = view->currentView();
        QTRY_VERIFY(rowNamed(items, QStringLiteral("sub")).isValid());
        items->setCurrentIndex(rowNamed(items, QStringLiteral("sub")));

        // Outside Yazi mode, the keys do nothing of their own.
        QVERIFY(!pane->yaziMode());
        for (Qt::Key key : {Qt::Key_J, Qt::Key_K, Qt::Key_L, Qt::Key_H, Qt::Key_Q}) {
            pressOnFocus(key);
        }
        pressOnFocus(Qt::Key_G, Qt::ShiftModifier);
        QTest::qWait(50);
        QCOMPARE(pane->url(), m_a);
        QCOMPARE(items->currentIndex().data().toString(), QStringLiteral("sub"));
        QVERIFY(!pane->yaziMode());

        // In the mode, letters the preset doesn't bind still reach type-ahead.
        pane->setYaziMode(true);
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        Settings::setFilterTypeAhead(true);
        auto *filterBar = pane->findChild<FilterBar *>();
        QVERIFY(filterBar);
        QVERIFY(!filterBar->isVisible());
        pressOnFocus(Qt::Key_E);
        QTRY_VERIFY(filterBar->isVisible());
        QCOMPARE(filterBar->filter().text(), QStringLiteral("e"));

        // The filter bar takes the preset's letters as text.
        QVERIFY(qobject_cast<QLineEdit *>(QApplication::focusWidget()));
        for (Qt::Key key : {Qt::Key_H, Qt::Key_J, Qt::Key_G, Qt::Key_G, Qt::Key_Q}) {
            pressOnFocus(key);
        }
        QCOMPARE(filterBar->filter().text(), QStringLiteral("ehjggq"));
        QCOMPARE(pane->url(), m_a);
        QVERIFY(pane->yaziMode());
        QVERIFY(!pane->yaziColumns()->keys()->isPending());
        filterBar->close();
        QTRY_VERIFY(filterBar->filter().text().isEmpty());
        Settings::setFilterTypeAhead(false);

        // So does the location bar.
        pane->editUrl();
        QTRY_VERIFY(YaziKeys::isTypingWidget(QApplication::focusWidget()));
        QWidget *locationWidget = QApplication::focusWidget();
        auto *combo = qobject_cast<QComboBox *>(locationWidget);
        QLineEdit *location = combo ? combo->lineEdit() : qobject_cast<QLineEdit *>(locationWidget);
        QVERIFY(location);
        pressOnFocus(Qt::Key_End);
        pressOnFocus(Qt::Key_H);
        QVERIFY(location->text().endsWith(QLatin1Char('h')));
        QTest::qWait(50);
        QCOMPARE(pane->url(), m_a);
        pressOnFocus(Qt::Key_Escape);

        // And an inline rename editor in the current column.
        view->setViewMode(FolderView::ViewMode::Cards);
        QAbstractItemView *cards = view->currentView();
        QTRY_VERIFY(rowNamed(cards, QStringLiteral("sub")).isValid());
        cards->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(cards->hasFocus());
        cards->selectionModel()->select(rowNamed(cards, QStringLiteral("sub")), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        // The keys apply in the cards view until the editor opens.
        pressOnFocus(Qt::Key_G);
        QVERIFY(pane->yaziColumns()->keys()->isPending());
        pressOnFocus(Qt::Key_Escape);
        QVERIFY(!pane->yaziColumns()->keys()->isPending());
        QVERIFY(view->renameInline());
        QTRY_VERIFY(YaziKeys::isTypingWidget(QApplication::focusWidget()));
        QWidget *editor = QApplication::focusWidget();
        QVERIFY(cards->isAncestorOf(editor));
        for (Qt::Key key : {Qt::Key_End, Qt::Key_H, Qt::Key_L, Qt::Key_G}) {
            pressOnFocus(key);
        }
        QTest::qWait(50);
        QCOMPARE(pane->url(), m_a);
        QVERIFY(!pane->yaziColumns()->keys()->isPending());
        if (auto *lineEdit = qobject_cast<QLineEdit *>(editor)) {
            QVERIFY2(lineEdit->text().endsWith(QStringLiteral("hlg")), qPrintable(lineEdit->text()));
        }
        pressOnFocus(Qt::Key_Escape);
        QTRY_VERIFY(cards->hasFocus());
        QVERIFY(QFileInfo::exists(m_sub.toLocalFile()));
    }

    void indicatorShowsThePresetAndPendingKeys()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_a, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        YaziColumns *columns = pane->yaziColumns();
        QLabel *indicator = columns->modeIndicator();
        QAbstractItemView *items = pane->view()->currentView();
        QCOMPARE(indicator->text(), QStringLiteral("Yazi · Vim"));
        QCOMPARE(indicator->accessibleName(), QStringLiteral("Yazi mode with Vim-style keys"));

        pressOnFocus(Qt::Key_G);
        QCOMPARE(indicator->text(), QStringLiteral("Yazi · Vim · g…"));
        QCOMPARE(indicator->accessibleName(), QStringLiteral("Yazi mode with Vim-style keys, waiting for the keys after g"));
        // Escape cancels the sequence and nothing else.
        pressOnFocus(Qt::Key_Escape);
        QCOMPARE(indicator->text(), QStringLiteral("Yazi · Vim"));
        QCOMPARE(indicator->accessibleName(), QStringLiteral("Yazi mode with Vim-style keys"));
        QVERIFY(pane->yaziMode());
        QVERIFY(items->hasFocus());

        // A pending sequence expires.
        pressOnFocus(Qt::Key_2);
        pressOnFocus(Qt::Key_G);
        QCOMPARE(indicator->text(), QStringLiteral("Yazi · Vim · 2g…"));
        QTRY_COMPARE_WITH_TIMEOUT(indicator->text(), QStringLiteral("Yazi · Vim"), 3000);
        QVERIFY(!columns->keys()->isPending());

        // Moving the focus cancels it.
        pressOnFocus(Qt::Key_Z);
        QCOMPARE(indicator->text(), QStringLiteral("Yazi · Vim · z…"));
        columns->parentColumn()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(columns->parentColumn()->hasFocus());
        QCOMPARE(indicator->text(), QStringLiteral("Yazi · Vim"));

        // The View menu action switches the preset.
        QAction *vimKeys = window.actionCollection()->action(QStringLiteral("toggle_yazi_vim_keys"));
        QVERIFY(vimKeys);
        QVERIFY(vimKeys->isChecked());
        vimKeys->trigger();
        QCOMPARE(Settings::yaziKeyPreset(), int(Settings::EnumYaziKeyPreset::Standard));
        QCOMPARE(indicator->text(), QStringLiteral("Yazi"));
        QCOMPARE(indicator->accessibleName(), QStringLiteral("Yazi mode"));
        QCOMPARE(window.inactivePane()->yaziColumns()->keys()->preset(), YaziMode::Preset::Standard);
        pressOnFocus(Qt::Key_G);
        QVERIFY(!columns->keys()->isPending());
        QCOMPARE(indicator->text(), QStringLiteral("Yazi"));
        vimKeys->trigger();
        QCOMPARE(Settings::yaziKeyPreset(), int(Settings::EnumYaziKeyPreset::Vim));
        QCOMPARE(indicator->text(), QStringLiteral("Yazi · Vim"));
    }

    void columnActionsAreRebindableAndFollowTheMode()
    {
        MainWindow window;
        showWithFocusOnItems(window, m_sub, false);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        const auto action = [&window](const char *name) {
            return window.actionCollection()->action(QLatin1String(name));
        };
        QAction *focusParent = action("yazi_focus_parent_column");
        QAction *focusCurrent = action("yazi_focus_current_column");
        QAction *focusPreview = action("yazi_focus_preview");
        QAction *vimKeys = action("toggle_yazi_vim_keys");
        for (QAction *each : {focusParent, focusCurrent, focusPreview, vimKeys}) {
            QVERIFY(each);
            QVERIFY(each->shortcuts().isEmpty());
            QVERIFY(!each->text().isEmpty());
        }
        QVERIFY(vimKeys->isCheckable());
        QVERIFY(!vimKeys->isChecked());
        auto *viewMenu = window.findChild<QMenu *>(QStringLiteral("view"));
        QVERIFY(viewMenu);
        QVERIFY(viewMenu->actions().contains(vimKeys));
        auto *columnsMenu = window.findChild<QMenu *>(QStringLiteral("yazi_columns"));
        QVERIFY(columnsMenu);
        QCOMPARE(columnsMenu->actions(), (QList<QAction *>{focusParent, focusCurrent, focusPreview}));

        QVERIFY(!focusParent->isEnabled());
        // Without the mode, the actions do nothing even if triggered.
        focusParent->trigger();
        QVERIFY(!pane->yaziColumns()->parentColumn()->hasFocus());

        pane->setYaziMode(true);
        QVERIFY(focusParent->isEnabled());
        QVERIFY(focusCurrent->isEnabled());
        QVERIFY(focusPreview->isEnabled());
        focusParent->trigger();
        QTRY_VERIFY(pane->yaziColumns()->parentColumn()->hasFocus());
        focusPreview->trigger();
        QTRY_VERIFY(pane->yaziColumns()->preview()->hasFocus());
        focusCurrent->trigger();
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());

        // A hidden preview column can't take the focus.
        pane->setYaziPreview(false);
        QSignalSpy messages(pane, &Pane::statusMessage);
        focusPreview->trigger();
        QCOMPARE(messages.count(), 1);
        QVERIFY(pane->view()->currentView()->hasFocus());

        pane->setYaziMode(false);
        QVERIFY(!focusParent->isEnabled());
    }

    void vimSpaceAndRangeSelectionMarkItems()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_many, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        YaziColumns *columns = pane->yaziColumns();
        // In grid views, j moves a row of items; use a list to count rows, in name order.
        pane->view()->setViewMode(FolderView::ViewMode::Details);
        pane->view()->setSorting(KDirModel::Name, Qt::AscendingOrder);
        QAbstractItemView *items = pane->view()->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        QLabel *count = columns->selectionIndicator();
        QVERIFY(count);
        const auto name = [](int number) {
            return QStringLiteral("file%1.txt").arg(number, 3, 10, QLatin1Char('0'));
        };
        const auto names = [&name](std::initializer_list<int> numbers) {
            QStringList list;
            for (int number : numbers) {
                list.append(name(number));
            }
            return list;
        };
        const auto focused = [items] {
            return items->currentIndex().data().toString();
        };
        QTRY_VERIFY(rowNamed(items, name(ManyFiles - 1)).isValid());
        items->setCurrentIndex(rowNamed(items, name(0)));
        QCOMPARE(selectedNames(items), names({0}));
        QVERIFY(!columns->isMarking());
        QVERIFY(!count->isVisible());

        // Space marks the focused item and moves down; moving keeps the marks.
        pressOnFocus(Qt::Key_Space);
        QCOMPARE(focused(), name(1));
        QCOMPARE(selectedNames(items), names({0}));
        QVERIFY(columns->isMarking());
        QVERIFY(count->isVisible());
        QCOMPARE(count->text(), QStringLiteral("1 selected"));
        pressOnFocus(Qt::Key_Space);
        pressOnFocus(Qt::Key_J);
        QCOMPARE(focused(), name(3));
        QCOMPARE(selectedNames(items), names({0, 1}));
        pressOnFocus(Qt::Key_Space);
        QCOMPARE(selectedNames(items), names({0, 1, 3}));
        QCOMPARE(count->text(), QStringLiteral("3 selected"));
        // On a marked item, Space unmarks it.
        pressOnFocus(Qt::Key_K);
        pressOnFocus(Qt::Key_Space);
        QCOMPARE(selectedNames(items), names({0, 1}));
        QCOMPARE(focused(), name(4));
        // Ctrl+Space toggles without moving, and a count repeats Space.
        pressOnFocus(Qt::Key_Space, Qt::ControlModifier);
        QCOMPARE(focused(), name(4));
        QCOMPARE(selectedNames(items), names({0, 1, 4}));
        pressOnFocus(Qt::Key_3);
        pressOnFocus(Qt::Key_Space);
        QCOMPARE(selectedNames(items), names({0, 1, 5, 6}));
        QCOMPARE(focused(), name(7));

        // Escape clears the selection, and moving selects the focused item again.
        pressOnFocus(Qt::Key_Escape);
        QVERIFY(selectedNames(items).isEmpty());
        QVERIFY(!columns->isMarking());
        QVERIFY(!count->isVisible());
        pressOnFocus(Qt::Key_J);
        QCOMPARE(selectedNames(items), names({8}));

        // v selects the range between where it started and the focused item.
        items->setCurrentIndex(rowNamed(items, name(10)));
        pressOnFocus(Qt::Key_V);
        QVERIFY(columns->isVisual());
        QCOMPARE(selectedNames(items), names({10}));
        pressOnFocus(Qt::Key_J);
        pressOnFocus(Qt::Key_J);
        QCOMPARE(selectedNames(items), names({10, 11, 12}));
        QCOMPARE(count->text(), QStringLiteral("Visual · 3 selected"));
        pressOnFocus(Qt::Key_3);
        pressOnFocus(Qt::Key_K);
        QCOMPARE(focused(), name(9));
        QCOMPARE(selectedNames(items), names({9, 10}));
        // v again ends it and keeps the range marked.
        pressOnFocus(Qt::Key_V);
        QVERIFY(!columns->isVisual());
        QVERIFY(columns->isMarking());
        pressOnFocus(Qt::Key_G, Qt::ShiftModifier);
        QCOMPARE(focused(), name(ManyFiles - 1));
        QCOMPARE(selectedNames(items), names({9, 10}));
        QCOMPARE(count->text(), QStringLiteral("2 selected"));
        // A new range adds to the marks made before it.
        pressOnFocus(Qt::Key_V);
        pressOnFocus(Qt::Key_K);
        QCOMPARE(selectedNames(items), names({9, 10, ManyFiles - 2, ManyFiles - 1}));
        // Escape ends range selection and clears the selection.
        pressOnFocus(Qt::Key_Escape);
        QVERIFY(!columns->isVisual());
        QVERIFY(!columns->isMarking());
        QVERIFY(selectedNames(items).isEmpty());

        // V and Ctrl+A select everything.
        pressOnFocus(Qt::Key_V, Qt::ShiftModifier);
        QCOMPARE(items->selectionModel()->selectedRows().size(), ManyFiles);
        QCOMPARE(count->text(), QStringLiteral("200 selected"));
        pressOnFocus(Qt::Key_Escape);
        QVERIFY(selectedNames(items).isEmpty());
        pressOnFocus(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(items->selectionModel()->selectedRows().size(), ManyFiles);
        pressOnFocus(Qt::Key_Escape);
        QVERIFY(selectedNames(items).isEmpty());

        // In the parent column, the selection keys only explain where they work.
        QSignalSpy messages(pane, &Pane::statusMessage);
        QVERIFY(columns->focusColumn(YaziColumns::Column::Parent));
        QTRY_VERIFY(columns->parentColumn()->hasFocus());
        pressOnFocus(Qt::Key_Space);
        QCOMPARE(messages.count(), 1);
        QVERIFY(selectedNames(items).isEmpty());
    }

    void vimFileKeysRunTheWindowActions()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_a, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        view->setViewMode(FolderView::ViewMode::Details);
        QAbstractItemView *items = view->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        QTRY_VERIFY(rowNamed(items, QStringLiteral("file.txt")).isValid());
        const auto action = [&window](const char *name) {
            return window.actionCollection()->action(QLatin1String(name));
        };
        // Stand in for the actions' slots, so nothing is trashed, renamed, transferred or undone.
        QAction *trash = action("move_to_trash");
        QAction *permanent = action("delete");
        QAction *rename = action("rename");
        QAction *copyTo = action("copy_to_other_pane");
        QAction *moveTo = action("move_to_other_pane");
        QAction *undo = action("edit_undo");
        for (QAction *each : {trash, permanent, rename, copyTo, moveTo, undo}) {
            QVERIFY(each);
            QObject::disconnect(each, &QAction::triggered, &window, nullptr);
        }
        QObject::disconnect(undo, &QAction::triggered, KIO::FileUndoManager::self(), nullptr);
        QSignalSpy trashed(trash, &QAction::triggered);
        QSignalSpy deleted(permanent, &QAction::triggered);
        QSignalSpy renamed(rename, &QAction::triggered);
        QSignalSpy copied(copyTo, &QAction::triggered);
        QSignalSpy moved(moveTo, &QAction::triggered);
        QSignalSpy undone(undo, &QAction::triggered);

        // With nothing selected, dd moves the focused item to the trash.
        items->selectionModel()->clearSelection();
        focusRow(items, QStringLiteral("file.txt"));
        QVERIFY(view->selectedItems().isEmpty());
        pressOnFocus(Qt::Key_D);
        QVERIFY(pane->yaziColumns()->keys()->isPending());
        QTest::qWait(20);
        QCOMPARE(trashed.count(), 0);
        pressOnFocus(Qt::Key_D);
        QTRY_COMPARE(trashed.count(), 1);
        QCOMPARE(selectedUrls(view), QList<QUrl>{m_file});

        // yy copies and x cuts to the clipboard.
        const QString cutFormat = QStringLiteral("application/x-kde-cutselection");
        pressOnFocus(Qt::Key_Y);
        pressOnFocus(Qt::Key_Y);
        QTRY_COMPARE(clipboardUrls(), QList<QUrl>{m_file});
        QCOMPARE(clipboardData(cutFormat), QByteArray("0"));
        pressOnFocus(Qt::Key_X);
        QTRY_COMPARE(clipboardData(cutFormat), QByteArray("1"));
        QCOMPARE(clipboardUrls(), QList<QUrl>{m_file});

        // r renames, and c and m copy and move to the other pane.
        pressOnFocus(Qt::Key_R);
        QTRY_COMPARE(renamed.count(), 1);
        pressOnFocus(Qt::Key_C);
        QTRY_COMPARE(copied.count(), 1);
        pressOnFocus(Qt::Key_M);
        QTRY_COMPARE(moved.count(), 1);

        // With items marked, the keys act on all of them.
        pressOnFocus(Qt::Key_Space, Qt::ControlModifier);
        pressOnFocus(Qt::Key_K);
        QCOMPARE(items->currentIndex().data().toString(), QStringLiteral("sub"));
        pressOnFocus(Qt::Key_Space, Qt::ControlModifier);
        pressOnFocus(Qt::Key_D);
        pressOnFocus(Qt::Key_D);
        QTRY_COMPARE(trashed.count(), 2);
        QList<QUrl> marked{m_file, m_sub};
        std::sort(marked.begin(), marked.end());
        QCOMPARE(selectedUrls(view), marked);

        // u runs the window's undo action.
        undo->setEnabled(true);
        pressOnFocus(Qt::Key_U);
        QTRY_COMPARE(undone.count(), 1);

        // . shows and hides hidden files.
        QAction *hidden = action("show_hidden_files");
        const bool showing = hidden->isChecked();
        pressOnFocus(Qt::Key_Period);
        QTRY_COMPARE(hidden->isChecked(), !showing);
        QTRY_VERIFY(items->hasFocus());
        pressOnFocus(Qt::Key_Period);
        QTRY_COMPARE(hidden->isChecked(), showing);
        QTRY_VERIFY(items->hasFocus());

        // From the parent column, the keys don't act on the current column's items.
        QSignalSpy messages(pane, &Pane::statusMessage);
        QVERIFY(pane->yaziColumns()->focusColumn(YaziColumns::Column::Parent));
        QTRY_VERIFY(pane->yaziColumns()->parentColumn()->hasFocus());
        pressOnFocus(Qt::Key_D);
        pressOnFocus(Qt::Key_D);
        QTRY_COMPARE(messages.count(), 1);
        QTest::qWait(20);
        QCOMPARE(trashed.count(), 2);
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());

        // a offers to create a folder or file below the focused item.
        pressOnFocus(Qt::Key_A);
        QTRY_VERIFY(qobject_cast<QMenu *>(QApplication::activePopupWidget()));
        QApplication::activePopupWidget()->close();
        QTRY_VERIFY(!QApplication::activePopupWidget());

        // With the standard keys, the window's own shortcuts pass through.
        pane->yaziColumns()->keys()->setPreset(YaziMode::Preset::Standard);
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        QVERIFY(!view->selectedItems().isEmpty());
        pressOnFocus(Qt::Key_Delete);
        QTRY_COMPARE(trashed.count(), 3);

        // Nothing ever deleted permanently.
        QCOMPARE(deleted.count(), 0);
        QVERIFY(QFileInfo::exists(m_file.toLocalFile()));
        QVERIFY(QFileInfo::exists(m_sub.toLocalFile()));
    }

    void trashKeepsItsConfirmation()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("keep.txt"));
        QVERIFY(writeFile(path, "keep"));
        // KIO asks before trashing when the user chose so.
        KSharedConfig::Ptr kiorc = KSharedConfig::openConfig(QStringLiteral("kiorc"), KConfig::NoGlobals);
        kiorc->group(QStringLiteral("Confirmations")).writeEntry("ConfirmTrash", true);
        kiorc->sync();
        const auto restore = qScopeGuard([kiorc] {
            kiorc->group(QStringLiteral("Confirmations")).deleteEntry("ConfirmTrash");
            kiorc->sync();
        });
        MainWindow window;
        showWithFocusOnItems(window, QUrl::fromLocalFile(folder.path()), true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        QTRY_VERIFY(rowNamed(items, QStringLiteral("keep.txt")).isValid());
        QAction *permanent = window.actionCollection()->action(QStringLiteral("delete"));
        QVERIFY(permanent);
        QSignalSpy deleted(permanent, &QAction::triggered);

        items->selectionModel()->clearSelection();
        focusRow(items, QStringLiteral("keep.txt"));
        pressOnFocus(Qt::Key_D);
        pressOnFocus(Qt::Key_D);
        QDialog *dialog = nullptr;
        QTRY_VERIFY((dialog = openDialog(&window)));
        dialog->reject();
        QTRY_VERIFY(!openDialog(&window));
        QTest::qWait(200);
        QVERIFY(QFileInfo::exists(path));
        QCOMPARE(deleted.count(), 0);
    }

    void yankAndPasteCopyWithConflictHandling()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        const auto conflictPolicy = Settings::transferConflictPolicy();
        const auto restore = qScopeGuard([conflictPolicy] {
            Settings::setTransferConflictPolicy(conflictPolicy);
        });
        Settings::setTransferConflictPolicy(Settings::EnumTransferConflictPolicy::Ask);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QDir root(folder.path());
        QVERIFY(root.mkpath(QStringLiteral("from")));
        QVERIFY(root.mkpath(QStringLiteral("to")));
        const QString source = folder.filePath(QStringLiteral("from/note.txt"));
        const QString copy = folder.filePath(QStringLiteral("to/note.txt"));
        QVERIFY(writeFile(source, "new"));
        const QUrl from = QUrl::fromLocalFile(folder.filePath(QStringLiteral("from")));
        const QUrl to = QUrl::fromLocalFile(folder.filePath(QStringLiteral("to")));

        MainWindow window;
        showWithFocusOnItems(window, from, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        QTRY_VERIFY(rowNamed(items, QStringLiteral("note.txt")).isValid());
        items->selectionModel()->clearSelection();
        focusRow(items, QStringLiteral("note.txt"));
        pressOnFocus(Qt::Key_Y);
        pressOnFocus(Qt::Key_Y);
        QTRY_COMPARE(clipboardUrls(), QList<QUrl>{QUrl::fromLocalFile(source)});

        // p pastes into the folder the pane shows.
        pane->setUrl(to);
        QTRY_COMPARE(stripped(pane->url()), stripped(to));
        QTRY_VERIFY(!pane->view()->isLoading());
        items = pane->view()->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        pressOnFocus(Qt::Key_P);
        QTRY_COMPARE(contents(copy), QByteArray("new"));
        QCOMPARE(contents(source), QByteArray("new"));

        // Pasting again over an existing file asks; cancelling keeps the file.
        QVERIFY(writeFile(copy, "old"));
        QTRY_VERIFY(rowNamed(items, QStringLiteral("note.txt")).isValid());
        pressOnFocus(Qt::Key_P);
        QDialog *dialog = nullptr;
        QTRY_VERIFY((dialog = openDialog(&window)));
        dialog->reject();
        QTRY_VERIFY(!openDialog(&window));
        QTest::qWait(200);
        QCOMPARE(contents(copy), QByteArray("old"));
        QCOMPARE(contents(source), QByteArray("new"));
    }

private:
    // Shows @p window with the active pane at @p url, optionally in Yazi mode, and focuses its items.
    void showWithFocusOnItems(MainWindow &window, const QUrl &url, bool yazi)
    {
        window.resize(1400, 600);
        window.openUrls({url, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        Pane *pane = window.activePane();
        QTRY_VERIFY(!pane->view()->isLoading());
        pane->setYaziMode(yazi);
        QAbstractItemView *items = pane->view()->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
    }

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
