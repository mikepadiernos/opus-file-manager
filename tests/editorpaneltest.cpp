// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorpanel.h"

#include <KTextEditor/Document>

#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class EditorPanelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
