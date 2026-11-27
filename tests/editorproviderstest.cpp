// SPDX-License-Identifier: GPL-3.0-or-later

#include "editorproviders.h"

#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using EditorProviders::Editor;

class EditorProvidersTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void describesEditors()
    {
        const QList<Editor> editors = EditorProviders::editors();
        QCOMPARE(editors, (QList<Editor>{Editor::KWrite, Editor::Kate, Editor::Neovim, Editor::Rune, Editor::Micro, Editor::Nano}));
        QSet<QString> ids;
        for (const Editor editor : editors) {
            ids.insert(EditorProviders::id(editor));
            QVERIFY(!EditorProviders::name(editor).isEmpty());
            QVERIFY(!EditorProviders::iconName(editor).isEmpty());
            QVERIFY(!EditorProviders::executable(editor).isEmpty());
            QCOMPARE(EditorProviders::desktopName(editor).isEmpty(), EditorProviders::runsInTerminal(editor));
        }
        QCOMPARE(ids.size(), editors.size());
        QCOMPARE(EditorProviders::executable(Editor::Neovim), QStringLiteral("nvim"));
        QCOMPARE(EditorProviders::desktopName(Editor::KWrite), QStringLiteral("org.kde.kwrite"));
        QCOMPARE(EditorProviders::desktopName(Editor::Kate), QStringLiteral("org.kde.kate"));
        QVERIFY(!EditorProviders::runsInTerminal(Editor::Kate));
        QVERIFY(EditorProviders::runsInTerminal(Editor::Nano));
    }

    void reportsMissingEditors()
    {
        QTemporaryDir bin;
        QVERIFY(bin.isValid());
        const QByteArray path = qgetenv("PATH");
        const auto restore = qScopeGuard([&path] {
            qputenv("PATH", path);
        });
        qputenv("PATH", bin.path().toLocal8Bit());
        for (const Editor editor : EditorProviders::editors()) {
            QVERIFY(EditorProviders::findExecutable(editor).isEmpty());
            QVERIFY(EditorProviders::unavailableReason(editor).contains(EditorProviders::executable(editor)));
        }
        QFile nvim(bin.filePath(QStringLiteral("nvim")));
        QVERIFY(nvim.open(QIODevice::WriteOnly));
        nvim.write("#!/bin/sh\n");
        nvim.close();
        QVERIFY(nvim.setPermissions(nvim.permissions() | QFileDevice::ExeOwner));
        QCOMPARE(EditorProviders::findExecutable(Editor::Neovim), nvim.fileName());
        QVERIFY(EditorProviders::unavailableReason(Editor::Neovim).isEmpty());
    }

    void passesAbsolutePaths()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString dashed = folder.filePath(QStringLiteral("-n"));
        const QString spaced = folder.filePath(QStringLiteral("with space.txt"));
        QString error;
        const QStringList args =
            EditorProviders::arguments(Editor::Neovim,
                                       {QUrl::fromLocalFile(dashed), QUrl::fromLocalFile(folder.path() + QStringLiteral("/./with space.txt"))},
                                       &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(args, (QStringList{QDir::cleanPath(dashed), QDir::cleanPath(spaced)}));
        for (const QString &arg : args) {
            QVERIFY(arg.startsWith(QLatin1Char('/')));
        }
        QCOMPARE(EditorProviders::workingDirectory({QUrl::fromLocalFile(spaced)}), QDir::cleanPath(folder.path()));
    }

    void rejectsUnsupportedSelections()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QString error;
        QVERIFY(EditorProviders::arguments(Editor::Kate, {}, &error).isEmpty());
        QVERIFY(!error.isEmpty());

        error.clear();
        QVERIFY(EditorProviders::arguments(Editor::Micro, {QUrl::fromLocalFile(folder.path())}, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("Folders")));

        const QUrl remote(QStringLiteral("sftp://example.invalid/home/me/notes.txt"));
        for (const Editor editor : {Editor::Neovim, Editor::Rune, Editor::Micro, Editor::Nano}) {
            error.clear();
            QVERIFY(EditorProviders::arguments(editor, {remote}, &error).isEmpty());
            QVERIFY(error.contains(QStringLiteral("local files")));
        }
        for (const Editor editor : {Editor::KWrite, Editor::Kate}) {
            error.clear();
            QCOMPARE(EditorProviders::arguments(editor, {remote}, &error), QStringList{remote.toString()});
            QVERIFY(error.isEmpty());
        }
        QCOMPARE(EditorProviders::workingDirectory({remote}), QDir::homePath());
        QCOMPARE(EditorProviders::workingDirectory({}), QDir::homePath());
    }
};

QTEST_GUILESS_MAIN(EditorProvidersTest)

#include "editorproviderstest.moc"
