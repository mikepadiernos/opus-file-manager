// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorpanel.h"
#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KTextEditor/Document>
#include <KTextEditor/View>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontInfo>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>

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

    void vaultLinksOpenNotesAndListBacklinks()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString root = QFileInfo(directory.path()).canonicalFilePath() + QStringLiteral("/Vault");
        const auto write = [](const QString &path, const QByteArray &contents) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(contents);
        };
        const QString home = root + QStringLiteral("/Home.md");
        const QString other = root + QStringLiteral("/Other.md");
        const QString homeSource = QStringLiteral("# Home\nSee [[Other|the other note]] and [[Missing]].\n");
        write(home, homeSource.toUtf8());
        write(other, "Other. Back to [[Home]].\n");
        write(root + QStringLiteral("/Sub/Third.md"), "[[Home#Top]]\n");
        const QString outside = directory.filePath(QStringLiteral("Outside.md"));
        write(outside, "[[Home]]\n");

        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        auto *panel = editor.findChild<QWidget *>(QStringLiteral("editorVaultPanel"));
        auto *backlinks = editor.findChild<QListWidget *>(QStringLiteral("editorBacklinks"));
        auto *status = editor.findChild<QLabel *>(QStringLiteral("editorVaultStatus"));
        auto *previewToggle = editor.findChild<QPushButton *>(QStringLiteral("editorMarkdownPreview"));
        auto *preview = editor.findChild<QTextBrowser *>(QStringLiteral("editorRenderedMarkdown"));
        auto *rescan = editor.findChild<QAction *>(QStringLiteral("editorVaultRescan"));
        QVERIFY(panel && backlinks && status && previewToggle && preview && rescan);
        QVERIFY(panel->isHidden());
        QVERIFY(!rescan->isEnabled());

        QSignalSpy scanned(&editor, &EditorPanel::vaultScanned);
        editor.setVault(root);
        QCOMPARE(editor.vaultRoot(), root);
        QCOMPARE(KSharedConfig::openStateConfig()->group(QStringLiteral("EditorVault")).readEntry(QStringLiteral("Root")), root);
        QVERIFY(!panel->isHidden());
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 1, 10000);
        QCOMPARE(editor.vaultIndex().notes.size(), 3);

        QVERIFY(editor.openUrl(QUrl::fromLocalFile(home)));
        QTRY_COMPARE(editor.document()->text(), homeSource);
        QTRY_COMPARE(backlinks->count(), 2);
        QStringList sources{backlinks->item(0)->data(Qt::UserRole).toString(), backlinks->item(1)->data(Qt::UserRole).toString()};
        sources.sort();
        QCOMPARE(sources, (QStringList{other, root + QStringLiteral("/Sub/Third.md")}));
        QVERIFY2(status->text().contains(QStringLiteral("1 link doesn't lead to a note")), qPrintable(status->text()));

        previewToggle->setChecked(true);
        const QTextCursor link = preview->document()->find(QStringLiteral("the other note"));
        QVERIFY(!link.isNull());
        QCOMPARE(link.charFormat().anchorHref(), QStringLiteral("opus-note:Other"));
        QVERIFY(!preview->toPlainText().contains(QStringLiteral("[[")));
        QCOMPARE(editor.document()->text(), homeSource);

        Q_EMIT preview->anchorClicked(QUrl(QStringLiteral("opus-note:Missing")));
        QVERIFY2(status->text().contains(QStringLiteral("Missing")), qPrintable(status->text()));
        QCOMPARE(editor.document()->url(), QUrl::fromLocalFile(home));
        Q_EMIT preview->anchorClicked(QUrl(QStringLiteral("opus-note:../Outside")));
        QVERIFY2(status->text().contains(QStringLiteral("outside the vault")), qPrintable(status->text()));
        Q_EMIT preview->anchorClicked(QUrl(QStringLiteral("https://example.org/")));
        QCOMPARE(editor.document()->url(), QUrl::fromLocalFile(home));

        Q_EMIT preview->anchorClicked(QUrl(QStringLiteral("opus-note:Other")));
        QTRY_COMPARE(editor.document()->url(), QUrl::fromLocalFile(other));
        QTRY_COMPARE(backlinks->count(), 1);
        QCOMPARE(backlinks->item(0)->data(Qt::UserRole).toString(), home);
        Q_EMIT backlinks->itemActivated(backlinks->item(0));
        QTRY_COMPARE(editor.document()->url(), QUrl::fromLocalFile(home));

        write(root + QStringLiteral("/New.md"), "Also see [[home]].\n");
        rescan->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 2, 10000);
        QTRY_COMPARE(backlinks->count(), 3);

        QVERIFY(editor.openUrl(QUrl::fromLocalFile(outside)));
        QTRY_COMPARE(editor.document()->url(), QUrl::fromLocalFile(outside));
        QVERIFY(preview->toPlainText().contains(QStringLiteral("[[Home]]")));
        QCOMPARE(backlinks->count(), 0);
        QVERIFY(status->text().contains(QStringLiteral("Open a note in the vault")));

        editor.setVault({});
        QVERIFY(panel->isHidden());
        QVERIFY(!KSharedConfig::openStateConfig()->group(QStringLiteral("EditorVault")).hasKey(QStringLiteral("Root")));
    }

    void vaultListsOutgoingLinksAndTags()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString root = QFileInfo(directory.path()).canonicalFilePath() + QStringLiteral("/Vault");
        const auto write = [](const QString &path, const QByteArray &contents) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(contents);
        };
        const QString home = root + QStringLiteral("/Home.md");
        const QString plan = root + QStringLiteral("/Plan.md");
        const QString old = root + QStringLiteral("/Archive/Old.md");
        write(home, "---\ntags: [project]\n---\nSee [[Plan]], [[plan]], [[Missing]] and ![[image.png]]. #idea\n");
        write(plan, "#project/active plan\n");
        write(old, "#project old\n");
        write(root + QStringLiteral("/image.png"), "png");

        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        auto *links = editor.findChild<QListWidget *>(QStringLiteral("editorOutgoingLinks"));
        auto *tags = editor.findChild<QTreeWidget *>(QStringLiteral("editorTags"));
        QVERIFY(links && tags);
        QSignalSpy scanned(&editor, &EditorPanel::vaultScanned);
        editor.setVault(root);
        QTRY_COMPARE_WITH_TIMEOUT(scanned.size(), 1, 10000);
        QVERIFY(editor.openUrl(QUrl::fromLocalFile(home)));
        QTRY_COMPARE(links->count(), 3);
        QVERIFY2(links->item(0)->text().contains(QStringLiteral("Plan.md")), qPrintable(links->item(0)->text()));
        QVERIFY2(links->item(1)->text().contains(QStringLiteral("not found")), qPrintable(links->item(1)->text()));
        QVERIFY2(links->item(2)->text().contains(QStringLiteral("attachment")), qPrintable(links->item(2)->text()));

        QCOMPARE(tags->topLevelItemCount(), 2);
        QTreeWidgetItem *project = tags->topLevelItem(0);
        QCOMPARE(project->data(0, Qt::UserRole + 1).toString(), QStringLiteral("project"));
        QCOMPARE(project->childCount(), 2);
        QCOMPARE(project->child(0)->data(0, Qt::UserRole).toString(), old);
        QCOMPARE(project->child(1)->data(0, Qt::UserRole).toString(), plan);
        QCOMPARE(tags->topLevelItem(1)->childCount(), 0);

        Q_EMIT links->itemActivated(links->item(0));
        QTRY_COMPARE(editor.document()->url(), QUrl::fromLocalFile(plan));
        QTRY_COMPARE(tags->topLevelItemCount(), 1);
        QCOMPARE(tags->topLevelItem(0)->data(0, Qt::UserRole + 1).toString(), QStringLiteral("project/active"));
        QCOMPARE(tags->topLevelItem(0)->childCount(), 0);
        editor.openUrl(QUrl::fromLocalFile(home));
        QTRY_COMPARE(tags->topLevelItemCount(), 2);
        Q_EMIT tags->itemActivated(tags->topLevelItem(0)->child(0), 0);
        QTRY_COMPARE(editor.document()->url(), QUrl::fromLocalFile(old));
        editor.setVault({});
    }

    void workspaceVaultOverridesWithoutForgettingTheOwnVault()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString own = QFileInfo(directory.path()).canonicalFilePath() + QStringLiteral("/Own");
        const QString workspace = QFileInfo(directory.path()).canonicalFilePath() + QStringLiteral("/Workspace");
        QVERIFY(QDir().mkpath(own));
        QVERIFY(QDir().mkpath(workspace));
        const QString note = workspace + QStringLiteral("/Note.md");
        QFile file(note);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("A note.\n");
        file.close();

        EditorPanel editor(nullptr, directory.filePath(QStringLiteral("drafts")));
        editor.show();
        auto *choose = editor.findChild<QAction *>(QStringLiteral("editorVaultChoose"));
        auto *close = editor.findChild<QAction *>(QStringLiteral("editorVaultClose"));
        auto *title = editor.findChild<QLabel *>(QStringLiteral("editorVaultTitle"));
        QVERIFY(choose && close && title);
        editor.setVault(own);
        QVERIFY(editor.openUrl(QUrl::fromLocalFile(note)));
        QTRY_COMPARE(editor.document()->text(), QStringLiteral("A note.\n"));

        QSignalSpy scanned(&editor, &EditorPanel::vaultScanned);
        EditorPanel::setWorkspaceVault(workspace);
        QCOMPARE(EditorPanel::workspaceVault(), workspace);
        QCOMPARE(editor.vaultRoot(), workspace);
        QVERIFY(!choose->isEnabled());
        QVERIFY(!close->isEnabled());
        QVERIFY2(title->text().contains(QStringLiteral("Workspace vault")), qPrintable(title->text()));
        QTRY_VERIFY_WITH_TIMEOUT(!scanned.isEmpty(), 10000);
        QCOMPARE(editor.vaultIndex().notes.size(), 1);
        QCOMPARE(editor.document()->url(), QUrl::fromLocalFile(note));
        QCOMPARE(KSharedConfig::openStateConfig()->group(QStringLiteral("EditorVault")).readEntry(QStringLiteral("Root")), own);

        {
            EditorPanel later(nullptr, directory.filePath(QStringLiteral("drafts")));
            QCOMPARE(later.vaultRoot(), workspace);
        }

        EditorPanel::setWorkspaceVault({});
        QCOMPARE(editor.vaultRoot(), own);
        QVERIFY(choose->isEnabled());
        QVERIFY(close->isEnabled());
        QVERIFY2(title->text().startsWith(QStringLiteral("Vault")), qPrintable(title->text()));
        QCOMPARE(editor.document()->url(), QUrl::fromLocalFile(note));
        editor.setVault({});
    }
};

QTEST_MAIN(EditorPanelTest)
#include "editorpaneltest.moc"
