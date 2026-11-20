// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "batchrenamedialog.h"
#include "checksumdialog.h"
#include "combinedpanel.h"
#include "contentchartdialog.h"
#include "customcommanddialog.h"
#include "duplicatefinderdialog.h"
#include "filediffdialog.h"
#include "fileoperations.h"
#include "filesplitdialog.h"
#include "filterbar.h"
#include "foldercomparisondialog.h"
#include "foldersyncdialog.h"
#include "foldertree.h"
#include "folderview.h"
#include "gallerywindow.h"
#include "metadataeditdialog.h"
#include "pane.h"
#include "previewpanel.h"
#include "settings.h"
#include "settingsdialog.h"
#include "tabmodel.h"
#include "transferqueuedialog.h"
#include "viewsettings.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KDirModel>
#include <KFileItemDelegate>
#include <KFilePlacesView>
#include <KSharedConfig>
#include <KTextEditor/Document>

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>

class MainWindowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("a/sub")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("b")));
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("c")));
        QFile file(m_root.filePath(QStringLiteral("a/file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        m_a = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a")));
        m_b = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("b")));
        m_c = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("c")));
    }

    void opensFilteredFolderComparison()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(root.path()).mkdir(QStringLiteral("right")));
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.filePath(QStringLiteral("left"))), QUrl::fromLocalFile(root.filePath(QStringLiteral("right")))});
        auto *action = window.actionCollection()->action(QStringLiteral("compare_folders_filtered"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<FolderSyncDialog *>(QStringLiteral("folderSyncDialog"));
        QVERIFY(dialog);
        dialog->close();
    }

    void opensTransferQueue()
    {
        MainWindow window;
        auto *action = window.actionCollection()->action(QStringLiteral("transfer_queue"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<TransferQueueDialog *>(QStringLiteral("transferQueueDialog"));
        QVERIFY(dialog);
        dialog->close();
    }

    void opensCustomCommandForLocalFolder()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        QAction *action = window.actionCollection()->action(QStringLiteral("custom_command"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<CustomCommandDialog *>(QStringLiteral("customCommandDialog"));
        QVERIFY(dialog);
        dialog->close();
    }

    void opensMetadataEditorForOneLocalFile()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile file(root.filePath(QStringLiteral("file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *action = window.actionCollection()->action(QStringLiteral("edit_metadata"));
        QVERIFY(action && !action->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        QTRY_VERIFY(action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<MetadataEditDialog *>(QStringLiteral("metadataEditDialog"));
        QVERIFY(dialog);
    }

    void opensBatchRenameForMultipleLocalItems()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString firstPath = root.filePath(QStringLiteral("a.txt"));
        const QString secondPath = root.filePath(QStringLiteral("b.txt"));
        for (const QString &path : {firstPath, secondPath}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectAll();
        QTRY_COMPARE(view->selectedItems().size(), 2);
        QAction *rename = window.actionCollection()->action(QStringLiteral("rename"));
        QVERIFY(rename && rename->isEnabled());
        rename->trigger();
        auto *dialog = window.findChild<BatchRenameDialog *>(QStringLiteral("batchRenameDialog"));
        QVERIFY(dialog);
        dialog->close();
    }

    void selectsVisibleItemsByPathFilter()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (const QString &name : {QStringLiteral("keep.txt"), QStringLiteral("other.txt")}) {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(root.path())});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        QTRY_VERIFY(!view->isLoading());
        auto *action = window.actionCollection()->action(QStringLiteral("select_by_path_filter"));
        QVERIFY(action);
        QTimer::singleShot(0, [&] {
            auto *dialog = window.findChild<QDialog *>(QStringLiteral("selectionPathDialog"));
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>(QStringLiteral("selectionPathFilter"))->setText(QStringLiteral("keep.txt"));
            auto *excluded = dialog->findChild<QPlainTextEdit *>(QStringLiteral("selectionExcludedFolders"));
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            excluded->setPlainText(QStringLiteral("relative"));
            buttons->button(QDialogButtonBox::Ok)->click();
            QVERIFY(!dialog->findChild<QLabel *>(QStringLiteral("selectionPathError"))->text().isEmpty());
            QVERIFY(view->selectedItems().isEmpty());
            excluded->clear();
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        action->trigger();
        QCOMPARE(view->selectedItems().size(), 1);
        QCOMPARE(view->selectedItems().first().name(), QStringLiteral("keep.txt"));
    }

    void startupFoldersAndPaneDefaultsApply()
    {
        KConfigGroup startup = KSharedConfig::openConfig()->group(QStringLiteral("Startup"));
        startup.writeEntry("StartupMode", QStringLiteral("FixedFolders"));
        startup.writeEntry("LeftStartUrl", m_a.toString());
        startup.writeEntry("RightStartUrl", m_b.toString());
        KSharedConfig::openConfig()->group(QStringLiteral("Panes")).writeEntry("DualPaneAtStartup", QStringLiteral("Never"));
        KSharedConfig::openConfig()->group(QStringLiteral("Panes")).writeEntry("NewTabsAfterCurrent", true);
        KSharedConfig::openConfig()->group(QStringLiteral("Navigation")).writeEntry("FollowSymlinks", true);
        KSharedConfig::openConfig()->group(QStringLiteral("Views")).writeEntry("DefaultViewMode", QStringLiteral("Icons"));
        KSharedConfig::openConfig()->group(QStringLiteral("Views")).writeEntry("ShowHiddenFiles", true);
        KSharedConfig::openConfig()->sync();
        Settings::self()->load();

        MainWindow window;
        window.openUrls({});
        QCOMPARE(window.activePane()->url(), m_a);
        QCOMPARE(window.inactivePane()->url(), m_b);
        QVERIFY(!window.actionCollection()->action(QStringLiteral("dual_layout"))->isChecked());
        QCOMPARE(window.activePane()->view()->viewMode(), FolderView::ViewMode::Icons);
        QVERIFY(window.activePane()->view()->showHiddenFiles());

        Pane *pane = window.activePane();
        pane->openInNewTab(m_b, true);
        pane->tabs()->setCurrentIndex(0);
        pane->openInNewTab(m_c, true);
        QCOMPARE(pane->tabs()->urls(), (QList<QUrl>{m_a, m_c, m_b}));

        const QString link = m_root.filePath(QStringLiteral("link-to-b"));
        QVERIFY(QFile::link(m_b.toLocalFile(), link));
        pane->setUrl(QUrl::fromLocalFile(link));
        QCOMPARE(pane->url(), m_b);
    }

    void init()
    {
        // Start from a clean session so the first window does not inherit
        // tabs or layout from an earlier test.
        QFile::remove(m_root.filePath(QStringLiteral("a/.mfm.json")));
        QFile::remove(m_root.filePath(QStringLiteral("b/.mfm.json")));
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("Session"));
        KSharedConfig::openStateConfig()->sync();
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("PopularFolders"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("SelectionFilters"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("View"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("PanelSettings"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Archives"));
        KSharedConfig::openConfig()->deleteGroup(QStringLiteral("Tabs"));
        for (const QString &group : {QStringLiteral("Startup"),
                                     QStringLiteral("Panes"),
                                     QStringLiteral("Navigation"),
                                     QStringLiteral("Previews"),
                                     QStringLiteral("PreviewSettings"),
                                     QStringLiteral("Confirmations"),
                                     QStringLiteral("StatusBar"),
                                     QStringLiteral("Views"),
                                     QStringLiteral("ContextMenu")}) {
            KSharedConfig::openConfig()->deleteGroup(group);
        }
        KSharedConfig::openConfig()->group(QStringLiteral("Confirmations")).writeEntry("ConfirmClosingMultipleTabs", false);
        KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("WindowLayout"));
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.deleteEntry("IgnoreList");
        files.deleteEntry("ExtensionColors");
        KSharedConfig::openConfig()->sync();
        Settings::self()->setDefaults();
        Settings::setConfirmClosingMultipleTabs(false);
        Settings::self()->save();
    }

    void opensCommandLinePathsInBothPanes()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QCOMPARE(window.activePane()->url(), m_a);
        QCOMPARE(window.inactivePane()->url(), m_b);
        QTRY_COMPARE(window.activePane()->view()->itemCount(), 2);
        QTRY_COMPARE(window.inactivePane()->view()->itemCount(), 0);
    }

    void previewFollowsSelectionAndRestoresLayout()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Pane *left = window.activePane();
        Pane *right = window.inactivePane();
        QTRY_COMPARE(left->view()->itemCount(), 2);
        auto *toggle = window.actionCollection()->action(QStringLiteral("show_preview_panel"));
        auto *panel = window.findChild<PreviewPanel *>(QStringLiteral("previewPanel"));
        QVERIFY(toggle && panel);
        QVERIFY(toggle->shortcuts().contains(QKeySequence(Qt::Key_F3)));
        toggle->trigger();
        QVERIFY(panel->isVisible());
        QVERIFY(left->isVisible());
        QVERIFY(!right->isVisible());
        left->view()->selectUrls({QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")))});
        auto *text = panel->findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QVERIFY(text);
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("hello"));
        left->view()->selectAll();
        QTRY_VERIFY(panel->findChild<QLabel *>(QStringLiteral("previewMessage"))->isVisible());
        window.actionCollection()->action(QStringLiteral("switch_pane"))->trigger();
        QCOMPARE(window.activePane(), right);
        QVERIFY(right->isVisible());
        QVERIFY(!left->isVisible());
        toggle->trigger();
        QVERIFY(!panel->isVisible());
        QVERIFY(left->isVisible());
        QVERIFY(right->isVisible());
    }

    void galleryUsesSortedItemsAndActivationSource()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage image(20, 10, QImage::Format_RGB32);
        image.fill(Qt::red);
        const QUrl alpha = QUrl::fromLocalFile(dir.filePath(QStringLiteral("alpha.png")));
        const QUrl beta = QUrl::fromLocalFile(dir.filePath(QStringLiteral("beta.png")));
        const QUrl gamma = QUrl::fromLocalFile(dir.filePath(QStringLiteral("gamma.png")));
        QVERIFY(image.save(alpha.toLocalFile()));
        QVERIFY(image.save(beta.toLocalFile()));
        QVERIFY(image.save(gamma.toLocalFile()));
        MainWindow window;
        window.openUrls({m_a, QUrl::fromLocalFile(dir.path())});
        window.show();
        Pane *other = window.inactivePane();
        FolderView *view = other->view();
        QTRY_COMPARE(view->itemCount(), 3);
        view->setSorting(KDirModel::Name, Qt::DescendingOrder);
        view->selectUrls({beta});
        Settings::setFileActivationDestination(Settings::EnumFileActivationDestination::Gallery);
        Q_EMIT view->itemActivated(view->selectedItems().first());
        QCOMPARE(window.activePane(), other);
        auto *gallery = window.findChild<GalleryWindow *>(QStringLiteral("galleryWindow"));
        QVERIFY(gallery);
        auto *filmstrip = gallery->findChild<QListWidget *>(QStringLiteral("galleryFilmstrip"));
        QVERIFY(filmstrip);
        QCOMPARE(filmstrip->count(), 3);
        QCOMPARE(filmstrip->item(0)->text(), QStringLiteral("gamma.png"));
        QCOMPARE(filmstrip->item(2)->text(), QStringLiteral("alpha.png"));
        QCOMPARE(filmstrip->currentRow(), 1);
        filmstrip->setCurrentRow(2);
        QCOMPARE(view->selectedItems().first().url(), alpha);
        gallery->close();
        QTRY_VERIFY(!window.findChild<GalleryWindow *>());
        view->selectUrls({alpha, gamma});
        window.actionCollection()->action(QStringLiteral("open_gallery"))->trigger();
        gallery = window.findChild<GalleryWindow *>(QStringLiteral("galleryWindow"));
        QVERIFY(gallery);
        filmstrip = gallery->findChild<QListWidget *>(QStringLiteral("galleryFilmstrip"));
        QCOMPARE(filmstrip->count(), 2);
        QCOMPARE(filmstrip->item(0)->text(), QStringLiteral("gamma.png"));
        QCOMPARE(filmstrip->item(1)->text(), QStringLiteral("alpha.png"));
        gallery->close();
    }

    void previewActivationPreservesFolderNavigation()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        const QUrl text = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        const QUrl folder = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub")));
        Settings::setFileActivationDestination(Settings::EnumFileActivationDestination::Preview);
        view->selectUrls({text});
        Q_EMIT view->itemActivated(view->selectedItems().first());
        auto *panel = window.findChild<PreviewPanel *>(QStringLiteral("previewPanel"));
        QVERIFY(panel->isVisible());
        auto *content = panel->findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(content->toPlainText(), QStringLiteral("hello"));
        view->selectUrls({folder});
        Q_EMIT view->itemActivated(view->selectedItems().first());
        QTRY_COMPARE(window.activePane()->url(), folder);
    }

    void previewStopsPlaybackOnSelectionChange()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile audio(dir.filePath(QStringLiteral("audio.wav")));
        QVERIFY(audio.open(QIODevice::WriteOnly));
        const QByteArray header = QByteArray::fromHex("52494646641f000057415645666d74201000000001000100401f0000401f00000100080064617461401f0000");
        QCOMPARE(audio.write(header), header.size());
        QCOMPARE(audio.write(QByteArray(8000, char(0x80))), 8000);
        audio.close();
        QFile text(dir.filePath(QStringLiteral("next.txt")));
        QVERIFY(text.open(QIODevice::WriteOnly));
        QCOMPARE(text.write("next"), 4);
        text.close();
        PreviewPanel panel;
        panel.show();
        panel.setItems({KFileItem(QUrl::fromLocalFile(audio.fileName()))});
        QPointer<QMediaPlayer> player = panel.findChild<QMediaPlayer *>(QStringLiteral("mediaPlayer"));
        QVERIFY(player);
        QCOMPARE(player->playbackState(), QMediaPlayer::StoppedState);
        panel.setItems({KFileItem(QUrl::fromLocalFile(text.fileName()))});
        QVERIFY(player.isNull());
        auto *preview = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(preview->toPlainText(), QStringLiteral("next"));
    }

    void previewMarkupDoesNotLoadExternalResources()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString imagePath = dir.filePath(QStringLiteral("private.png"));
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));
        QFile html(dir.filePath(QStringLiteral("page.html")));
        QVERIFY(html.open(QIODevice::WriteOnly));
        const QByteArray markup = QByteArray("<h1>Hello</h1><img src=\"") + QUrl::fromLocalFile(imagePath).toEncoded() + QByteArray("\">");
        QCOMPARE(html.write(markup), markup.size());
        html.close();
        PreviewPanel panel;
        panel.show();
        panel.setItems({KFileItem(QUrl::fromLocalFile(html.fileName()))});
        auto *rendered = panel.findChild<QTextBrowser *>(QStringLiteral("previewRenderedText"));
        auto *source = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        auto *mode = panel.findChild<QToolButton *>(QStringLiteral("previewRenderMode"));
        QVERIFY(rendered && source && mode);
        QTRY_VERIFY(rendered->isVisible());
        QVERIFY(rendered->toPlainText().contains(QStringLiteral("Hello")));
        const QVariant resource = rendered->document()->resource(QTextDocument::ImageResource, QUrl::fromLocalFile(imagePath));
        QCOMPARE(resource.metaType(), QMetaType::fromType<QImage>());
        QVERIFY(resource.value<QImage>().isNull());
        mode->click();
        QCOMPARE(source->toPlainText().toUtf8(), markup);
    }

    void previewPresentationSettings()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile html(dir.filePath(QStringLiteral("page.html")));
        QVERIFY(html.open(QIODevice::WriteOnly));
        QCOMPARE(html.write("<h1>Hello</h1>"), 14);
        html.close();
        QFile markdown(dir.filePath(QStringLiteral("page.md")));
        QVERIFY(markdown.open(QIODevice::WriteOnly));
        QCOMPARE(markdown.write("# Hello"), 7);
        markdown.close();
        QFile code(dir.filePath(QStringLiteral("code.cpp")));
        QVERIFY(code.open(QIODevice::WriteOnly));
        QCOMPARE(code.write("int main() { return 42; }\n"), 26);
        code.close();
        Settings::setPreviewFormatted(false);
        Settings::setPreviewLineNumbers(false);
        Settings::setPreviewSyntaxHighlighting(false);
        PreviewPanel panel;
        panel.show();
        auto *source = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        auto *rendered = panel.findChild<QTextBrowser *>(QStringLiteral("previewRenderedText"));
        auto *mode = panel.findChild<QToolButton *>(QStringLiteral("previewRenderMode"));
        auto *lineNumbers = panel.findChild<QWidget *>(QStringLiteral("previewLineNumbers"));
        QVERIFY(source && rendered && mode && lineNumbers);
        panel.setItems({KFileItem(QUrl::fromLocalFile(html.fileName()))});
        QTRY_VERIFY(source->isVisible());
        QVERIFY(!mode->isChecked());
        QVERIFY(lineNumbers->isHidden());
        mode->click();
        QVERIFY(rendered->isVisible());
        panel.setItems({KFileItem(QUrl::fromLocalFile(markdown.fileName()))});
        QTRY_VERIFY(source->isVisible());
        QVERIFY(!mode->isChecked());
        Settings::setPreviewFormatted(true);
        panel.applyPresentationSettings();
        QVERIFY(rendered->isVisible());
        QVERIFY(mode->isChecked());

        panel.setItems({KFileItem(QUrl::fromLocalFile(code.fileName()))});
        QTRY_VERIFY(source->isVisible());
        QTRY_COMPARE(source->toPlainText(), QStringLiteral("int main() { return 42; }\n"));
        QVERIFY(source->document()->firstBlock().layout()->formats().isEmpty());
        Settings::setPreviewLineNumbers(true);
        Settings::setPreviewSyntaxHighlighting(true);
        panel.applyPresentationSettings();
        QVERIFY(lineNumbers->isVisible());
        QTRY_VERIFY(!source->document()->firstBlock().layout()->formats().isEmpty());
        Settings::setPreviewSyntaxHighlighting(false);
        panel.applyPresentationSettings();
        QTRY_VERIFY(source->document()->firstBlock().layout()->formats().isEmpty());
    }

    void editorPlacementsKeepBothPanesAndDocument()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("sample.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("before"), 6);
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->setChecked(true);
        auto *open = window.actionCollection()->action(QStringLiteral("open_editor"));
        QVERIFY(open);
        open->trigger();
        auto *editor = window.findChild<EditorPanel *>(QStringLiteral("editorPanel"));
        QVERIFY(editor);
        QTRY_COMPARE(editor->document()->text(), QStringLiteral("before"));
        const auto panes = window.findChildren<Pane *>();
        QCOMPARE(panes.size(), 2);
        QVERIFY(panes.at(0)->isVisible() && panes.at(1)->isVisible());
        QVERIFY(editor->document()->setText(QStringLiteral("after")));
        auto *markdownButton = editor->findChild<QPushButton *>(QStringLiteral("editorMarkdownPreview"));
        auto *markdownPreview = editor->findChild<QTextBrowser *>(QStringLiteral("editorRenderedMarkdown"));
        QVERIFY(markdownButton && markdownPreview);
        markdownButton->setChecked(true);
        QVERIFY(editor->document()->setText(QStringLiteral("# Live heading")));
        QCOMPARE(markdownPreview->toPlainText().trimmed(), QStringLiteral("Live heading"));
        QVERIFY(editor->document()->setText(QStringLiteral("after")));
        const QStringList placements{QStringLiteral("editorPlaceBelow"),
                                     QStringLiteral("editorPlaceRight"),
                                     QStringLiteral("editorPlaceWindow"),
                                     QStringLiteral("editorPlaceMaximized"),
                                     QStringLiteral("editorPlacePanel")};
        for (const QString &name : placements) {
            auto *action = editor->findChild<QAction *>(name);
            QVERIFY(action);
            action->trigger();
            QCOMPARE(editor->document()->text(), QStringLiteral("after"));
            if (name != QLatin1String("editorPlaceWindow") && name != QLatin1String("editorPlaceMaximized")) {
                QVERIFY(panes.at(0)->isVisible() && panes.at(1)->isVisible());
            }
        }
        QVERIFY(editor->document()->documentSave());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray saved = file.readAll();
        QVERIFY(saved == QByteArray("after") || saved == QByteArray("after\n"));
    }

    void editorUsesConfiguredOpeningPlacement()
    {
        Settings::setEditorOpening(Settings::EnumEditorOpening::Embedded);
        Settings::setEditorEmbeddedPlacement(Settings::EnumEditorEmbeddedPlacement::Right);
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        window.actionCollection()->action(QStringLiteral("open_editor"))->trigger();
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("editorDock"));
        QVERIFY(dock);
        QVERIFY(dock->isVisible());
        QCOMPARE(window.dockWidgetArea(dock), Qt::RightDockWidgetArea);
        Settings::setEditorOpening(Settings::EnumEditorOpening::Window);
        window.actionCollection()->action(QStringLiteral("open_editor"))->trigger();
        QVERIFY(dock->isVisible());
        dock->hide();
        window.actionCollection()->action(QStringLiteral("open_editor"))->trigger();
        auto *editorWindow = window.findChild<QDialog *>(QStringLiteral("editorWindow"));
        QVERIFY(editorWindow && editorWindow->isVisible());
        QVERIFY(!dock->isVisible());
    }

    void fileComparison()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("-first.txt"));
        const QString second = dir.filePath(QStringLiteral("second.txt"));
        QFile a(first);
        QFile b(second);
        QVERIFY(a.open(QIODevice::WriteOnly));
        QCOMPARE(a.write("same\nold\n"), 9);
        a.close();
        QVERIFY(b.open(QIODevice::WriteOnly));
        QCOMPARE(b.write("same\nnew\n"), 9);
        b.close();
        QVERIFY(!QStandardPaths::findExecutable(QStringLiteral("diff")).isEmpty());

        FileDiffDialog different(first, second);
        auto *status = different.findChild<QLabel *>(QStringLiteral("fileDiffStatus"));
        auto *output = different.findChild<QPlainTextEdit *>(QStringLiteral("fileDiffOutput"));
        QVERIFY(status && output);
        QTRY_COMPARE(status->text(), QStringLiteral("The files differ."));
        QVERIFY(output->toPlainText().contains(QStringLiteral("-old")));
        QVERIFY(output->toPlainText().contains(QStringLiteral("+new")));

        FileDiffDialog identical(first, first);
        QTRY_COMPARE(identical.findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text(), QStringLiteral("The files are identical."));
        FileDiffDialog missing(first, dir.filePath(QStringLiteral("missing")));
        QTRY_VERIFY(missing.findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text().startsWith(QStringLiteral("The comparison failed:")));

        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toLocal8Bit());
        FileDiffDialog missingTool(first, second);
        qputenv("PATH", path);
        QTRY_VERIFY(missingTool.findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text().startsWith(QStringLiteral("Could not start diff:")));

        QVERIFY(b.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(b.write(QByteArray(5 * 1024 * 1024, 'x')), qint64(5 * 1024 * 1024));
        b.close();
        FileDiffDialog large(first, second);
        QTRY_COMPARE(large.findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text(),
                     QStringLiteral("The comparison output exceeded 4 MiB and was stopped."));
        QVERIFY(large.findChild<QPlainTextEdit *>(QStringLiteral("fileDiffOutput"))->toPlainText().isEmpty());
    }

    void compareFilesActionUsesBothPanes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("right")));
        const QString first = dir.filePath(QStringLiteral("left/first.txt"));
        const QString second = dir.filePath(QStringLiteral("right/second.txt"));
        QFile a(first);
        QFile b(second);
        QVERIFY(a.open(QIODevice::WriteOnly));
        a.write("first\n");
        a.close();
        QVERIFY(b.open(QIODevice::WriteOnly));
        b.write("second\n");
        b.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.filePath(QStringLiteral("left"))), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")))});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        auto *left = window.activePane()->view();
        auto *right = window.inactivePane()->view();
        QTRY_COMPARE(left->itemCount(), 1);
        QTRY_COMPARE(right->itemCount(), 1);
        auto *action = window.actionCollection()->action(QStringLiteral("compare_files"));
        QVERIFY(action && !action->isEnabled());
        left->selectUrls({QUrl::fromLocalFile(first)});
        QVERIFY(!action->isEnabled());
        right->selectUrls({QUrl::fromLocalFile(second)});
        QTRY_VERIFY(action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<FileDiffDialog *>(QStringLiteral("fileDiffDialog"));
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text(), QStringLiteral("The files differ."));
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->setChecked(true);
        QVERIFY(!action->isEnabled());
        window.actionCollection()->action(QStringLiteral("show_preview_panel"))->setChecked(false);
        QVERIFY(action->isEnabled());
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        QVERIFY(!action->isEnabled());
        QPointer<FileDiffDialog> closedDialog = dialog;
        dialog->close();
        QTRY_VERIFY(closedDialog.isNull());
        QFile localSecond(dir.filePath(QStringLiteral("left/third.txt")));
        QVERIFY(localSecond.open(QIODevice::WriteOnly));
        localSecond.write("third\n");
        localSecond.close();
        QTRY_COMPARE(left->itemCount(), 2);
        left->selectUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(localSecond.fileName())});
        QTRY_VERIFY(action->isEnabled());
        action->trigger();
        auto *samePaneDialog = window.findChild<FileDiffDialog *>(QStringLiteral("fileDiffDialog"));
        QVERIFY(samePaneDialog);
        QTRY_COMPARE(samePaneDialog->findChild<QLabel *>(QStringLiteral("fileDiffStatus"))->text(), QStringLiteral("The files differ."));
    }

    void createsSymbolicLinkFromSelection()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile source(dir.filePath(QStringLiteral("source.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("source");
        source.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        QAction *action = window.actionCollection()->action(QStringLiteral("create_symlink"));
        QVERIFY(action && !action->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(source.fileName())});
        QTRY_VERIFY(action->isEnabled());
        QTimer::singleShot(0, [] {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->textValue(), QStringLiteral("source.txt - link"));
            dialog->setTextValue(QStringLiteral("alias"));
            dialog->accept();
        });
        action->trigger();
        const QString link = dir.filePath(QStringLiteral("alias"));
        QTRY_VERIFY(QFileInfo(link).isSymLink());
        QCOMPARE(QFileInfo(link).symLinkTarget(), source.fileName());
    }

    void exportsFilteredListingInColumnOrder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const QString &name : {QStringLiteral("z.txt"), QStringLiteral("a.txt"), QStringLiteral("hidden.bin")}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("data");
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 3);
        QTRY_VERIFY(!view->isLoading());
        view->setViewMode(FolderView::ViewMode::Details);
        view->setNameFilter(NameFilter(QStringLiteral("*.txt")));
        view->setSorting(KDirModel::Name, Qt::AscendingOrder);
        auto *details = qobject_cast<QTreeView *>(view->currentView());
        QVERIFY(details);
        view->setColumnVisible(KDirModel::Size, true);
        details->header()->moveSection(details->header()->visualIndex(KDirModel::Size), 0);
        const ListingExport::Table table = view->listingForExport();
        QCOMPARE(table.rows.size(), 2);
        QCOMPARE(table.headers.first(), details->model()->headerData(KDirModel::Size, Qt::Horizontal).toString());
        const int nameColumn = table.headers.indexOf(details->model()->headerData(KDirModel::Name, Qt::Horizontal).toString());
        QVERIFY(nameColumn >= 0);
        QCOMPARE(table.rows.at(0).at(nameColumn), QStringLiteral("a.txt"));
        QCOMPARE(table.rows.at(1).at(nameColumn), QStringLiteral("z.txt"));
        const QString output = dir.filePath(QStringLiteral("listing.csv"));
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->selectFile(dir.filePath(QStringLiteral("listing")));
            QMetaObject::invokeMethod(dialog, "accept");
        });
        window.actionCollection()->action(QStringLiteral("export_listing_csv"))->trigger();
        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray csv = file.readAll();
        QVERIFY(csv.contains("a.txt"));
        QVERIFY(csv.contains("z.txt"));
        QVERIFY(!csv.contains("hidden.bin"));
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->selectFile(dir.filePath(QStringLiteral("listing")));
            QMetaObject::invokeMethod(dialog, "accept");
        });
        window.actionCollection()->action(QStringLiteral("export_listing_pdf"))->trigger();
        QFile pdf(dir.filePath(QStringLiteral("listing.pdf")));
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QCOMPARE(pdf.read(5), QByteArray("%PDF-"));
    }

    void contentChartUsesSelectedFolderOrCurrentFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));
        QFile file(dir.filePath(QStringLiteral("file")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        auto *action = window.actionCollection()->action(QStringLiteral("content_chart"));
        QVERIFY(action && action->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(dir.filePath(QStringLiteral("sub")))});
        action->trigger();
        QPointer<ContentChartDialog> chart = window.findChild<ContentChartDialog *>(QStringLiteral("contentChartDialog"));
        QVERIFY(chart);
        QCOMPARE(chart->findChild<QLabel *>(QStringLiteral("contentChartFolder"))->text(),
                 QUrl::fromLocalFile(dir.filePath(QStringLiteral("sub"))).toDisplayString());
        chart->close();
        QTRY_VERIFY(chart.isNull());
        view->selectUrls({QUrl::fromLocalFile(file.fileName())});
        action->trigger();
        chart = window.findChild<ContentChartDialog *>(QStringLiteral("contentChartDialog"));
        QVERIFY(chart);
        QCOMPARE(chart->findChild<QLabel *>(QStringLiteral("contentChartFolder"))->text(), QUrl::fromLocalFile(dir.path()).toDisplayString());
    }

    void createsAndVerifiesChecksumFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile source(dir.filePath(QStringLiteral("source.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("source");
        source.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *create = window.actionCollection()->action(QStringLiteral("create_checksum"));
        auto *verify = window.actionCollection()->action(QStringLiteral("verify_checksum"));
        QVERIFY(create && verify && !create->isEnabled() && !verify->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(source.fileName())});
        QTRY_VERIFY(create->isEnabled());
        const QString manifest = dir.filePath(QStringLiteral("checksums.sha256"));
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->selectFile(manifest);
            QMetaObject::invokeMethod(dialog, "accept");
        });
        create->trigger();
        QPointer<ChecksumDialog> dialog = window.findChild<ChecksumDialog *>(QStringLiteral("checksumDialog"));
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>(QStringLiteral("checksumStatus"))->text(), QStringLiteral("Checksum file created."));
        QVERIFY(QFileInfo::exists(manifest));
        dialog->close();
        QTRY_VERIFY(dialog.isNull());
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectUrls({QUrl::fromLocalFile(manifest)});
        QTRY_VERIFY(verify->isEnabled());
        verify->trigger();
        dialog = window.findChild<ChecksumDialog *>(QStringLiteral("checksumDialog"));
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>(QStringLiteral("checksumStatus"))->text(), QStringLiteral("All files verified."));
        auto *results = dialog->findChild<QTreeWidget *>(QStringLiteral("checksumResults"));
        QVERIFY(results);
        QCOMPARE(results->topLevelItemCount(), 1);
        QCOMPARE(results->topLevelItem(0)->text(0), QStringLiteral("source.txt"));
        dialog->close();
        QTRY_VERIFY(dialog.isNull());
        QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Truncate));
        source.write("changed");
        source.close();
        verify->trigger();
        dialog = window.findChild<ChecksumDialog *>(QStringLiteral("checksumDialog"));
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>(QStringLiteral("checksumStatus"))->text(), QStringLiteral("Verification finished: 1 file failed."));
    }

    void opensDuplicateFinderForLocalFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        auto *action = window.actionCollection()->action(QStringLiteral("find_duplicates"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<DuplicateFinderDialog *>(QStringLiteral("duplicateFinderDialog"));
        QVERIFY(dialog);
        QTRY_VERIFY(!dialog->findChild<QPushButton *>(QStringLiteral("duplicateFinderCancel"))->isEnabled());
        QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("duplicateFinderStatus"))->text().startsWith(QStringLiteral("Found 0 groups")));
    }

    void linksPaneNavigationWithinStartingFolders()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("left/sub/child")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("right/sub/child")));
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("left/only-left")));
        const QUrl leftRoot = QUrl::fromLocalFile(dir.filePath(QStringLiteral("left")));
        const QUrl rightRoot = QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")));
        MainWindow window;
        window.openUrls({leftRoot, rightRoot});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        auto *link = window.actionCollection()->action(QStringLiteral("link_panes"));
        QVERIFY(link);
        link->setChecked(true);
        QVERIFY(link->isChecked());
        Pane *left = window.activePane();
        Pane *right = window.inactivePane();
        left->setUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/sub/child"))));
        QCOMPARE(right->url(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right/sub/child"))));
        right->setUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("right/sub"))));
        QCOMPARE(left->url(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/sub"))));
        left->setUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("left/only-left"))));
        QVERIFY(!link->isChecked());
        QCOMPARE(right->url(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right/sub"))));
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("unlinked")));
        left->setUrl(leftRoot);
        right->setUrl(rightRoot);
        link->setChecked(true);
        QVERIFY(link->isChecked());
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        QVERIFY(!link->isChecked());
        link->setChecked(true);
        QVERIFY(!link->isChecked());
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Show both panes")));
    }

    void opensSplitAndCombineForLocalFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile source(dir.filePath(QStringLiteral("source")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write("data"), qint64(4));
        source.close();
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.path())});
        window.show();
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *split = window.actionCollection()->action(QStringLiteral("split_file"));
        auto *combine = window.actionCollection()->action(QStringLiteral("combine_files"));
        QVERIFY(split && combine);
        QVERIFY(!split->isEnabled());
        QVERIFY(!combine->isEnabled());
        view->selectUrls({QUrl::fromLocalFile(source.fileName())});
        QTRY_VERIFY(split->isEnabled());
        QVERIFY(combine->isEnabled());
        split->trigger();
        auto *dialog = window.findChild<FileSplitDialog *>(QStringLiteral("fileSplitDialog"));
        QVERIFY(dialog);
        dialog->close();
        QTRY_VERIFY(!window.findChild<FileSplitDialog *>(QStringLiteral("fileSplitDialog")));
        combine->trigger();
        QVERIFY(window.findChild<FileSplitDialog *>(QStringLiteral("fileSplitDialog")));
    }

    void comparesPaneFoldersAndSelectsDifferences()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("left")));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("right")));
        for (const auto &[name, data] : QList<QPair<QString, QByteArray>>{{QStringLiteral("left/left-only"), "l"},
                                                                          {QStringLiteral("left/.hidden"), "h"},
                                                                          {QStringLiteral("right/right-only"), "r"},
                                                                          {QStringLiteral("left/shared"), "small"},
                                                                          {QStringLiteral("right/shared"), "larger!"}}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(data), qint64(data.size()));
        }
        MainWindow window;
        window.openUrls({QUrl::fromLocalFile(dir.filePath(QStringLiteral("left"))), QUrl::fromLocalFile(dir.filePath(QStringLiteral("right")))});
        window.show();
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(true);
        Pane *left = window.activePane();
        Pane *right = window.inactivePane();
        QTRY_COMPARE(left->view()->itemCount(), 2);
        QTRY_COMPARE(right->view()->itemCount(), 2);
        QAction *action = window.actionCollection()->action(QStringLiteral("compare_pane_folders"));
        QVERIFY(action && action->isEnabled());
        action->trigger();
        auto *dialog = window.findChild<FolderComparisonDialog *>(QStringLiteral("folderComparisonDialog"));
        QVERIFY(dialog);
        auto *select = dialog->findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"));
        auto *results = dialog->findChild<QTreeWidget *>(QStringLiteral("folderComparisonResults"));
        QVERIFY(select && results);
        QTRY_VERIFY(select->isEnabled());
        QCOMPARE(results->topLevelItemCount(), 4);
        select->click();
        QTRY_COMPARE(left->view()->selectedItems().size(), 2);
        QTRY_COMPARE(right->view()->selectedItems().size(), 2);
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("Some differences could not be selected")));
        left->view()->setShowHiddenFiles(true);
        QTRY_COMPARE(left->view()->itemCount(), 3);
        select->click();
        QTRY_COMPARE(left->view()->selectedItems().size(), 3);
        right->setUrl(QUrl::fromLocalFile(dir.path()));
        select->click();
        QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("changed or are hidden")));
        window.actionCollection()->action(QStringLiteral("dual_layout"))->setChecked(false);
        QVERIFY(!action->isEnabled());
    }

    void folderComparisonHandlesErrorsAndCancellation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FolderComparisonDialog failed(QUrl::fromLocalFile(dir.path()), QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing"))));
        auto *status = failed.findChild<QLabel *>(QStringLiteral("folderComparisonStatus"));
        auto *cancel = failed.findChild<QPushButton *>(QStringLiteral("folderComparisonCancel"));
        auto *select = failed.findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"));
        QVERIFY(status && cancel && select);
        QTRY_VERIFY(!cancel->isEnabled());
        QVERIFY(status->text().startsWith(QStringLiteral("Could not list")));
        QVERIFY(!select->isEnabled());
        FolderComparisonDialog canceled(QUrl::fromLocalFile(dir.path()), QUrl::fromLocalFile(dir.path()));
        canceled.findChild<QPushButton *>(QStringLiteral("folderComparisonCancel"))->click();
        QCOMPARE(canceled.findChild<QLabel *>(QStringLiteral("folderComparisonStatus"))->text(), QStringLiteral("Comparison canceled."));
        QVERIFY(!canceled.findChild<QPushButton *>(QStringLiteral("folderComparisonSelect"))->isEnabled());
    }

    void navigationUpdatesHistory()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        Pane *pane = window.activePane();
        QTRY_COMPARE(pane->view()->itemCount(), 2);

        pane->setUrl(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub"))));
        QVERIFY(pane->tabs()->canGoBack());
        pane->goUp();
        QCOMPARE(pane->url(), m_a);
        pane->goBack();
        QCOMPARE(pane->url().toLocalFile(), m_root.filePath(QStringLiteral("a/sub")));
    }

    void displayStyleIsIndependentPerPane()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        Pane *active = window.activePane();
        Pane *other = window.inactivePane();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Details);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);

        window.actionCollection()->action(QStringLiteral("toggle_view_mode"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Icons);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);
        window.actionCollection()->action(QStringLiteral("compact_view"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Compact);
        QCOMPARE(other->view()->viewMode(), FolderView::ViewMode::Details);
        window.actionCollection()->action(QStringLiteral("toggle_view_mode"))->trigger();
        QCOMPARE(active->view()->viewMode(), FolderView::ViewMode::Details);
    }

    void selectionModeActionBarCopiesFromItsPane()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        auto *action = window.actionCollection()->action(QStringLiteral("selection_mode"));
        QVERIFY(action);
        action->trigger();
        QVERIFY(view->selectionMode());
        auto *copy = view->findChild<QToolButton *>(QStringLiteral("selectionCopy"));
        QVERIFY(copy);
        QVERIFY(!copy->isEnabled());
        const QUrl file = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        view->selectUrls({file});
        QVERIFY(copy->isEnabled());
        copy->click();
        QCOMPARE(QApplication::clipboard()->mimeData()->urls(), QList<QUrl>{file});
        view->findChild<QToolButton *>(QStringLiteral("selectionDone"))->click();
        QVERIFY(!action->isChecked());
        QVERIFY(!view->selectionMode());
    }

    void tabAppearanceSettingsApplyToBothPanes()
    {
        KConfigGroup tabs = KSharedConfig::openConfig()->group(QStringLiteral("Tabs"));
        tabs.writeEntry("ShowIcons", false);
        tabs.writeEntry("Expand", true);
        tabs.sync();
        Settings::self()->load();

        MainWindow window;
        window.openUrls({m_a});
        for (Pane *pane : {window.activePane(), window.inactivePane()}) {
            QTabBar *tabBar = pane->findChild<QTabBar *>();
            QVERIFY(tabBar);
            QVERIFY(tabBar->expanding());
            QVERIFY(tabBar->tabIcon(0).isNull());
        }
    }

    void panelSettingsApplyOnStartup()
    {
        KConfigGroup panels = KSharedConfig::openConfig()->group(QStringLiteral("PanelSettings"));
        panels.writeEntry("Visible", false);
        panels.writeEntry("Position", QStringLiteral("right"));
        panels.writeEntry("TitlesVisible", false);
        panels.writeEntry("FavoritesVisible", false);
        panels.writeEntry("PlacesVisible", false);
        panels.writeEntry("FoldersVisible", false);
        panels.sync();
        Settings::self()->load();

        MainWindow window;
        auto *combined = window.findChild<QDockWidget *>(QStringLiteral("combinedDock"));
        auto *places = window.findChild<QDockWidget *>(QStringLiteral("placesDock"));
        QVERIFY(combined);
        QVERIFY(places);
        QVERIFY(combined->isHidden());
        QCOMPARE(window.dockWidgetArea(places), Qt::RightDockWidgetArea);
        QVERIFY(places->titleBarWidget());

        auto *panel = combined->findChild<CombinedPanel *>();
        QVERIFY(panel);
        auto *favorites = panel->findChild<QListView *>();
        auto *panelPlaces = panel->findChild<KFilePlacesView *>();
        auto *tree = panel->findChild<FolderTree *>();
        QVERIFY(favorites);
        QVERIFY(panelPlaces);
        QVERIFY(tree);
        QVERIFY(favorites->isHidden());
        QVERIFY(panelPlaces->isHidden());
        QVERIFY(tree->isHidden());
    }

    void jumpBackTogglesBetweenFolders()
    {
        MainWindow window;
        window.openUrls({m_a});
        Pane *pane = window.activePane();
        pane->setUrl(m_b);
        QAction *jumpBack = window.actionCollection()->action(QStringLiteral("jump_back"));
        QVERIFY(jumpBack && jumpBack->isEnabled());
        jumpBack->trigger();
        QCOMPARE(pane->url(), m_a);
        jumpBack->trigger();
        QCOMPARE(pane->url(), m_b);
    }

    void popularFoldersMenuListsVisitedLocations()
    {
        MainWindow window;
        window.openUrls({m_a});
        window.activePane()->setUrl(m_b);

        QAction *popularAction = window.actionCollection()->action(QStringLiteral("popular_folders"));
        QVERIFY(popularAction && popularAction->menu());
        QMenu *menu = popularAction->menu();
        QMetaObject::invokeMethod(menu, "aboutToShow");
        QStringList names;
        for (QAction *action : menu->actions()) {
            names.append(action->text());
        }
        QVERIFY(names.contains(QStringLiteral("a")));
        QVERIFY(names.contains(QStringLiteral("b")));
    }

    void savedSelectionFilterSelectsMatchingFiles()
    {
        KSharedConfig::openConfig()->group(QStringLiteral("SelectionFilters")).writeEntry("Patterns", QStringList{QStringLiteral("*.txt")});
        MainWindow window;
        window.openUrls({m_a});
        QTRY_COMPARE(window.activePane()->view()->itemCount(), 2);

        QAction *selectAction = window.actionCollection()->action(QStringLiteral("select_menu"));
        QVERIFY(selectAction && selectAction->menu());
        QMenu *saved = nullptr;
        for (QAction *action : selectAction->menu()->actions()) {
            if (action->menu() && action->text() == QLatin1String("Saved Selection Filters")) {
                saved = action->menu();
                break;
            }
        }
        QVERIFY(saved);
        QMetaObject::invokeMethod(saved, "aboutToShow");
        QCOMPARE(saved->actions().size(), 1);
        saved->actions().first()->trigger();
        QTRY_COMPARE(window.activePane()->view()->selectedItems().size(), 1);
        QCOMPARE(window.activePane()->view()->selectedItems().first().name(), QStringLiteral("file.txt"));
    }

    void occupiedSpaceActionUsesKioDirectorySize()
    {
        QFile nested(m_root.filePath(QStringLiteral("a/sub/nested.txt")));
        QVERIFY(nested.open(QIODevice::WriteOnly));
        QCOMPARE(nested.write("hi"), 2);
        nested.close();

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);
        view->selectUrls({QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")))});

        window.actionCollection()->action(QStringLiteral("calculate_occupied_space"))->trigger();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("occupied in")));
        QVERIFY2(window.statusBar()->currentMessage().contains(QStringLiteral("5 B")), qPrintable(window.statusBar()->currentMessage()));

        view->clearSelection();
        window.actionCollection()->action(QStringLiteral("calculate_occupied_space"))->trigger();
        QTRY_VERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("folder")));
    }

    void filterPersistsWithThePaneSession()
    {
        {
            MainWindow window;
            window.openUrls({m_a});
            auto *filter = window.activePane()->findChild<FilterBar *>();
            QVERIFY(filter);
            filter->setFilter(NameFilter(QStringLiteral("*.txt"), NameFilter::Scope::Files));
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        QCOMPARE(restored.activePane()->view()->nameFilter(), NameFilter(QStringLiteral("*.txt"), NameFilter::Scope::Files));
        QTRY_COMPARE(restored.activePane()->view()->itemCount(), 2);
    }

    void viewSettingsApplyToCurrentListing()
    {
        ViewSettings settings;
        settings.visibleColumns = {QStringLiteral("size")};
        settings.groupBy = QStringLiteral("ext");
        settings.sizeFormat = QStringLiteral("bytes");
        settings.dateFormat = QStringLiteral("iso");
        settings.iconNameLines = 4;
        settings.iconInformation = {QStringLiteral("size")};
        KConfigGroup viewConfig = KSharedConfig::openConfig()->group(QStringLiteral("View"));
        settings.writeConfig(viewConfig);
        KConfigGroup files = KSharedConfig::openConfig()->group(QStringLiteral("Files"));
        files.writeEntry("IgnoreList", QStringList{QStringLiteral("sub")});
        files.writeEntry("ExtensionColors", QStringList{QStringLiteral("txt=#123456")});
        KSharedConfig::openConfig()->sync();

        MainWindow window;
        window.openUrls({m_a});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        QCOMPARE(view->groupBy(), QStringLiteral("ext"));

        auto *details = view->findChild<QTreeView *>();
        QVERIFY(details);
        QVERIFY(details->isColumnHidden(KDirModel::Type));
        QVERIFY(!details->isColumnHidden(KDirModel::Size));
        QVERIFY(details->isColumnHidden(KDirModel::ModifiedTime));
        const QModelIndex nameIndex = details->model()->index(0, KDirModel::Name);
        QCOMPARE(details->model()->data(nameIndex, Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#123456")));
        QCOMPARE(details->model()->data(details->model()->index(0, KDirModel::Size)).toString(), QStringLiteral("5 B"));
        QVERIFY(QRegularExpression(QStringLiteral(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}$)"))
                    .match(details->model()->data(details->model()->index(0, KDirModel::ModifiedTime)).toString())
                    .hasMatch());

        view->setViewMode(FolderView::ViewMode::Icons);
        auto *delegate = qobject_cast<KFileItemDelegate *>(view->currentView()->itemDelegate());
        QVERIFY(delegate);
        QCOMPARE(delegate->showInformation(), KFileItemDelegate::InformationList{KFileItemDelegate::Size});
        auto *icons = qobject_cast<QListView *>(view->currentView());
        QVERIFY(icons);
        QVERIFY(icons->gridSize().height() > 112);
    }

    void preferencesActionAppliesSettingsToOpenPanes()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);

        QAction *preferences = window.actionCollection()->action(QStringLiteral("options_configure"));
        QVERIFY(preferences);
        preferences->trigger();
        auto *dialog = window.findChild<SettingsDialog *>();
        QVERIFY(dialog);
        auto *groupBy = dialog->findChild<QComboBox *>(QStringLiteral("kcfg_GroupBy"));
        QVERIFY(groupBy);
        groupBy->setCurrentIndex(Settings::EnumGroupBy::ext);
        auto *typeColumn = dialog->findChild<QCheckBox *>(QStringLiteral("kcfg_ShowTypeColumn"));
        QVERIFY(typeColumn);
        typeColumn->setChecked(false);
        QPushButton *ok = dialog->button(QDialogButtonBox::Ok);
        QVERIFY(ok);
        ok->click();

        QCOMPARE(view->groupBy(), QStringLiteral("ext"));
        auto *details = view->findChild<QTreeView *>();
        QVERIFY(details);
        QVERIFY(details->isColumnHidden(KDirModel::Type));
        QCOMPARE(window.inactivePane()->view()->groupBy(), QStringLiteral("ext"));
    }

    void copyToOtherPaneUsesKio()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 2);

        const QUrl source = QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/file.txt")));
        FileOperations::copy({source}, window.inactivePane()->url(), &window);
        QTRY_VERIFY(QFile::exists(m_root.filePath(QStringLiteral("b/file.txt"))));
        QTRY_COMPARE(window.inactivePane()->view()->itemCount(), 1);
    }

    void stackedLayoutMeetsTabBarsInTheMiddle()
    {
        MainWindow window;
        window.openUrls({m_a, m_b});
        window.inactivePane()->openInNewTab(m_a);
        window.resize(800, 600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        window.setStackedLayout(true);
        QVERIFY(window.isStackedLayout());
        Pane *top = window.activePane();
        Pane *bottom = window.inactivePane();
        auto *splitter = window.findChild<QSplitter *>(QStringLiteral("paneSplitter"));
        QVERIFY(splitter);
        QCOMPARE(splitter->orientation(), Qt::Vertical);
        QCOMPARE(splitter->widget(0), top);
        QCOMPARE(splitter->widget(1), bottom);

        QTRY_VERIFY(tabBarOf(top)->isVisible() && tabBarOf(bottom)->isVisible());
        QCOMPARE(tabBarOf(top)->shape(), QTabBar::RoundedSouth);
        QCOMPARE(tabBarOf(bottom)->shape(), QTabBar::RoundedNorth);
        // Top pane: view above tabs. Bottom pane: tabs above view.
        QTRY_VERIFY(globalTop(tabBarOf(top)) > globalTop(top->view()));
        QTRY_VERIFY(globalTop(tabBarOf(bottom)) < globalTop(bottom->view()));
        // Nothing but the splitter handle separates the two tab bars.
        const int gap = globalTop(tabBarOf(bottom)) - (globalTop(tabBarOf(top)) + tabBarOf(top)->height());
        QVERIFY2(gap >= 0 && gap <= splitter->handleWidth(), qPrintable(QString::number(gap)));

        window.setStackedLayout(false);
        QCOMPARE(splitter->orientation(), Qt::Horizontal);
        QCOMPARE(tabBarOf(top)->shape(), QTabBar::RoundedNorth);
        QTRY_VERIFY(globalTop(tabBarOf(top)) < globalTop(top->view()));
    }

    void stackedLayoutSurvivesRestart()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            window.setStackedLayout(true);
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        QVERIFY(restored.isStackedLayout());
    }

    void sessionSurvivesRestart()
    {
        {
            MainWindow window;
            window.openUrls({m_a, m_b});
            window.activePane()->openInNewTab(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub"))), true);
            QCOMPARE(window.activePane()->tabs()->count(), 3);
            window.close();
        }

        MainWindow restored;
        restored.openUrls({});
        // The first window restored an empty session (home), then added a and a/sub.
        const QList<QUrl> left = restored.activePane()->tabs()->urls();
        QVERIFY(left.contains(m_a));
        QVERIFY(left.contains(QUrl::fromLocalFile(m_root.filePath(QStringLiteral("a/sub")))));
        QCOMPARE(restored.activePane()->url().toLocalFile(), m_root.filePath(QStringLiteral("a/sub")));
        QVERIFY(restored.inactivePane()->tabs()->urls().contains(m_b));
    }

private:
    static QTabBar *tabBarOf(Pane *pane)
    {
        return pane->findChild<QTabBar *>();
    }

    static int globalTop(QWidget *widget)
    {
        return widget->mapToGlobal(QPoint(0, 0)).y();
    }

    QTemporaryDir m_root;
    QUrl m_a;
    QUrl m_b;
    QUrl m_c;
};

QTEST_MAIN(MainWindowTest)

#include "mainwindowtest.moc"
