// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileoperations.h"
#include "filterbar.h"
#include "folderpreview.h"
#include "folderview.h"
#include "informationpanel.h"
#include "mainwindow.h"
#include "pane.h"
#include "previewpanel.h"
#include "searchpanel.h"
#include "searchwidget.h"
#include "session.h"
#include "settings.h"
#include "tabmodel.h"
#include "transferqueue.h"
#include "transferqueuedialog.h"
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
#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

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

QStringList listNames(const QListWidget *list)
{
    QStringList names;
    for (int row = 0; row < list->count(); ++row) {
        names.append(list->item(row)->text());
    }
    return names;
}

class FakeTransfer : public KJob
{
public:
    void start() override
    {
    }

    void finish(int error = 0)
    {
        if (error) {
            setError(error);
            setErrorText(QStringLiteral("disk full"));
        }
        emitResult();
    }

    void progress(unsigned long percent)
    {
        setPercent(percent);
    }
};

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

// What assistive technology was asked to announce while recordAnnouncements() was installed.
QStringList s_announcements;

void recordAnnouncements(QAccessibleEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (event->type() == QAccessible::Announcement) {
        s_announcements.append(static_cast<QAccessibleAnnouncementEvent *>(event)->message());
    }
#else
    if (event->type() == QAccessible::Alert) {
        if (QAccessibleInterface *interface = event->accessibleInterface()) {
            s_announcements.append(interface->text(QAccessible::Name));
        }
    }
#endif
}

bool announced(const QString &text)
{
    return std::any_of(s_announcements.cbegin(), s_announcements.cend(), [&text](const QString &announcement) {
        return announcement.contains(text);
    });
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

        // A folder lists its contents; this one has none.
        QCOMPARE(title->text(), QStringLiteral("sub"));
        auto *folder = preview->findChild<FolderPreview *>(QStringLiteral("previewFolder"));
        QVERIFY(folder);
        QVERIFY(folder->isVisible());
        QTRY_COMPARE(folder->note()->text(), QStringLiteral("This folder is empty."));
        QCOMPARE(folder->list()->count(), 0);

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

    void folderPreviewListsTheFocusedFolder()
    {
        MainWindow window;
        showWithFocusOnItems(window, QUrl::fromLocalFile(m_root.path()), true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QVERIFY(!view->showHiddenFiles());
        QAbstractItemView *items = view->currentView();
        YaziPreview *preview = pane->yaziColumns()->preview();
        QVERIFY(preview);
        QTRY_VERIFY(rowNamed(items, QStringLiteral("a")).isValid());
        focusRow(items, QStringLiteral("a"));
        preview->updateNow();
        QCOMPARE(preview->item().url(), m_a);
        auto *folder = preview->findChild<FolderPreview *>(QStringLiteral("previewFolder"));
        QVERIFY(folder);
        QVERIFY(folder->isVisible());
        QCOMPARE(stripped(folder->url()), m_a);
        QCOMPARE(folder->list()->focusPolicy(), Qt::NoFocus);
        QCOMPARE(folder->list()->selectionMode(), QAbstractItemView::NoSelection);

        // Folders come first, and hidden files follow the pane.
        QTRY_VERIFY(!folder->isListing());
        QTRY_COMPARE(listNames(folder->list()), (QStringList{QStringLiteral("other"), QStringLiteral("sub"), QStringLiteral("file.txt")}));
        QVERIFY(folder->note()->isHidden());
        QVERIFY(!preview->panel()->isBusy());
        QVERIFY(preview->busyIndicator()->isHidden());
        view->setShowHiddenFiles(true);
        QTRY_COMPARE(folder->list()->count(), 4);
        QVERIFY(listNames(folder->list()).contains(QStringLiteral(".hidden")));
        view->setShowHiddenFiles(false);
        QTRY_COMPARE(folder->list()->count(), 3);
        QVERIFY(!listNames(folder->list()).contains(QStringLiteral(".hidden")));

        // A file replaces the listing with its preview.
        pane->setUrl(m_a);
        QTRY_VERIFY(!view->isLoading());
        QTRY_VERIFY(rowNamed(items, QStringLiteral("file.txt")).isValid());
        focusRow(items, QStringLiteral("file.txt"));
        preview->updateNow();
        QVERIFY(folder->isHidden());
        QVERIFY(folder->url().isEmpty());
        QCOMPARE(folder->list()->count(), 0);
    }

    void folderPreviewLimitsLongListings()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("huge")));
        constexpr int Extra = 7;
        for (int i = 0; i < FolderPreview::EntryLimit + Extra; ++i) {
            QVERIFY(writeFile(root.filePath(QStringLiteral("huge/f%1").arg(i, 4, 10, QLatin1Char('0'))), QByteArray()));
        }
        MainWindow window;
        showWithFocusOnItems(window, QUrl::fromLocalFile(root.path()), true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        YaziPreview *preview = pane->yaziColumns()->preview();
        QVERIFY(preview);
        QTRY_VERIFY(rowNamed(items, QStringLiteral("huge")).isValid());
        focusRow(items, QStringLiteral("huge"));
        preview->updateNow();
        auto *folder = preview->findChild<FolderPreview *>(QStringLiteral("previewFolder"));
        QVERIFY(folder);
        QTRY_VERIFY(!folder->isListing());
        QTRY_COMPARE(folder->entryCount(), FolderPreview::EntryLimit + Extra);
        QTRY_COMPARE(folder->list()->count(), FolderPreview::EntryLimit);
        QCOMPARE(folder->list()->item(0)->text(), QStringLiteral("f0000"));
        QCOMPARE(folder->list()->item(FolderPreview::EntryLimit - 1)->text(), QStringLiteral("f0499"));
        QVERIFY(folder->note()->isVisible());
        QCOMPARE(folder->note()->text(), QStringLiteral("and 7 more…"));
    }

    void previewCancelsWorkWhenTheFocusMoves()
    {
        MainWindow window;
        showWithFocusOnItems(window, QUrl::fromLocalFile(m_root.path()), true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        YaziPreview *preview = pane->yaziColumns()->preview();
        QVERIFY(preview);
        PreviewPanel *panel = preview->panel();
        for (const char *name : {"a", "b", "folders", "many"}) {
            QTRY_VERIFY(rowNamed(items, QLatin1String(name)).isValid());
        }
        focusRow(items, QStringLiteral("b"));
        preview->updateNow();
        QTRY_VERIFY(!panel->isBusy());
        auto *folder = preview->findChild<FolderPreview *>(QStringLiteral("previewFolder"));
        QVERIFY(folder);

        // Passing over items within the delay starts no work for them.
        QSignalSpy changed(preview, &YaziPreview::itemChanged);
        QSignalSpy busy(panel, &PreviewPanel::busyChanged);
        bool indicatorShown = false;
        connect(panel, &PreviewPanel::busyChanged, &window, [&](bool isBusy) {
            if (isBusy) {
                indicatorShown = preview->busyIndicator()->isVisible();
            }
        });
        for (const char *name : {"a", "folders", "many"}) {
            focusRow(items, QLatin1String(name));
        }
        QVERIFY(preview->isUpdatePending());
        QVERIFY(busy.isEmpty());
        QTRY_COMPARE(changed.count(), 1);
        QCOMPARE(stripped(preview->item().url()), m_many);
        QCOMPARE(stripped(folder->url()), m_many);
        QTRY_COMPARE(folder->list()->count(), ManyFiles);
        QTRY_VERIFY(!panel->isBusy());
        QVERIFY(preview->busyIndicator()->isHidden());
        QCOMPARE(busy.count(), 2);
        QCOMPARE(busy.at(0).at(0).toBool(), true);
        QCOMPARE(busy.at(1).at(0).toBool(), false);
        QVERIFY(indicatorShown);

        // Moving on while a folder is listed stops the listing and drops its entries.
        focusRow(items, QStringLiteral("folders"));
        preview->updateNow();
        QCOMPARE(stripped(folder->url()), QUrl::fromLocalFile(m_root.filePath(QStringLiteral("folders"))));
        QVERIFY(folder->isListing());
        QVERIFY(panel->isBusy());
        focusRow(items, QStringLiteral("a"));
        preview->updateNow();
        QCOMPARE(stripped(folder->url()), m_a);
        QTRY_VERIFY(!folder->isListing());
        QTest::qWait(200);
        QCOMPARE(listNames(folder->list()), (QStringList{QStringLiteral("other"), QStringLiteral("sub"), QStringLiteral("file.txt")}));
        QVERIFY(!panel->isBusy());
    }

    void informationToggleSwitchesThePreviewColumn()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        const int previous = Settings::yaziPreviewContent();
        const auto restore = qScopeGuard([previous] {
            Settings::setYaziPreviewContent(static_cast<Settings::EnumYaziPreviewContent::type>(previous));
        });
        MainWindow window;
        showWithFocusOnItems(window, m_a, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        QAction *action = window.actionCollection()->action(QStringLiteral("toggle_yazi_preview_information"));
        QVERIFY(action);
        QVERIFY(action->isCheckable());
        QVERIFY(action->shortcuts().isEmpty());
        auto *viewMenu = window.findChild<QMenu *>(QStringLiteral("view"));
        QVERIFY(viewMenu);
        QVERIFY(viewMenu->actions().contains(action));
        QVERIFY(action->isEnabled());
        QVERIFY(!action->isChecked());

        Pane *pane = window.activePane();
        YaziColumns *columns = pane->yaziColumns();
        YaziPreview *preview = columns->preview();
        QVERIFY(preview);
        QAbstractItemView *items = pane->view()->currentView();
        QTRY_VERIFY(rowNamed(items, QStringLiteral("file.txt")).isValid());
        focusRow(items, QStringLiteral("file.txt"));
        preview->updateNow();
        QCOMPARE(pane->yaziPreviewContent(), YaziMode::PreviewContent::Preview);
        auto *information = preview->findChild<QWidget *>(QStringLiteral("yaziInformation"));
        QVERIFY(information);
        QVERIFY(!information->isVisible());
        QVERIFY(preview->panel()->isVisible());

        QSignalSpy changed(pane, &Pane::yaziPreviewContentChanged);
        QSignalSpy layoutChanged(pane, &Pane::workspaceLayoutChanged);
        action->trigger();
        QCOMPARE(changed.count(), 1);
        QVERIFY(layoutChanged.count() >= 1);
        QCOMPARE(pane->yaziPreviewContent(), YaziMode::PreviewContent::Information);
        QCOMPARE(columns->previewContent(), YaziMode::PreviewContent::Information);
        QCOMPARE(preview->content(), YaziMode::PreviewContent::Information);
        QVERIFY(action->isChecked());
        QVERIFY(information->isVisible());
        QVERIFY(!preview->panel()->isVisible());
        QVERIFY(preview->busyIndicator()->isHidden());
        QCOMPARE(preview->item().url(), m_file);
        auto *title = preview->findChild<QLabel *>(QStringLiteral("yaziInformationTitle"));
        QVERIFY(title);
        QCOMPARE(title->text(), QStringLiteral("file.txt"));
        QStringList values;
        const auto labels = preview->information()->findChildren<QLabel *>();
        for (const QLabel *label : labels) {
            if (label->isVisible()) {
                values.append(label->text());
            }
        }
        QVERIFY2(std::any_of(values.cbegin(),
                             values.cend(),
                             [](const QString &value) {
                                 return value.contains(QLatin1String("rw"));
                             }),
                 qPrintable(values.join(QLatin1Char('|'))));
        QVERIFY(!values.contains(QStringLiteral("Unavailable")));
        QCOMPARE(pane->saveState().yaziPreviewContent, 1);
        QCOMPARE(window.inactivePane()->saveState().yaziPreviewContent, -1);
        QCOMPARE(window.inactivePane()->yaziPreviewContent(), YaziMode::PreviewContent::Preview);

        // The Vim key i switches back.
        QVERIFY(items->hasFocus());
        pressOnFocus(Qt::Key_I);
        QCOMPARE(pane->yaziPreviewContent(), YaziMode::PreviewContent::Preview);
        QVERIFY(!action->isChecked());
        QVERIFY(preview->panel()->isVisible());
        QVERIFY(!information->isVisible());
        QCOMPARE(pane->saveState().yaziPreviewContent, 0);

        // The choice is restored; without one, panes follow the setting.
        PaneSession state = pane->saveState();
        state.yaziPreviewContent = 1;
        pane->replaceState(state);
        QCOMPARE(pane->yaziPreviewContent(), YaziMode::PreviewContent::Information);
        QVERIFY(action->isChecked());
        QCOMPARE(pane->saveState(), state);
        Settings::setYaziPreviewContent(Settings::EnumYaziPreviewContent::Information);
        window.applySettings();
        QCOMPARE(window.inactivePane()->yaziPreviewContent(), YaziMode::PreviewContent::Information);
        state.yaziPreviewContent = -1;
        pane->replaceState(state);
        QCOMPARE(pane->yaziPreviewContent(), YaziMode::PreviewContent::Information);
        Settings::setYaziPreviewContent(Settings::EnumYaziPreviewContent::Preview);
        window.applySettings();
        QCOMPARE(pane->yaziPreviewContent(), YaziMode::PreviewContent::Preview);
        QCOMPARE(window.inactivePane()->yaziPreviewContent(), YaziMode::PreviewContent::Preview);
        QCOMPARE(pane->saveState().yaziPreviewContent, -1);
    }

    void previewKeepsTheSizeLimits()
    {
        const int previous = Settings::textPreviewLimitKiB();
        const auto restore = qScopeGuard([previous] {
            Settings::setTextPreviewLimitKiB(previous);
        });
        Settings::setTextPreviewLimitKiB(1);
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QByteArray text;
        for (int line = 0; line < 100; ++line) {
            text.append(QByteArray(24, 'x') + '\n');
        }
        QVERIFY(writeFile(root.filePath(QStringLiteral("long.txt")), text));
        QFile image(root.filePath(QStringLiteral("huge.png")));
        QVERIFY(image.open(QIODevice::WriteOnly));
        QVERIFY(image.resize(8 * 1024 * 1024 + 1));
        image.close();
        MainWindow window;
        showWithFocusOnItems(window, QUrl::fromLocalFile(root.path()), true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        YaziPreview *preview = pane->yaziColumns()->preview();
        QVERIFY(preview);
        auto *footer = preview->findChild<QLabel *>(QStringLiteral("previewFooter"));
        auto *message = preview->findChild<QLabel *>(QStringLiteral("previewMessage"));
        auto *textView = preview->findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QVERIFY(footer && message && textView);
        QTRY_VERIFY(rowNamed(items, QStringLiteral("long.txt")).isValid());
        QTRY_VERIFY(rowNamed(items, QStringLiteral("huge.png")).isValid());

        // Work in progress shows the loading message and the busy indicator.
        QStringList loadingMessages;
        connect(preview->panel(), &PreviewPanel::busyChanged, &window, [&](bool busy) {
            if (busy && preview->busyIndicator()->isVisible()) {
                loadingMessages.append(message->text());
            }
        });
        focusRow(items, QStringLiteral("long.txt"));
        preview->updateNow();
        QTRY_VERIFY(footer->text().contains(QStringLiteral("Preview truncated.")));
        QVERIFY(textView->toPlainText().size() <= 1024);
        QTRY_VERIFY(!preview->panel()->isBusy());
        QCOMPARE(loadingMessages, QStringList{QStringLiteral("Loading preview…")});

        focusRow(items, QStringLiteral("huge.png"));
        preview->updateNow();
        QTRY_COMPARE(message->text(), QStringLiteral("This image is too large to preview."));
        QVERIFY(message->isVisible());
        QTRY_VERIFY(!preview->panel()->isBusy());
        QVERIFY(preview->busyIndicator()->isHidden());
    }

    void transferIndicatorFollowsTheQueue()
    {
        TransferQueue queue;
        YaziColumns columns;
        QToolButton *indicator = columns.transferIndicator();
        QVERIFY(indicator->isHidden());
        QVERIFY(indicator->parentWidget() == columns.modeIndicator()->parentWidget());
        columns.setTransferQueue(&queue);
        QCOMPARE(columns.transferQueue(), &queue);
        QVERIFY(indicator->isHidden());

        FakeTransfer *first = nullptr;
        FakeTransfer *second = nullptr;
        queue.enqueue(QStringLiteral("Copying one"), [&] {
            first = new FakeTransfer;
            return first;
        });
        queue.enqueue(QStringLiteral("Copying two"), [&] {
            second = new FakeTransfer;
            return second;
        });
        QTRY_VERIFY(first);
        QVERIFY(!indicator->isHidden());
        QCOMPARE(indicator->text(), QStringLiteral("1 transfer running · 0% · 1 waiting"));
        first->progress(42);
        QCOMPARE(indicator->text(), QStringLiteral("1 transfer running · 42% · 1 waiting"));
        QVERIFY(indicator->toolTip().contains(QStringLiteral("Copying one: 42%")));
        QVERIFY(indicator->toolTip().contains(QStringLiteral("Copying two: waiting")));
        QSignalSpy requested(&columns, &YaziColumns::transferQueueRequested);
        indicator->click();
        QCOMPARE(requested.count(), 1);

        first->finish();
        QTRY_VERIFY(second);
        QTRY_COMPARE(indicator->text(), QStringLiteral("1 transfer running · 0%"));
        second->finish(KJob::UserDefinedError);
        QTRY_COMPARE(indicator->text(), QStringLiteral("Transfer failed"));
        QVERIFY(indicator->toolTip().contains(QStringLiteral("Copying two")));
        QVERIFY(!indicator->isHidden());

        // Columns that start following the queue later don't report old failures.
        YaziColumns later;
        later.setTransferQueue(&queue);
        QVERIFY(later.transferIndicator()->isHidden());
        queue.clearFinished();
        columns.setTransferQueue(nullptr);
        QVERIFY(indicator->isHidden());
    }

    void transferIndicatorShowsTheWindowQueue()
    {
        MainWindow window;
        showWithFocusOnItems(window, m_a, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        TransferQueue *queue = FileOperations::transferQueue();
        YaziColumns *columns = window.activePane()->yaziColumns();
        QCOMPARE(columns->transferQueue(), queue);
        QCOMPARE(window.inactivePane()->yaziColumns()->transferQueue(), queue);
        QToolButton *indicator = columns->transferIndicator();
        QVERIFY(indicator->isHidden());

        FakeTransfer *copy = nullptr;
        queue->enqueue(QStringLiteral("Copying file.txt"), [&] {
            copy = new FakeTransfer;
            return copy;
        });
        QTRY_VERIFY(copy);
        QTRY_VERIFY(indicator->isVisible());
        copy->progress(50);
        QCOMPARE(indicator->text(), QStringLiteral("1 transfer running · 50%"));
        QCOMPARE(window.inactivePane()->yaziColumns()->transferIndicator()->text(), indicator->text());

        QVERIFY(!window.findChild<TransferQueueDialog *>());
        QTest::mouseClick(indicator, Qt::LeftButton);
        QTRY_VERIFY(window.findChild<TransferQueueDialog *>());
        auto *dialog = window.findChild<TransferQueueDialog *>();
        QTRY_VERIFY(dialog->isVisible());
        dialog->close();

        copy->finish();
        QTRY_VERIFY(indicator->isHidden());
        queue->clearFinished();
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

    void vimSlashFiltersAndTheBadgeFollowsTheFilter()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_many, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        YaziColumns *columns = pane->yaziColumns();
        auto *filterBar = pane->findChild<FilterBar *>();
        QVERIFY(filterBar);
        auto *text = filterBar->findChild<QLineEdit *>();
        QVERIFY(text);
        QTRY_COMPARE(items->model()->rowCount(), ManyFiles);
        QVERIFY(columns->filterIndicator()->isHidden());
        QVERIFY(columns->filterClearButton()->isHidden());

        // / focuses the filter, where the keys type rather than run commands.
        pressOnFocus(Qt::Key_Slash);
        QTRY_VERIFY(filterBar->isVisible());
        QCOMPARE(QApplication::focusWidget(), text);
        for (Qt::Key key : {Qt::Key_L, Qt::Key_E, Qt::Key_1, Qt::Key_9, Qt::Key_9}) {
            pressOnFocus(key);
        }
        QCOMPARE(text->text(), QStringLiteral("le199"));
        QCOMPARE(pane->url(), m_many);
        QVERIFY(!columns->keys()->isPending());
        QTRY_COMPARE(items->model()->rowCount(), 1);
        QCOMPARE(items->model()->index(0, 0).data().toString(), QStringLiteral("file199.txt"));
        QVERIFY(columns->filterIndicator()->isVisible());
        QCOMPARE(columns->filterIndicator()->text(), QStringLiteral("Filter: le199"));
        QVERIFY(columns->filterClearButton()->isVisible());

        // Escape clears the filter, as before, and returns to the current column.
        pressOnFocus(Qt::Key_Escape);
        QTRY_VERIFY(items->hasFocus());
        QVERIFY(filterBar->isHidden());
        QTRY_COMPARE(items->model()->rowCount(), ManyFiles);
        QVERIFY(columns->filterIndicator()->isHidden());

        // The badge follows a filter set elsewhere; its button and \ clear it.
        filterBar->setFilter(NameFilter(QStringLiteral("*.txt")));
        QVERIFY(columns->filterIndicator()->isVisible());
        QCOMPARE(columns->filterIndicator()->text(), QStringLiteral("Filter: *.txt"));
        columns->filterClearButton()->click();
        QVERIFY(filterBar->filter().isEmpty());
        QVERIFY(columns->filterIndicator()->isHidden());
        QVERIFY(columns->filterClearButton()->isHidden());

        filterBar->setFilter(NameFilter(QStringLiteral("file0")));
        QVERIFY(columns->filterIndicator()->isVisible());
        QTRY_COMPARE(items->model()->rowCount(), 100);
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        pressOnFocus(Qt::Key_Backslash);
        QVERIFY(filterBar->filter().isEmpty());
        QVERIFY(columns->filterIndicator()->isHidden());
        QTRY_COMPARE(items->model()->rowCount(), ManyFiles);
        QTRY_VERIFY(items->hasFocus());

        // Without a filter, \ says there is nothing to clear.
        QSignalSpy messages(pane, &Pane::statusMessage);
        pressOnFocus(Qt::Key_Backslash);
        QCOMPARE(messages.count(), 1);
    }

    void vimCommaKeysSortTheCurrentColumn()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_many, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QAbstractItemView *items = view->currentView();
        YaziColumns *columns = pane->yaziColumns();
        const int previousColumn = view->sortColumn();
        const Qt::SortOrder previousOrder = view->sortOrder();
        const auto restore = qScopeGuard([view, previousColumn, previousOrder] {
            view->setSorting(previousColumn, previousOrder);
        });
        view->setSorting(KDirModel::Name, Qt::AscendingOrder);
        QLabel *sort = columns->sortIndicator();
        QTRY_VERIFY(sort->isVisible());
        QCOMPARE(sort->accessibleName(), QStringLiteral("Sorted by Name, ascending"));
        const auto first = [items] {
            return items->model()->index(0, 0).data().toString();
        };
        QTRY_COMPARE(first(), QStringLiteral("file000.txt"));
        const auto press = [](Qt::Key key) {
            pressOnFocus(Qt::Key_Comma);
            pressOnFocus(key);
        };

        // ,r reverses the order and keeps the column.
        pressOnFocus(Qt::Key_Comma);
        QVERIFY(columns->keys()->isPending());
        pressOnFocus(Qt::Key_R);
        QCOMPARE(view->sortColumn(), int(KDirModel::Name));
        QCOMPARE(view->sortOrder(), Qt::DescendingOrder);
        QTRY_COMPARE(first(), QStringLiteral("file199.txt"));
        QCOMPARE(sort->accessibleName(), QStringLiteral("Sorted by Name, descending"));

        // The column keys keep the order.
        press(Qt::Key_S);
        QCOMPARE(view->sortColumn(), int(KDirModel::Size));
        QCOMPARE(view->sortOrder(), Qt::DescendingOrder);
        QCOMPARE(sort->accessibleName(), QStringLiteral("Sorted by Size, descending"));
        press(Qt::Key_R);
        QCOMPARE(view->sortOrder(), Qt::AscendingOrder);
        QTRY_COMPARE(first(), QStringLiteral("file000.txt"));
        QCOMPARE(sort->accessibleName(), QStringLiteral("Sorted by Size, ascending"));
        press(Qt::Key_M);
        QCOMPARE(view->sortColumn(), int(KDirModel::ModifiedTime));
        QCOMPARE(sort->accessibleName(), QStringLiteral("Sorted by Modified, ascending"));
        press(Qt::Key_E);
        QCOMPARE(view->sortColumn(), int(KDirModel::Type));
        QCOMPARE(sort->accessibleName(), QStringLiteral("Sorted by Type, ascending"));
        press(Qt::Key_N);
        QCOMPARE(view->sortColumn(), int(KDirModel::Name));
        QCOMPARE(view->sortOrder(), Qt::AscendingOrder);
        QCOMPARE(sort->accessibleName(), QStringLiteral("Sorted by Name, ascending"));
        QVERIFY(items->hasFocus());

        // The label follows sorting changed outside the keys.
        view->setSorting(KDirModel::Size, Qt::DescendingOrder);
        QCOMPARE(sort->accessibleName(), QStringLiteral("Sorted by Size, descending"));
    }

    void vimTabKeysKeepTheModeAndTheColumnsFollow()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_a, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        TabModel *tabs = pane->tabs();
        YaziColumns *columns = pane->yaziColumns();
        QListView *parent = columns->parentColumn();
        const QUrl root = QUrl::fromLocalFile(m_root.path());
        const auto focusItems = [pane] {
            QAbstractItemView *items = pane->view()->currentView();
            if (!items->hasFocus()) {
                items->setFocus(Qt::OtherFocusReason);
            }
            return QTest::qWaitFor([items] {
                return items->hasFocus();
            });
        };
        QTRY_COMPARE(stripped(pane->url()), m_a);
        while (tabs->count() > 1) {
            QVERIFY(tabs->closeTab(tabs->currentIndex() == 0 ? 1 : 0));
        }
        QCOMPARE(tabs->count(), 1);
        QVERIFY(focusItems());
        QTRY_VERIFY(rowNamed(pane->view()->currentView(), QStringLiteral("sub")).isValid());
        focusRow(pane->view()->currentView(), QStringLiteral("sub"));
        QTRY_COMPARE(stripped(columns->parentUrl()), root);

        // t opens the focused folder in a new tab, still in the mode.
        pressOnFocus(Qt::Key_T);
        QTRY_COMPARE(tabs->count(), 2);
        QCOMPARE(tabs->currentIndex(), 1);
        QTRY_COMPARE(stripped(pane->url()), m_sub);
        QVERIFY(pane->yaziMode());
        QTRY_COMPARE(stripped(columns->parentUrl()), m_a);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("sub"));
        QTRY_VERIFY(pane->view()->currentView()->hasFocus());

        // [ and ], and gT and gt, go to the previous and next tabs.
        pressOnFocus(Qt::Key_BracketLeft);
        QTRY_COMPARE(tabs->currentIndex(), 0);
        QTRY_COMPARE(stripped(pane->url()), m_a);
        QVERIFY(pane->yaziMode());
        QTRY_COMPARE(stripped(columns->parentUrl()), root);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("a"));
        QVERIFY(focusItems());
        pressOnFocus(Qt::Key_BracketRight);
        QTRY_COMPARE(tabs->currentIndex(), 1);
        QTRY_COMPARE(stripped(columns->parentUrl()), m_a);
        QVERIFY(focusItems());
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_T, Qt::ShiftModifier);
        QTRY_COMPARE(tabs->currentIndex(), 0);
        QTRY_COMPARE(stripped(columns->parentUrl()), root);
        QVERIFY(focusItems());
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_T);
        QTRY_COMPARE(tabs->currentIndex(), 1);
        QTRY_COMPARE(stripped(pane->url()), m_sub);
        QVERIFY(pane->yaziMode());
        QVERIFY(columns->isVisible());

        // gw keeps a locked tab and closes an unlocked one.
        QSignalSpy messages(pane, &Pane::statusMessage);
        tabs->setLocked(1, true);
        QVERIFY(focusItems());
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_W);
        QCOMPARE(messages.count(), 1);
        QCOMPARE(tabs->count(), 2);
        tabs->setLocked(1, false);
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_W);
        QTRY_COMPARE(tabs->count(), 1);
        QTRY_COMPARE(stripped(pane->url()), m_a);
        QVERIFY(pane->yaziMode());
        QTRY_COMPARE(stripped(columns->parentUrl()), root);

        // The only tab stays, and there is no other tab to go to.
        QVERIFY(focusItems());
        pressOnFocus(Qt::Key_G);
        pressOnFocus(Qt::Key_W);
        QCOMPARE(messages.count(), 2);
        pressOnFocus(Qt::Key_BracketRight);
        QCOMPARE(messages.count(), 3);
        QCOMPARE(tabs->count(), 1);
    }

    void historyKeepsTheColumnsInStep()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_a, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        YaziColumns *columns = pane->yaziColumns();
        QListView *parent = columns->parentColumn();
        YaziPreview *preview = columns->preview();
        QVERIFY(preview);
        const QUrl root = QUrl::fromLocalFile(m_root.path());
        const auto inStepWith = [&](const QUrl &url, const QUrl &parentUrl, const QString &parentItem) {
            return QTest::qWaitFor([&] {
                       return stripped(pane->url()) == url && stripped(columns->url()) == url && stripped(columns->parentUrl()) == parentUrl
                           && parent->currentIndex().data().toString() == parentItem;
                   })
                && QTest::qWaitFor([&] {
                       const QModelIndex current = items->currentIndex();
                       const KFileItem item = preview->item();
                       return !preview->isUpdatePending() && (current.isValid() ? !item.isNull() && item.name() == current.data().toString() : item.isNull());
                   });
        };

        // Going back restores the selection, which the preview then shows.
        pane->view()->selectUrls({m_sub});
        QTRY_COMPARE(preview->item().url(), m_sub);
        pane->setUrl(m_sub);
        QVERIFY(inStepWith(m_sub, m_a, QStringLiteral("sub")));
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());

        // H and L go back and forward, with the parent column and preview following.
        pressOnFocus(Qt::Key_H, Qt::ShiftModifier);
        QVERIFY(inStepWith(m_a, root, QStringLiteral("a")));
        QTRY_COMPARE(preview->item().url(), m_sub);
        pressOnFocus(Qt::Key_L, Qt::ShiftModifier);
        QVERIFY(inStepWith(m_sub, m_a, QStringLiteral("sub")));
        QVERIFY(preview->item().isNull());

        // The pane's own history buttons do the same.
        pane->goBack();
        QVERIFY(inStepWith(m_a, root, QStringLiteral("a")));
        QTRY_COMPARE(preview->item().url(), m_sub);
        pane->goForward();
        QVERIFY(inStepWith(m_sub, m_a, QStringLiteral("sub")));
        QVERIFY(pane->yaziMode());
    }

    void vimSearchKeysOpenTheFolderSearchAndReportIt()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_a, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        QAbstractItemView *items = pane->view()->currentView();
        YaziColumns *columns = pane->yaziColumns();
        auto *panel = window.findChild<SearchPanel *>();
        QVERIFY(panel);
        SearchWidget *search = panel->searchWidget();
        QSignalSpy requested(pane, &Pane::yaziSearchRequested);
        const auto focusItems = [items] {
            items->setFocus(Qt::OtherFocusReason);
            return QTest::qWaitFor([items] {
                return items->hasFocus();
            });
        };

        // s opens the panel for a file name search of the pane's folder.
        pressOnFocus(Qt::Key_S);
        QTRY_COMPARE(requested.count(), 1);
        QCOMPARE(requested.at(0).at(0).toBool(), false);
        QTRY_VERIFY(panel->isVisible());
        QCOMPARE(stripped(search->folder()), m_a);
        QVERIFY(search->searchesCurrentFolder());
        QCOMPARE(search->field(), LiveSearch::Field::FileName);
        QTRY_VERIFY(search->isAncestorOf(QApplication::focusWidget()));
        QVERIFY(columns->searchIndicator()->isHidden());

        // S searches the contents instead.
        QVERIFY(focusItems());
        pressOnFocus(Qt::Key_S, Qt::ShiftModifier);
        QTRY_COMPARE(requested.count(), 2);
        QCOMPARE(requested.at(1).at(0).toBool(), true);
        QTRY_COMPARE(search->field(), LiveSearch::Field::Contents);
        QVERIFY(search->searchesCurrentFolder());

        // The outcome of the search shows in the status row.
        QSignalSpy finished(search, &SearchWidget::finished);
        QVERIFY(search->search(QStringLiteral("file")));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(columns->searchIndicator()->isVisible());
        QCOMPARE(columns->searchIndicator()->text(), search->status());
        QVERIFY(!columns->isSearchRunning());
        QVERIFY(columns->searchCancelButton()->isHidden());

        // While it runs, Escape in the columns cancels it.
        QVERIFY(search->prepareFolderSearch(LiveSearch::Field::FileName));
        QVERIFY(search->setFolder(QUrl::fromLocalFile(QDir::rootPath())));
        QVERIFY(search->search(QStringLiteral("a-name-that-should-not-exist-anywhere")));
        QVERIFY(search->isRunning());
        QVERIFY(columns->isSearchRunning());
        QVERIFY(columns->searchIndicator()->isVisible());
        QVERIFY(columns->searchIndicator()->text().startsWith(QStringLiteral("Searching")));
        QVERIFY(columns->searchCancelButton()->isVisible());
        QVERIFY(focusItems());
        QSignalSpy cancelRequested(columns, &YaziColumns::searchCancelRequested);
        pressOnFocus(Qt::Key_Escape);
        QCOMPARE(cancelRequested.count(), 1);
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY(!columns->isSearchRunning());
        QCOMPARE(columns->searchIndicator()->text(), search->status());
        QVERIFY(search->status().contains(QStringLiteral("canceled")));
        QVERIFY(columns->searchCancelButton()->isHidden());
        QVERIFY(items->hasFocus());

        // Without a running search, Escape is left to the columns.
        pressOnFocus(Qt::Key_Escape);
        QCOMPARE(cancelRequested.count(), 1);

        // A location the search can't handle is reported in the row.
        QVERIFY(search->setFolder(QUrl(QStringLiteral("sftp://example.invalid/path"))));
        QVERIFY(!search->search(QStringLiteral("x")));
        QTRY_VERIFY(columns->searchIndicator()->text().contains(QStringLiteral("local folders only")));
        QVERIFY(!columns->isSearchRunning());

        // A search opened another way isn't reported.
        QVERIFY(search->setFolder(m_a));
        window.actionCollection()->action(QStringLiteral("search_folder"))->trigger();
        QTRY_VERIFY(columns->searchIndicator()->isHidden());
        QVERIFY(search->search(QStringLiteral("file")));
        QTRY_COMPARE(finished.count(), 3);
        QVERIFY(columns->searchIndicator()->isHidden());
    }

    void largeFoldersStayResponsive()
    {
        constexpr int Entries = 10000;
        constexpr int Presses = 200;
        constexpr int Timeout = 120000;
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkpath(QStringLiteral("big/sub")));
        for (int i = 0; i < Entries; ++i) {
            QFile entry(root.filePath(QStringLiteral("big/f%1").arg(i, 5, 10, QLatin1Char('0'))));
            QVERIFY(entry.open(QIODevice::WriteOnly));
        }
        const QUrl big = QUrl::fromLocalFile(root.filePath(QStringLiteral("big")));
        const QUrl sub = QUrl::fromLocalFile(root.filePath(QStringLiteral("big/sub")));
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        window.resize(1400, 600);
        window.openUrls({big, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        Pane *pane = window.activePane();
        FolderView *view = pane->view();
        QTRY_COMPARE_WITH_TIMEOUT(view->itemCount(), Entries + 1, Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(!view->isLoading(), Timeout);
        view->setViewMode(FolderView::ViewMode::Details);
        pane->setYaziMode(true);
        YaziColumns *columns = pane->yaziColumns();
        QListView *parent = columns->parentColumn();
        QAbstractItemView *items = view->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        const QModelIndex first = items->model()->index(0, 0);
        QCOMPARE(first.data().toString(), QStringLiteral("sub"));
        items->selectionModel()->setCurrentIndex(first, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

        // A held j moves through the listing, selecting only the focused item.
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < Presses; ++i) {
            QTest::keyClick(items, Qt::Key_J);
        }
        QCOMPARE(items->currentIndex().row(), Presses);
        QCOMPARE(items->selectionModel()->selectedRows().size(), 1);
        QVERIFY(columns->selectionIndicator()->isHidden());
        QVERIFY2(timer.elapsed() < Timeout / 2, qPrintable(QStringLiteral("%1 ms").arg(timer.elapsed())));
        YaziPreview *preview = columns->preview();
        QVERIFY(preview);
        QTRY_VERIFY(!preview->isUpdatePending());
        QCOMPARE(preview->item().name(), items->currentIndex().data().toString());

        // The parent column lists the large folder in the background, and going back up reuses it.
        items->selectionModel()->setCurrentIndex(first, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QTest::keyClick(items, Qt::Key_L);
        QTRY_COMPARE(stripped(pane->url()), sub);
        QTRY_COMPARE_WITH_TIMEOUT(parent->model()->rowCount(), Entries + 1, Timeout);
        QTRY_COMPARE(parent->currentIndex().data().toString(), QStringLiteral("sub"));
        QVERIFY(columns->parentNote()->isHidden());
        items = view->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        QTest::keyClick(items, Qt::Key_H);
        QTRY_COMPARE(stripped(pane->url()), big);
        QTRY_COMPARE_WITH_TIMEOUT(view->itemCount(), Entries + 1, Timeout);
        QTRY_COMPARE(view->currentView()->currentIndex().data().toString(), QStringLiteral("sub"));

        // Marking every item and moving on keeps the count without recounting at each key.
        items = view->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());
        QTest::keyClick(items, Qt::Key_V, Qt::ShiftModifier);
        QTRY_VERIFY(columns->selectionIndicator()->isVisible());
        QVERIFY(columns->isMarking());
        QSignalSpy selectionChanges(items->selectionModel(), &QItemSelectionModel::selectionChanged);
        timer.restart();
        for (int i = 0; i < Presses; ++i) {
            QTest::keyClick(items, Qt::Key_J);
        }
        QVERIFY2(timer.elapsed() < Timeout / 2, qPrintable(QStringLiteral("%1 ms").arg(timer.elapsed())));
        QCOMPARE(items->currentIndex().row(), Presses);
        QCOMPARE(selectionChanges.count(), 0);
        QCOMPARE(items->selectionModel()->selectedRows().size(), Entries + 1);
        QVERIFY(columns->selectionIndicator()->accessibleName().endsWith(QStringLiteral("items selected")));
        QVERIFY(pane->yaziMode());
    }

    void unsupportedLocationsKeepOrdinaryBrowsing()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_a, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        YaziColumns *columns = pane->yaziColumns();
        QLabel *location = columns->locationIndicator();
        QCOMPARE(columns->support().level, YaziMode::SupportLevel::Full);
        QVERIFY(columns->parentColumn()->isVisible());
        QVERIFY(columns->previewSlot()->isVisible());
        QVERIFY(location->isHidden());

        // A location without a meaningful parent folder keeps only the current column.
        pane->setUrl(QUrl(QStringLiteral("remote:/")));
        QTRY_COMPARE(pane->url().scheme(), QStringLiteral("remote"));
        QVERIFY(columns->isCollapsed());
        QVERIFY(pane->yaziMode());
        QVERIFY(columns->isVisible());
        QVERIFY(pane->view()->isVisible());
        QVERIFY(!columns->parentColumn()->isVisible());
        QVERIFY(!columns->previewSlot()->isVisible());
        QVERIFY(columns->parentUrl().isEmpty());
        QVERIFY(location->isVisible());
        QVERIFY(location->text().startsWith(QStringLiteral("Yazi mode isn't available here: ")));
        QCOMPARE(location->text(), columns->unavailableText());
        QCOMPARE(location->accessibleName(), location->text());

        // The commands that need the other columns say why they can't run.
        QSignalSpy messages(pane, &Pane::statusMessage);
        pane->runYaziCommand(YaziMode::Command::FocusParent);
        QCOMPARE(messages.count(), 1);
        QCOMPARE(messages.last().first().toString(), columns->unavailableText());
        pane->runYaziCommand(YaziMode::Command::Parent);
        QCOMPARE(messages.count(), 2);
        QCOMPARE(messages.last().first().toString(), columns->unavailableText());
        QVERIFY(columns->messageIndicator()->isVisible());
        QCOMPARE(pane->url().scheme(), QStringLiteral("remote"));

        // Returning to a local folder brings the columns back.
        pane->setUrl(m_a);
        QTRY_COMPARE(stripped(pane->url()), m_a);
        QVERIFY(!columns->isCollapsed());
        QVERIFY(location->isHidden());
        QVERIFY(columns->parentColumn()->isVisible());
        QVERIFY(columns->previewSlot()->isVisible());
        QTRY_COMPARE(stripped(columns->parentUrl()), QUrl::fromLocalFile(m_root.path()));
        QTRY_COMPARE(columns->parentColumn()->currentIndex().data().toString(), QStringLiteral("a"));

        // A remote folder is limited: its parent column loads in the background and a failure shows in the column.
        columns->setParentListingTimeout(2000);
        pane->setUrl(QUrl(QStringLiteral("ftp://example.invalid/folder/")));
        QTRY_COMPARE(columns->support().level, YaziMode::SupportLevel::Limited);
        QVERIFY(!columns->isCollapsed());
        QVERIFY(location->isVisible());
        QVERIFY(location->text().startsWith(QStringLiteral("Limited here: ")));
        QVERIFY(columns->parentColumn()->isVisible());
        QVERIFY(columns->parentNote()->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(columns->parentNote()->text() != QStringLiteral("Loading…"), 20000);
        QVERIFY(columns->parentNote()->isVisible());
        QVERIFY(columns->parentNote()->text().startsWith(QStringLiteral("Could not list the parent folder"))
                || columns->parentNote()->text() == QStringLiteral("The parent folder is taking too long to respond."));
        QCOMPARE(columns->parentColumn()->accessibleDescription(), columns->parentNote()->text());

        pane->setUrl(m_a);
        QTRY_COMPARE(stripped(pane->url()), m_a);
        QVERIFY(location->isHidden());
        QTRY_VERIFY(columns->parentNote()->isHidden());
        QVERIFY(pane->yaziMode());
    }

    void unavailableCommandsSayWhy()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_b, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        Pane *pane = window.activePane();
        YaziColumns *columns = pane->yaziColumns();
        QSignalSpy messages(pane, &Pane::statusMessage);
        const auto focusItems = [pane] {
            QAbstractItemView *items = pane->view()->currentView();
            items->setFocus(Qt::OtherFocusReason);
            return QTest::qWaitFor([items] {
                return items->hasFocus();
            });
        };
        const auto pressReports = [&](Qt::Key key, Qt::KeyboardModifiers modifiers) {
            const qsizetype before = messages.count();
            pressOnFocus(key, modifiers);
            if (!QTest::qWaitFor([&] {
                    return messages.count() > before;
                })) {
                return QString();
            }
            const QString message = messages.last().first().toString();
            const QLabel *shown = columns->messageIndicator();
            return shown->isVisible() && shown->text() == message ? message : QStringLiteral("not shown: %1").arg(message);
        };
        QVERIFY(focusItems());
        QTRY_COMPARE(pane->view()->itemCount(), 0);

        // An empty folder has nothing to move through, select or act on.
        QCOMPARE(pressReports(Qt::Key_J, {}), QStringLiteral("This column has no items to move through."));
        QCOMPARE(pressReports(Qt::Key_Space, {}), QStringLiteral("There is no item here to select."));
        QCOMPARE(pressReports(Qt::Key_R, {}), QStringLiteral("There is no item here to act on."));
        QCOMPARE(pressReports(Qt::Key_L, {}), QStringLiteral("There is no focused item to open."));

        // A new tab has no history, and the only tab stays.
        QCOMPARE(pressReports(Qt::Key_H, Qt::ShiftModifier), QStringLiteral("There is no earlier folder in this tab's history."));
        QCOMPARE(pressReports(Qt::Key_L, Qt::ShiftModifier), QStringLiteral("There is no later folder in this tab's history."));

        // p says the clipboard is empty.
        QApplication::clipboard()->clear();
        QCOMPARE(pressReports(Qt::Key_P, {}), QStringLiteral("The clipboard has nothing to paste. Copy or cut items first."));

        // c needs a second pane.
        pane->setUrl(m_a);
        QTRY_COMPARE(stripped(pane->url()), m_a);
        QTRY_VERIFY(!pane->view()->isLoading());
        QVERIFY(focusItems());
        QTRY_VERIFY(rowNamed(pane->view()->currentView(), QStringLiteral("file.txt")).isValid());
        focusRow(pane->view()->currentView(), QStringLiteral("file.txt"));
        QCOMPARE(pressReports(Qt::Key_C, {}), QStringLiteral("Copy to Other Pane needs a second pane. Turn on the dual-pane layout first."));
        QVERIFY(QFileInfo::exists(m_root.filePath(QStringLiteral("a/file.txt"))));

        // A locked tab stays.
        pane->openInNewTab(m_sub, true);
        QTRY_COMPARE(stripped(pane->url()), m_sub);
        pane->tabs()->setLocked(pane->tabs()->currentIndex(), true);
        QVERIFY(focusItems());
        pressOnFocus(Qt::Key_G);
        QCOMPARE(pressReports(Qt::Key_W, {}), QStringLiteral("This tab is locked. Unlock it to close it."));
        pane->tabs()->setLocked(pane->tabs()->currentIndex(), false);

        // A root has no parent.
        pane->setUrl(QUrl::fromLocalFile(QDir::rootPath()));
        QTRY_COMPARE(pane->url().path(), QDir::rootPath());
        QVERIFY(focusItems());
        QCOMPARE(pressReports(Qt::Key_H, {}), QStringLiteral("This folder has no parent folder to open."));
        QVERIFY(pane->yaziMode());
    }

    void statusRowIsAccessibleAndAnnouncesChanges()
    {
        Settings::setYaziKeyPreset(Settings::EnumYaziKeyPreset::Vim);
        MainWindow window;
        showWithFocusOnItems(window, m_many, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        YaziColumns *columns = pane->yaziColumns();
        QAbstractItemView *items = pane->view()->currentView();

        // Every part of the status row and the columns describes itself.
        const QList<QWidget *> described{columns->modeIndicator(),
                                         columns->selectionIndicator(),
                                         columns->filterIndicator(),
                                         columns->filterClearButton(),
                                         columns->searchIndicator(),
                                         columns->searchCancelButton(),
                                         columns->sortIndicator(),
                                         columns->transferIndicator(),
                                         columns->locationIndicator(),
                                         columns->messageIndicator(),
                                         columns->parentColumn(),
                                         columns->previewSlot(),
                                         columns->modeIndicator()->parentWidget()};
        for (const QWidget *widget : described) {
            QVERIFY2(!widget->accessibleDescription().isEmpty(), qPrintable(widget->objectName()));
        }
        QCOMPARE(columns->modeIndicator()->parentWidget()->accessibleName(), QStringLiteral("Yazi mode status"));
        for (const QWidget *widget : {static_cast<QWidget *>(columns->modeIndicator()),
                                      static_cast<QWidget *>(columns->sortIndicator()),
                                      static_cast<QWidget *>(columns->filterClearButton()),
                                      static_cast<QWidget *>(columns->searchCancelButton()),
                                      static_cast<QWidget *>(columns->parentColumn()),
                                      columns->previewSlot()}) {
            QVERIFY2(!widget->accessibleName().isEmpty(), qPrintable(widget->objectName()));
        }
        QVERIFY(columns->sortIndicator()->accessibleName().startsWith(QStringLiteral("Sorted by")));

        // The parts shown on demand take their names when shown.
        PaneSession filtered = pane->saveState();
        filtered.filterText = QStringLiteral("file00");
        pane->replaceState(filtered);
        QTRY_VERIFY(columns->filterIndicator()->isVisible());
        QVERIFY(columns->filterIndicator()->accessibleName().contains(QStringLiteral("file00")));
        columns->setSearchStatus(QStringLiteral("Searching…"), true);
        QVERIFY(columns->searchIndicator()->accessibleName().contains(QStringLiteral("Searching")));
        columns->clearSearchStatus();

        s_announcements.clear();
        const bool wasActive = QAccessible::isActive();
        const QAccessible::UpdateHandler previous = QAccessible::installUpdateHandler(recordAnnouncements);
        QAccessible::setActive(true);
        const auto restore = qScopeGuard([previous, wasActive] {
            QAccessible::installUpdateHandler(previous);
            QAccessible::setActive(wasActive);
            s_announcements.clear();
        });

        // Turning the mode off and on is announced.
        pane->setYaziMode(false);
        QVERIFY(announced(QStringLiteral("Yazi mode off")));
        pane->setYaziMode(true);
        QVERIFY(announced(QStringLiteral("Yazi mode on")));
        items = pane->view()->currentView();
        items->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(items->hasFocus());

        // A pending key sequence is announced once.
        s_announcements.clear();
        pressOnFocus(Qt::Key_G);
        QVERIFY(announced(QStringLiteral("waiting for the keys after g")));
        QCOMPARE(columns->modeIndicator()->accessibleName(), QStringLiteral("Yazi mode with Vim-style keys, waiting for the keys after g"));
        pressOnFocus(Qt::Key_Escape);
        QCOMPARE(columns->modeIndicator()->accessibleName(), QStringLiteral("Yazi mode with Vim-style keys"));

        // A command that can't run is announced, but once while it is shown.
        s_announcements.clear();
        pressOnFocus(Qt::Key_H, Qt::ShiftModifier);
        QCOMPARE(s_announcements, QStringList{QStringLiteral("There is no earlier folder in this tab's history.")});
        QCOMPARE(columns->messageIndicator()->accessibleName(), s_announcements.first());
        pressOnFocus(Qt::Key_H, Qt::ShiftModifier);
        QCOMPARE(s_announcements.size(), 1);

        // So are an unsupported location and the return to a supported one.
        s_announcements.clear();
        pane->setUrl(QUrl(QStringLiteral("remote:/")));
        QTRY_VERIFY(columns->isCollapsed());
        QVERIFY(announced(QStringLiteral("Yazi mode isn't available here: ")));
        s_announcements.clear();
        pane->setUrl(m_a);
        QTRY_VERIFY(!columns->isCollapsed());
        QVERIFY(announced(QStringLiteral("Yazi mode is available again")));
    }

    void columnsAreReachableByKeyboardWithVisibleFocus()
    {
        MainWindow window;
        showWithFocusOnItems(window, m_sub, true);
        if (QTest::currentTestFailed()) {
            return;
        }
        Pane *pane = window.activePane();
        YaziColumns *columns = pane->yaziColumns();
        QAbstractItemView *items = pane->view()->currentView();
        QTRY_VERIFY(columns->preview());
        QTRY_COMPARE(columns->parentColumn()->currentIndex().data().toString(), QStringLiteral("sub"));

        // The columns take the focus in order, left to right, with the window's Yazi Columns actions.
        const auto trigger = [&window](const char *name) {
            QAction *action = window.actionCollection()->action(QLatin1String(name));
            QVERIFY(action);
            QVERIFY(action->isEnabled());
            action->trigger();
        };
        trigger("yazi_focus_parent_column");
        QTRY_VERIFY(columns->parentColumn()->hasFocus());
        QCOMPARE(columns->focusedColumn(), YaziColumns::Column::Parent);
        trigger("yazi_focus_preview");
        QTRY_COMPARE(columns->focusedColumn(), YaziColumns::Column::Preview);
        trigger("yazi_focus_current_column");
        QTRY_VERIFY(items->hasFocus());

        // Tab moves from the parent column to the current column, as the columns are laid out.
        QWidget *next = columns->parentColumn()->nextInFocusChain();
        while (next != items && next != columns->parentColumn() && !(next->isVisible() && (next->focusPolicy() & Qt::TabFocus))) {
            next = next->nextInFocusChain();
        }
        QCOMPARE(next, static_cast<QWidget *>(items));

        // No style sheet hides the focus, and the columns use the palette rather than fixed colours.
        for (const QWidget *widget : {static_cast<QWidget *>(columns), static_cast<QWidget *>(columns->parentColumn()), columns->previewSlot()}) {
            QVERIFY2(!widget->styleSheet().contains(QStringLiteral("outline")), qPrintable(widget->objectName()));
            QVERIFY2(!widget->styleSheet().contains(QStringLiteral("color")), qPrintable(widget->objectName()));
        }
        const QList<QLabel *> labels = columns->findChildren<QLabel *>();
        for (const QLabel *label : labels) {
            QVERIFY2(label->styleSheet().isEmpty(), qPrintable(label->objectName()));
        }
        QVERIFY(items->focusPolicy() & Qt::TabFocus);
        QVERIFY(columns->parentColumn()->focusPolicy() & Qt::TabFocus);
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
