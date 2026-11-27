// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorpanel.h"
#include "settings.h"

#include <KTextEditor/Document>
#include <KTextEditor/View>

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontInfo>
#include <QImage>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>

class EditorPanelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        QStandardPaths::setTestModeEnabled(true);
        Settings::self()->setDefaults();
    }

    void textAndDraftSettingsApply()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Settings::setEditorFontSize(18);
        Settings::setEditorWordWrap(true);
        Settings::setEditorDraftsEnabled(false);
        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        auto *view = editor.findChild<KTextEditor::View *>();
        QVERIFY(view);
        QCOMPARE(view->configValue(QStringLiteral("font")).value<QFont>().pointSize(), 18);
        auto *visual = editor.findChild<QTextEdit *>(QStringLiteral("editorVisualText"));
        QVERIFY(visual);
        QCOMPARE(visual->document()->defaultFont().pointSize(), 18);
        QVERIFY(!QFontInfo(visual->document()->defaultFont()).fixedPitch());
        QVERIFY(view->configValue(QStringLiteral("dynamic-word-wrap")).toBool());
        QVERIFY(editor.document()->setText(QStringLiteral("unsaved")));
        QTest::qWait(900);
        QString error;
        QVERIFY(DraftStore(directory.filePath(QStringLiteral("drafts"))).drafts(QUrl(), &error).isEmpty());
        QVERIFY(error.isEmpty());
        Settings::setEditorDraftsEnabled(true);
        Settings::setEditorWordWrap(false);
        Settings::setEditorDraftIntervalMs(100);
        editor.applySettings();
        QVERIFY(!view->configValue(QStringLiteral("dynamic-word-wrap")).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(DraftStore(directory.filePath(QStringLiteral("drafts"))).drafts(QUrl(), &error).size(), 1, 5000);
    }

    void markdownPreviewSupportsCommonMarkWithoutChangingSource()
    {
        QTemporaryDir directory;
        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        const QString source = QStringLiteral("A ~~strike~~ and **bold**\n");
        QVERIFY(editor.document()->setText(source));
        auto *previewToggle = editor.findChild<QPushButton *>(QStringLiteral("editorMarkdownPreview"));
        auto *preview = editor.findChild<QTextBrowser *>(QStringLiteral("editorRenderedMarkdown"));
        auto *dialect = editor.findChild<QComboBox *>(QStringLiteral("editorMarkdownDialect"));
        QVERIFY(previewToggle && preview && dialect);
        previewToggle->setChecked(true);
        QTextCursor strike = preview->document()->find(QStringLiteral("strike"));
        QVERIFY(!strike.isNull());
        QVERIFY(strike.charFormat().fontStrikeOut());
        dialect->setCurrentIndex(1);
        QCOMPARE(preview->toPlainText().trimmed(), QStringLiteral("A ~~strike~~ and bold"));
        const QTextCursor bold = preview->document()->find(QStringLiteral("bold"));
        QVERIFY(!bold.isNull());
        QVERIFY(bold.charFormat().fontWeight() >= QFont::Bold);
        QCOMPARE(editor.document()->text(), source);
        QVERIFY(editor.document()->setText(QStringLiteral("# Updated\n")));
        QCOMPARE(preview->toPlainText().trimmed(), QStringLiteral("Updated"));
        QVERIFY(editor.document()->setText(source));
        dialect->setCurrentIndex(0);
        strike = preview->document()->find(QStringLiteral("strike"));
        QVERIFY(strike.charFormat().fontStrikeOut());
    }

    void visualMarkdownRequiresConsentAndPreservesSource()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile source(directory.filePath(QStringLiteral("source.md")));
        const QByteArray original("# Heading\n\nA [link](https://example.invalid) and <!-- custom --> text.\n");
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(original), original.size());
        source.close();
        const QUrl url = QUrl::fromLocalFile(source.fileName());
        const QString path = directory.filePath(QStringLiteral("drafts"));
        Settings::setEditorMarkdownMode(Settings::EnumEditorMarkdownMode::Visual);
        EditorPanel editor(nullptr, path);
        editor.show();
        QVERIFY(editor.openUrl(url));
        auto *visual = editor.findChild<QTextEdit *>(QStringLiteral("editorVisualText"));
        auto *enable = editor.findChild<QPushButton *>(QStringLiteral("editorEnableVisualEditing"));
        auto *sourceMode = editor.findChild<QPushButton *>(QStringLiteral("editorSourceMode"));
        auto *visualMode = editor.findChild<QPushButton *>(QStringLiteral("editorVisualMode"));
        QVERIFY(visual && enable && sourceMode && visualMode);
        QTRY_VERIFY(visual->isVisible());
        QVERIFY(visual->isReadOnly());
        const QString imagePath = directory.filePath(QStringLiteral("private.png"));
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(imagePath));
        QVERIFY(visual->document()->resource(QTextDocument::ImageResource, QUrl::fromLocalFile(imagePath)).value<QImage>().isNull());
        QCOMPARE(editor.document()->text(), QString::fromUtf8(original));
        const QString before = editor.document()->text();
        sourceMode->click();
        visualMode->click();
        QCOMPARE(editor.document()->text(), before);
        QVERIFY(visual->isReadOnly());
        QTimer answer;
        answer.setInterval(10);
        connect(&answer, &QTimer::timeout, &editor, [] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                dialog->reject();
            }
        });
        answer.start();
        enable->click();
        answer.stop();
        QVERIFY(visual->isReadOnly());
        QCOMPARE(editor.document()->text(), before);
        answer.disconnect();
        connect(&answer, &QTimer::timeout, &editor, [] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                if (auto *box = dialog->findChild<QDialogButtonBox *>()) {
                    for (auto *button : box->buttons()) {
                        if (box->buttonRole(button) == QDialogButtonBox::AcceptRole || box->buttonRole(button) == QDialogButtonBox::YesRole) {
                            button->click();
                            return;
                        }
                    }
                }
            }
        });
        answer.start();
        enable->click();
        answer.stop();
        QVERIFY(!visual->isReadOnly());
        QString error;
        const auto snapshots = DraftStore(path).drafts(url, &error);
        QCOMPARE(snapshots.size(), 1);
        QCOMPARE(snapshots.first().text, before);
        visual->moveCursor(QTextCursor::End);
        visual->insertPlainText(QStringLiteral(" added"));
        QVERIFY(editor.document()->text().contains(QStringLiteral("added")));
        QVERIFY(editor.document()->isModified());
        QTextCursor selection = visual->textCursor();
        selection.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 5);
        visual->setTextCursor(selection);
        auto *bold = editor.findChild<QPushButton *>(QStringLiteral("editorVisualBold"));
        QVERIFY(bold);
        bold->click();
        QTextDocument rendered;
        rendered.setMarkdown(editor.document()->text());
        const QTextCursor formatted = rendered.find(QStringLiteral("added"));
        QVERIFY(!formatted.isNull());
        QVERIFY(formatted.charFormat().fontWeight() >= QFont::Bold);
        QVERIFY(source.open(QIODevice::ReadOnly));
        QCOMPARE(source.readAll(), original);
        sourceMode->click();
        QVERIFY(!visual->isVisible());
        visualMode->click();
        QVERIFY(visual->isReadOnly());
        QVERIFY(visual->toPlainText().contains(QStringLiteral("added")));
        QVERIFY(editor.document()->documentSave());
        source.close();
        QVERIFY(source.open(QIODevice::ReadOnly));
        QVERIFY(source.readAll().contains("added"));
        QTRY_COMPARE(DraftStore(path).drafts(url, &error).size(), 1);
        QCOMPARE(DraftStore(path).drafts(url, &error).first().text, before);
    }

    void visualModeIsNotAppliedToPlainText()
    {
        QTemporaryDir directory;
        QFile source(directory.filePath(QStringLiteral("plain.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("plain");
        source.close();
        Settings::setEditorMarkdownMode(Settings::EnumEditorMarkdownMode::Visual);
        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        QVERIFY(editor.openUrl(QUrl::fromLocalFile(source.fileName())));
        auto *sourceMode = editor.findChild<QPushButton *>(QStringLiteral("editorSourceMode"));
        QTRY_VERIFY(sourceMode && sourceMode->isChecked());
    }

    void sourceIsTheConfiguredMarkdownDefault()
    {
        QTemporaryDir directory;
        QFile source(directory.filePath(QStringLiteral("source.md")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("# source");
        source.close();
        Settings::setEditorMarkdownMode(Settings::EnumEditorMarkdownMode::Source);
        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        QVERIFY(editor.openUrl(QUrl::fromLocalFile(source.fileName())));
        auto *sourceMode = editor.findChild<QPushButton *>(QStringLiteral("editorSourceMode"));
        auto *visual = editor.findChild<QTextEdit *>(QStringLiteral("editorVisualText"));
        QTest::qWait(50);
        QVERIFY(sourceMode && sourceMode->isChecked());
        QVERIFY(visual && !visual->isVisible());
        QCOMPARE(editor.document()->text(), QStringLiteral("# source"));
    }

    void unsynchronizedVisualEditsBlockNavigation()
    {
        QTemporaryDir directory;
        QFile source(directory.filePath(QStringLiteral("source.md")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("original");
        source.close();
        Settings::setEditorMarkdownMode(Settings::EnumEditorMarkdownMode::Visual);
        Settings::setEditorDraftsEnabled(false);
        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        const QUrl url = QUrl::fromLocalFile(source.fileName());
        QVERIFY(editor.openUrl(url));
        auto *visual = editor.findChild<QTextEdit *>(QStringLiteral("editorVisualText"));
        auto *enable = editor.findChild<QPushButton *>(QStringLiteral("editorEnableVisualEditing"));
        QTRY_VERIFY(visual->isVisible());
        QTimer answer;
        answer.setInterval(10);
        connect(&answer, &QTimer::timeout, &editor, [] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                if (auto *box = dialog->findChild<QDialogButtonBox *>()) {
                    for (auto *button : box->buttons()) {
                        if (box->buttonRole(button) == QDialogButtonBox::YesRole || box->buttonRole(button) == QDialogButtonBox::AcceptRole) {
                            button->click();
                            return;
                        }
                    }
                }
            }
        });
        answer.start();
        enable->click();
        answer.stop();
        QVERIFY(!visual->isReadOnly());
        editor.document()->setReadWrite(false);
        visual->moveCursor(QTextCursor::End);
        visual->insertPlainText(QStringLiteral(" pending"));
        QVERIFY(visual->isReadOnly());
        QVERIFY(visual->toPlainText().contains(QStringLiteral("pending")));
        QCOMPARE(editor.document()->text(), QStringLiteral("original"));
        QVERIFY(!editor.openUrl(QUrl::fromLocalFile(directory.filePath(QStringLiteral("another.md")))));
        QVERIFY(!editor.queryClose());
        editor.document()->setReadWrite(true);
        auto *retry = editor.findChild<QPushButton *>(QStringLiteral("editorRetryVisualSync"));
        QVERIFY(retry && retry->isVisible());
        retry->click();
        QVERIFY(!visual->isReadOnly());
        QVERIFY(editor.document()->text().contains(QStringLiteral("pending")));
    }

    void recoveryDoesNotOverwriteSource()
    {
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile source(directory.filePath(QStringLiteral("document.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write("original"), 8);
        source.close();
        const QUrl url = QUrl::fromLocalFile(source.fileName());
        const QString drafts = directory.filePath(QStringLiteral("drafts"));
        {
            EditorPanel editor(nullptr, drafts);
            editor.show();
            QVERIFY(editor.openUrl(url));
            QTRY_COMPARE(editor.document()->text(), QStringLiteral("original"));
            QVERIFY(editor.document()->setText(QStringLiteral("unsaved edits")));
            QString error;
            DraftStore store(drafts);
            QTRY_COMPARE_WITH_TIMEOUT(store.drafts(url, &error).size(), 1, 5000);
            QVERIFY(error.isEmpty());
            QCOMPARE(store.drafts(url, &error).first().text, QStringLiteral("unsaved edits"));
        }

        {
            EditorPanel editor(nullptr, drafts);
            editor.show();
            QVERIFY(editor.openUrl(url));
            auto *banner = editor.findChild<QWidget *>(QStringLiteral("editorDraftRecovery"));
            QVERIFY(banner);
            QTRY_VERIFY(!banner->isHidden());
            auto *restore = editor.findChild<QPushButton *>(QStringLiteral("editorRestoreDraft"));
            QVERIFY(restore);
            restore->click();
            QCOMPARE(editor.document()->text(), QStringLiteral("unsaved edits"));
            QVERIFY(editor.document()->isModified());
            QVERIFY(source.open(QIODevice::ReadOnly));
            QCOMPARE(source.readAll(), QByteArray("original"));
        }
        {
            EditorPanel editor(nullptr, drafts);
            editor.show();
            QVERIFY(editor.openUrl(url));
            auto *banner = editor.findChild<QWidget *>(QStringLiteral("editorDraftRecovery"));
            QVERIFY(banner);
            QTRY_VERIFY(!banner->isHidden());
            auto *discard = editor.findChild<QPushButton *>(QStringLiteral("editorDiscardDraft"));
            QVERIFY(discard);
            discard->click();
            QVERIFY(banner->isHidden());
            QString error;
            DraftStore store(drafts);
            QVERIFY(store.drafts(url, &error).isEmpty());
            QVERIFY(error.isEmpty());
        }
    }

    void cancelSwitchAndCloseKeepEdits()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile first(directory.filePath(QStringLiteral("first.txt")));
        QFile second(directory.filePath(QStringLiteral("second.txt")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        first.write("first");
        first.close();
        QVERIFY(second.open(QIODevice::WriteOnly));
        second.write("second");
        second.close();
        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        QVERIFY(editor.openUrl(QUrl::fromLocalFile(first.fileName())));
        QVERIFY(editor.document()->setText(QStringLiteral("modified")));
        QTimer cancelPrompt;
        cancelPrompt.setInterval(10);
        connect(&cancelPrompt, &QTimer::timeout, [] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                dialog->reject();
            }
        });
        cancelPrompt.start();
        QVERIFY(!editor.openUrl(QUrl::fromLocalFile(second.fileName())));
        QCOMPARE(editor.document()->text(), QStringLiteral("modified"));
        QCOMPARE(editor.document()->url(), QUrl::fromLocalFile(first.fileName()));
        QVERIFY(!editor.queryClose());
        cancelPrompt.stop();
        QCOMPARE(editor.document()->text(), QStringLiteral("modified"));
    }

    void closingUnsavedOrDiscardedDocumentsSucceeds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile source(directory.filePath(QStringLiteral("source.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("original");
        source.close();
        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        QVERIFY(editor.queryClose());
        QVERIFY(editor.openUrl(QUrl::fromLocalFile(source.fileName())));
        QVERIFY(editor.queryClose());
        QVERIFY(editor.document()->setText(QStringLiteral("discarded")));
        QTimer discardPrompt;
        discardPrompt.setInterval(10);
        connect(&discardPrompt, &QTimer::timeout, [] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                if (auto *box = dialog->findChild<QDialogButtonBox *>()) {
                    box->button(QDialogButtonBox::No)->click();
                }
            }
        });
        discardPrompt.start();
        QVERIFY(editor.closeDocument());
        discardPrompt.stop();
        QVERIFY(editor.document()->url().isEmpty());
        QVERIFY(!editor.document()->isModified());
        QVERIFY(source.open(QIODevice::ReadOnly));
        QCOMPARE(source.readAll(), QByteArray("original"));
        QString error;
        QVERIFY(DraftStore(directory.filePath(QStringLiteral("drafts"))).drafts(QUrl::fromLocalFile(source.fileName()), &error).isEmpty());
    }

    void savedDraftIsRemoved()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile source(directory.filePath(QStringLiteral("source.txt")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("original");
        source.close();
        const QUrl url = QUrl::fromLocalFile(source.fileName());
        const QString path = directory.filePath(QStringLiteral("drafts"));
        EditorPanel editor(nullptr, path);
        editor.show();
        QVERIFY(editor.openUrl(url));
        QVERIFY(editor.document()->setText(QStringLiteral("saved")));
        DraftStore store(path);
        QString error;
        QTRY_COMPARE_WITH_TIMEOUT(store.drafts(url, &error).size(), 1, 5000);
        QVERIFY(editor.document()->documentSave());
        QTRY_VERIFY(store.drafts(url, &error).isEmpty());
        QVERIFY(error.isEmpty());
    }
};

QTEST_MAIN(EditorPanelTest)
#include "editorpaneltest.moc"
