// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorpanel.h"
#include "folderview.h"
#include "mainwindow.h"
#include "pane.h"
#include "previewpanel.h"
#include "settings.h"

#include <KActionCollection>
#include <KFileItem>
#include <KTextEditor/Document>

#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

namespace
{
// Answers every modal dialog that appears while it runs with @p button.
class DialogAnswer
{
public:
    explicit DialogAnswer(QDialogButtonBox::StandardButton button)
    {
        m_timer.setInterval(10);
        QObject::connect(&m_timer, &QTimer::timeout, [button] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }
            const auto boxes = dialog->findChildren<QDialogButtonBox *>();
            for (QDialogButtonBox *box : boxes) {
                if (QAbstractButton *answer = box->button(button)) {
                    answer->click();
                    return;
                }
            }
            dialog->reject();
        });
        m_timer.start();
    }

private:
    QTimer m_timer;
};
}

class PreviewEditingTest : public QObject
{
    Q_OBJECT

private:
    QString writeFile(const QString &name, const QByteArray &contents)
    {
        QFile file(m_root.filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()) {
            return {};
        }
        return file.fileName();
    }

    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    static KFileItem item(const QString &path)
    {
        return KFileItem(QUrl::fromLocalFile(path));
    }

    static QToolButton *button(QWidget *panel, const QString &name)
    {
        return panel->findChild<QToolButton *>(name);
    }

    // Shows @p path in @p panel and switches to the editor.
    static bool startEditing(PreviewPanel &panel, const QString &path)
    {
        panel.setItems({item(path)});
        QToolButton *edit = button(&panel, QStringLiteral("previewEdit"));
        if (!edit || !QTest::qWaitFor([edit] {
                return edit->isVisible();
            })) {
            return false;
        }
        edit->click();
        return panel.isEditing();
    }

    static KTextEditor::Document *document(PreviewPanel &panel)
    {
        auto *editor = panel.findChild<EditorPanel *>(QStringLiteral("previewEditor"));
        return editor ? editor->document() : nullptr;
    }

    QTemporaryDir m_root;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
    }

    void init()
    {
        Settings::self()->setDefaults();
        Settings::setEditorDraftsEnabled(false);
    }

    void editsTheWholeFileAndReturnsAfterSaving()
    {
        Settings::setTextPreviewLimitKiB(1);
        const QByteArray contents = QByteArray(2500, 'x') + "\nend\n";
        const QString path = writeFile(QStringLiteral("long.txt"), contents);
        QVERIFY(!path.isEmpty());
        PreviewPanel panel;
        panel.resize(500, 400);
        panel.show();
        auto *content = panel.findChild<QStackedWidget *>();
        QVERIFY(content);
        QVERIFY(startEditing(panel, path));
        QCOMPARE(content->currentWidget()->objectName(), QStringLiteral("previewEditor"));
        QVERIFY(!button(&panel, QStringLiteral("previewEdit"))->isVisible());
        QVERIFY(button(&panel, QStringLiteral("previewEditMove"))->isVisible());
        QVERIFY(button(&panel, QStringLiteral("previewEditClose"))->isVisible());
        auto *editor = panel.findChild<EditorPanel *>(QStringLiteral("previewEditor"));
        QVERIFY(editor);
        QVERIFY(!editor->findChild<QPushButton *>(QStringLiteral("editorOpen"))->isVisible());
        QVERIFY(!editor->findChild<QPushButton *>(QStringLiteral("editorHide"))->isVisible());
        QVERIFY(!editor->findChild<QWidget *>(QStringLiteral("editorPlacement"))->isVisible());
        KTextEditor::Document *doc = document(panel);
        QCOMPARE(doc->text().toUtf8(), contents);

        QVERIFY(doc->setText(QStringLiteral("saved\n")));
        QVERIFY(doc->documentSave());
        QTRY_VERIFY(!panel.isEditing());
        QCOMPARE(readFile(path), QByteArray("saved\n"));
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QVERIFY(text);
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("saved\n"));
        QCOMPARE(content->currentWidget(), text);
        QVERIFY(button(&panel, QStringLiteral("previewEdit"))->isVisible());
        QVERIFY(!button(&panel, QStringLiteral("previewEditClose"))->isVisible());
    }

    void unsavedChangesKeepTheFileOpen()
    {
        const QString first = writeFile(QStringLiteral("first.txt"), "first\n");
        const QString second = writeFile(QStringLiteral("second.txt"), "second\n");
        PreviewPanel panel;
        panel.resize(500, 400);
        panel.show();
        QVERIFY(startEditing(panel, first));
        KTextEditor::Document *doc = document(panel);
        QVERIFY(doc->setText(QStringLiteral("changed\n")));

        panel.setItems({item(second)});
        QVERIFY(panel.isEditing());
        QCOMPARE(doc->url(), QUrl::fromLocalFile(first));
        auto *notice = panel.findChild<QWidget *>(QStringLiteral("previewEditNotice"));
        auto *showSelection = button(&panel, QStringLiteral("previewEditShowSelection"));
        QVERIFY(notice && showSelection);
        QVERIFY(notice->isVisible());
        QVERIFY(showSelection->isVisible());
        panel.clear();
        QVERIFY(panel.isEditing());
        QVERIFY(notice->isVisible());

        // Selecting the edited file again drops the pending selection.
        panel.setItems({item(first)});
        QVERIFY(panel.isEditing());
        QVERIFY(!notice->isVisible());

        panel.setItems({item(second)});
        {
            DialogAnswer cancel(QDialogButtonBox::Cancel);
            showSelection->click();
        }
        QVERIFY(panel.isEditing());
        QCOMPARE(doc->text(), QStringLiteral("changed\n"));
        {
            DialogAnswer cancel(QDialogButtonBox::Cancel);
            QVERIFY(!panel.queryClose());
        }
        QVERIFY(panel.isEditing());

        {
            DialogAnswer discard(QDialogButtonBox::No);
            showSelection->click();
        }
        QVERIFY(!panel.isEditing());
        QCOMPARE(readFile(first), QByteArray("first\n"));
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("second\n"));
        QVERIFY(!notice->isVisible());
    }

    void savingFromThePromptShowsThePendingSelection()
    {
        const QString first = writeFile(QStringLiteral("prompt-first.txt"), "first\n");
        const QString second = writeFile(QStringLiteral("prompt-second.txt"), "second\n");
        PreviewPanel panel;
        panel.show();
        QVERIFY(startEditing(panel, first));
        QVERIFY(document(panel)->setText(QStringLiteral("kept\n")));
        panel.setItems({item(second)});
        {
            DialogAnswer save(QDialogButtonBox::Yes);
            QVERIFY(panel.queryClose());
        }
        QVERIFY(!panel.isEditing());
        QCOMPARE(readFile(first), QByteArray("kept\n"));
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("second\n"));
    }

    void unmodifiedEditorFollowsTheSelection()
    {
        const QString first = writeFile(QStringLiteral("plain-first.txt"), "first\n");
        const QString second = writeFile(QStringLiteral("plain-second.txt"), "second\n");
        PreviewPanel panel;
        panel.show();
        QVERIFY(startEditing(panel, first));
        panel.setItems({item(second)});
        QVERIFY(!panel.isEditing());
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("second\n"));
        QVERIFY(document(panel)->url().isEmpty());
        QVERIFY(startEditing(panel, second));
        QVERIFY(panel.queryClose());
        QVERIFY(!panel.isEditing());
    }

    void reportsFilesItCannotEdit()
    {
        PreviewPanel panel;
        panel.show();
        auto *notice = panel.findChild<QWidget *>(QStringLiteral("previewEditNotice"));
        auto *message = panel.findChild<QLabel *>(QStringLiteral("previewEditNoticeText"));
        QVERIFY(notice && message);

        const QString large = writeFile(QStringLiteral("large.txt"), QByteArray(10 * 1024 * 1024 + 1, 'a'));
        QVERIFY(!large.isEmpty());
        QVERIFY(!startEditing(panel, large));
        QVERIFY(notice->isVisible());
        QVERIFY(message->text().contains(QStringLiteral("larger")));

        const QString readOnly = writeFile(QStringLiteral("read-only.txt"), "locked\n");
        QVERIFY(QFile::setPermissions(readOnly, QFileDevice::ReadOwner | QFileDevice::ReadUser));
        if (QFileInfo(readOnly).isWritable()) {
            QSKIP("The test user can write read-only files");
        }
        QVERIFY(!startEditing(panel, readOnly));
        QVERIFY(notice->isVisible());
        QVERIFY(message->text().contains(QStringLiteral("read-only")));

        // A different selection clears the report.
        const QString other = writeFile(QStringLiteral("other.txt"), "other\n");
        panel.setItems({item(other)});
        QVERIFY(!notice->isVisible());
    }

    void editingIsOnlyOfferedForLocalTextPreviews()
    {
        const QString path = writeFile(QStringLiteral("compact.txt"), "compact\n");
        PreviewPanel panel;
        panel.show();
        panel.setItems({item(path)});
        auto *text = panel.findChild<QPlainTextEdit *>(QStringLiteral("previewText"));
        QTRY_COMPARE(text->toPlainText(), QStringLiteral("compact\n"));
        auto *edit = button(&panel, QStringLiteral("previewEdit"));
        QVERIFY(edit->isVisible());
        panel.setCompact(true);
        QVERIFY(!edit->isVisible());
        panel.setCompact(false);
        QVERIFY(edit->isVisible());
        panel.clear();
        QVERIFY(!edit->isVisible());
    }

    void moveToEditorHandsOverTheChanges()
    {
        const QString path = writeFile(QStringLiteral("move.txt"), "move\n");
        PreviewPanel panel;
        panel.show();
        QVERIFY(startEditing(panel, path));
        QVERIFY(document(panel)->setText(QStringLiteral("moved\n")));
        QSignalSpy requested(&panel, &PreviewPanel::editorRequested);
        button(&panel, QStringLiteral("previewEditMove"))->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().at(0).toUrl(), QUrl::fromLocalFile(path));
        QCOMPARE(requested.first().at(1).toString(), QStringLiteral("moved\n"));
        QVERIFY(requested.first().at(2).toBool());
        // The panel keeps editing until the host has taken over.
        QVERIFY(panel.isEditing());
        panel.endEditing();
        QVERIFY(!panel.isEditing());
        QCOMPARE(readFile(path), QByteArray("move\n"));
    }

    void mainWindowMovesSessionsAndGuardsHiding()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QFile file(folder.filePath(QStringLiteral("notes.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("notes\n");
        file.close();
        const QUrl url = QUrl::fromLocalFile(file.fileName());

        MainWindow window;
        window.resize(1200, 800);
        window.openUrls({QUrl::fromLocalFile(folder.path())});
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        FolderView *view = window.activePane()->view();
        QTRY_COMPARE(view->itemCount(), 1);
        auto *toggle = window.actionCollection()->action(QStringLiteral("show_preview_panel"));
        auto *panel = window.findChild<PreviewPanel *>(QStringLiteral("previewPanel"));
        QVERIFY(toggle && panel);
        toggle->trigger();
        view->selectUrls({url});
        auto *edit = button(panel, QStringLiteral("previewEdit"));
        QTRY_VERIFY(edit->isVisible());
        edit->click();
        QVERIFY(panel->isEditing());
        QVERIFY(document(*panel)->setText(QStringLiteral("unsaved\n")));

        {
            DialogAnswer cancel(QDialogButtonBox::Cancel);
            toggle->trigger();
        }
        QVERIFY(toggle->isChecked());
        QVERIFY(panel->isVisible());
        QVERIFY(panel->isEditing());

        button(panel, QStringLiteral("previewEditMove"))->click();
        QVERIFY(!panel->isEditing());
        auto *editor = window.findChild<EditorPanel *>(QStringLiteral("editorPanel"));
        QVERIFY(editor);
        QCOMPARE(editor->document()->url(), url);
        QCOMPARE(editor->document()->text(), QStringLiteral("unsaved\n"));
        QVERIFY(editor->document()->isModified());
        editor->document()->setModified(false);
    }
};

QTEST_MAIN(PreviewEditingTest)
#include "previeweditingtest.moc"
