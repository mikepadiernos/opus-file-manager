// SPDX-License-Identifier: GPL-3.0-or-later

#include "chooserdialog.h"
#include "filechooserportal.h"
#include "folderview.h"
#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPointer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace FileChooser;

class ChooserDialogTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;

    QUrl url(const QString &relative = QString()) const
    {
        return QUrl::fromLocalFile(relative.isEmpty() ? m_root.path() : m_root.filePath(relative));
    }

    Request request(Mode mode) const
    {
        Request request;
        request.mode = mode;
        request.currentFolder = url();
        return request;
    }

    static Filter globFilter(const QString &name, const QString &glob)
    {
        return {name, {{0, glob}}};
    }

    static void waitForItems(ChooserDialog &dialog, int count)
    {
        QTRY_VERIFY(!dialog.view()->isLoading());
        QTRY_COMPARE(dialog.view()->itemCount(), count);
    }

    static QVariantMap replyResults(const QDBusMessage &reply)
    {
        return qdbus_cast<QVariantMap>(reply.arguments().at(1).value<QDBusArgument>());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_root.isValid());
        QVERIFY(QDir(m_root.path()).mkpath(QStringLiteral("sub")));
        for (const QString &name : {QStringLiteral("a.txt"), QStringLiteral("b.txt"), QStringLiteral("c.png")}) {
            QFile file(m_root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("x");
        }
        registerTypes();
    }

    void opensTheSelectedFile()
    {
        ChooserDialog dialog(request(Mode::Open));
        QCOMPARE(dialog.folder(), url());
        QVERIFY(!dialog.nameEdit());
        QVERIFY(!dialog.filterCombo());
        waitForItems(dialog, 4);
        dialog.view()->selectUrls({url(QStringLiteral("a.txt"))});
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.chooserResult().urls, QList<QUrl>{url(QStringLiteral("a.txt"))});
    }

    void opensAFolderInsteadOfAcceptingIt()
    {
        ChooserDialog dialog(request(Mode::Open));
        waitForItems(dialog, 4);
        dialog.view()->selectUrls({url(QStringLiteral("sub"))});
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QTRY_COMPARE(dialog.folder(), url(QStringLiteral("sub")));
    }

    void refusesSeveralFilesUnlessMultiple()
    {
        ChooserDialog single(request(Mode::Open));
        waitForItems(single, 4);
        single.view()->selectUrls({url(QStringLiteral("a.txt")), url(QStringLiteral("b.txt"))});
        single.accept();
        QCOMPARE(single.result(), int(QDialog::Rejected));
        QVERIFY(!single.errorText().isEmpty());

        Request multiple = request(Mode::Open);
        multiple.multiple = true;
        ChooserDialog dialog(multiple);
        waitForItems(dialog, 4);
        dialog.view()->selectUrls({url(QStringLiteral("a.txt")), url(QStringLiteral("b.txt")), url(QStringLiteral("sub"))});
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.chooserResult().urls.size(), 2);
    }

    void filtersByTypeAndName()
    {
        Request filtered = request(Mode::Open);
        filtered.filters = {globFilter(QStringLiteral("Images"), QStringLiteral("*.png")), globFilter(QStringLiteral("Text"), QStringLiteral("*.txt"))};
        filtered.currentFilter = 0;
        ChooserDialog dialog(filtered);
        QVERIFY(dialog.filterCombo());
        QCOMPARE(dialog.filterCombo()->currentText(), QStringLiteral("Images"));
        waitForItems(dialog, 2);
        dialog.filterCombo()->setCurrentIndex(1);
        waitForItems(dialog, 3);
        dialog.searchEdit()->setText(QStringLiteral("a."));
        waitForItems(dialog, 2);
        dialog.searchEdit()->clear();
        waitForItems(dialog, 3);

        dialog.view()->selectUrls({url(QStringLiteral("b.txt"))});
        dialog.accept();
        QCOMPARE(dialog.chooserResult().filter, 1);
    }

    void selectsFolders()
    {
        Request folders = request(Mode::Open);
        folders.directory = true;
        ChooserDialog dialog(folders);
        waitForItems(dialog, 1);
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.chooserResult().urls, QList<QUrl>{url()});

        ChooserDialog selected(folders);
        waitForItems(selected, 1);
        selected.view()->selectUrls({url(QStringLiteral("sub"))});
        selected.accept();
        QCOMPARE(selected.chooserResult().urls, QList<QUrl>{url(QStringLiteral("sub"))});
    }

    void savesWithTheFilterExtensionAfterConfirmingOverwrite()
    {
        Request save = request(Mode::Save);
        save.currentName = QStringLiteral("a");
        save.filters = {globFilter(QStringLiteral("Text"), QStringLiteral("*.txt"))};
        save.currentFilter = 0;
        ChooserDialog dialog(save);
        QVERIFY(dialog.nameEdit());
        QCOMPARE(dialog.nameEdit()->text(), QStringLiteral("a"));
        QList<QUrl> asked;
        bool replace = false;
        dialog.setOverwriteConfirmation([&](const QList<QUrl> &existing) {
            asked = existing;
            return replace;
        });
        dialog.accept();
        QCOMPARE(asked, QList<QUrl>{url(QStringLiteral("a.txt"))});
        QCOMPARE(dialog.result(), int(QDialog::Rejected));

        replace = true;
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.chooserResult().urls, QList<QUrl>{url(QStringLiteral("a.txt"))});
    }

    void savesANewFileWithoutAsking()
    {
        Request save = request(Mode::Save);
        save.currentName = QStringLiteral("new.odt");
        ChooserDialog dialog(save);
        bool asked = false;
        dialog.setOverwriteConfirmation([&](const QList<QUrl> &) {
            asked = true;
            return false;
        });
        dialog.accept();
        QVERIFY(!asked);
        QCOMPARE(dialog.chooserResult().urls, QList<QUrl>{url(QStringLiteral("new.odt"))});
    }

    void enteringAFolderNameOpensIt()
    {
        Request save = request(Mode::Save);
        ChooserDialog dialog(save);
        dialog.nameEdit()->setText(QStringLiteral("sub"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QTRY_COMPARE(dialog.folder(), url(QStringLiteral("sub")));
        QVERIFY(dialog.nameEdit()->text().isEmpty());

        dialog.nameEdit()->setText(QStringLiteral("missing/file.txt"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(!dialog.errorText().isEmpty());
    }

    void savesSeveralFilesIntoAFolder()
    {
        Request save = request(Mode::SaveFiles);
        save.files = {QStringLiteral("a.txt"), QStringLiteral("new.txt")};
        ChooserDialog dialog(save);
        QList<QUrl> asked;
        dialog.setOverwriteConfirmation([&](const QList<QUrl> &existing) {
            asked = existing;
            return true;
        });
        dialog.accept();
        QCOMPARE(asked, QList<QUrl>{url(QStringLiteral("a.txt"))});
        QCOMPARE(dialog.chooserResult().urls, (QList<QUrl>{url(QStringLiteral("a.txt")), url(QStringLiteral("new.txt"))}));

        ChooserDialog intoFolder(save);
        waitForItems(intoFolder, 4);
        intoFolder.view()->selectUrls({url(QStringLiteral("sub"))});
        intoFolder.accept();
        QCOMPARE(intoFolder.chooserResult().urls, (QList<QUrl>{url(QStringLiteral("sub/a.txt")), url(QStringLiteral("sub/new.txt"))}));
    }

    void returnsChoices()
    {
        Request choices = request(Mode::Save);
        choices.currentName = QStringLiteral("choice.txt");
        choices.choices = {
            {QStringLiteral("encoding"),
             QStringLiteral("Encoding"),
             {{QStringLiteral("utf8"), QStringLiteral("UTF-8")}, {QStringLiteral("latin1"), QStringLiteral("Latin-1")}},
             QStringLiteral("latin1")},
            {QStringLiteral("bom"), QStringLiteral("Write a byte order mark"), {}, QStringLiteral("true")},
        };
        ChooserDialog dialog(choices);
        auto *encoding = dialog.findChild<QComboBox *>(QStringLiteral("fileChooserChoice-encoding"));
        auto *bom = dialog.findChild<QCheckBox *>(QStringLiteral("fileChooserChoice-bom"));
        QVERIFY(encoding && bom);
        QCOMPARE(encoding->currentData().toString(), QStringLiteral("latin1"));
        QVERIFY(bom->isChecked());
        encoding->setCurrentIndex(0);
        bom->setChecked(false);
        dialog.accept();
        QCOMPARE(dialog.chooserResult().choices,
                 (ChoiceValues{{QStringLiteral("encoding"), QStringLiteral("utf8")}, {QStringLiteral("bom"), QStringLiteral("false")}}));
    }

    void startsInTheLastFolderWhenNoneIsSuggested()
    {
        Request open;
        open.lastFolder = url(QStringLiteral("sub"));
        ChooserDialog dialog(open);
        QCOMPARE(dialog.folder(), url(QStringLiteral("sub")));

        open.lastFolder = url(QStringLiteral("missing"));
        ChooserDialog home(open);
        QCOMPARE(home.folder(), QUrl::fromLocalFile(QDir::homePath()));

        open.currentFolder = url();
        open.lastFolder = url(QStringLiteral("sub"));
        ChooserDialog suggested(open);
        QCOMPARE(suggested.folder(), url());
    }

    void remembersTheLastFolderOnTheBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("No session bus");
        }
        const KSharedConfigPtr state = KSharedConfig::openStateConfig();
        state->deleteGroup(stateGroupName());
        QVERIFY(state->sync());
        Settings::self()->setDefaults();
        Settings::setFileChooserStartFolder(Settings::EnumFileChooserStartFolder::LastFolder);
        QVERIFY(Settings::self()->save());

        FileChooserPortal portal;
        QVERIFY(bus.registerObject(FileChooserPortal::objectPath(), &portal, QDBusConnection::ExportScriptableSlots));
        ChooserDialog *dialog = nullptr;
        connect(&portal, &FileChooserPortal::dialogShown, this, [&dialog](ChooserDialog *shown) {
            dialog = shown;
        });
        QDBusConnection client = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("chooser-folder-client"));
        QVERIFY(client.isConnected());
        const auto openFile = [&](const QString &appId, const QVariantMap &options) {
            dialog = nullptr;
            QDBusMessage call = QDBusMessage::createMethodCall(bus.baseService(),
                                                               FileChooserPortal::objectPath(),
                                                               QStringLiteral("org.freedesktop.impl.portal.FileChooser"),
                                                               QStringLiteral("OpenFile"));
            call << QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/freedesktop/portal/desktop/request/1_1/folder"))) << appId << QString()
                 << QStringLiteral("Open") << options;
            return client.asyncCall(call);
        };
        const QUrl home = QUrl::fromLocalFile(QDir::homePath());

        // Accepting a file in a suggested folder remembers that folder for the application.
        QDBusPendingCall pending = openFile(QStringLiteral("org.example.App"),
                                            {{QStringLiteral("current_folder"), QByteArray(QFile::encodeName(m_root.path()) + QByteArrayView("", 1))}});
        QTRY_VERIFY(dialog);
        waitForItems(*dialog, 4);
        dialog->view()->selectUrls({url(QStringLiteral("a.txt"))});
        dialog->accept();
        QTRY_VERIFY(pending.isFinished());
        QCOMPARE(pending.reply().arguments().at(0).toUInt(), uint(Accepted));
        state->reparseConfiguration();
        QCOMPARE(lastFolder(state->group(stateGroupName()), QStringLiteral("org.example.App")), url());

        pending = openFile(QStringLiteral("org.example.App"), {});
        QTRY_VERIFY(dialog);
        QCOMPARE(dialog->folder(), url());
        dialog->reject();
        QTRY_VERIFY(pending.isFinished());

        pending = openFile(QStringLiteral("org.example.Other"), {});
        QTRY_VERIFY(dialog);
        QCOMPARE(dialog->folder(), home);
        dialog->reject();
        QTRY_VERIFY(pending.isFinished());

        // The home folder setting neither uses nor remembers the last folder.
        Settings::setFileChooserStartFolder(Settings::EnumFileChooserStartFolder::Home);
        QVERIFY(Settings::self()->save());
        pending = openFile(QStringLiteral("org.example.App"), {});
        QTRY_VERIFY(dialog);
        QCOMPARE(dialog->folder(), home);
        dialog->reject();
        QTRY_VERIFY(pending.isFinished());

        bus.unregisterObject(FileChooserPortal::objectPath());
        QDBusConnection::disconnectFromBus(QStringLiteral("chooser-folder-client"));
        state->deleteGroup(stateGroupName());
        QVERIFY(state->sync());
    }

    void answersRequestsOnTheBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            QSKIP("No session bus");
        }
        FileChooserPortal portal;
        QVERIFY(bus.registerObject(FileChooserPortal::objectPath(), &portal, QDBusConnection::ExportScriptableSlots));
        ChooserDialog *dialog = nullptr;
        connect(&portal, &FileChooserPortal::dialogShown, this, [&dialog](ChooserDialog *shown) {
            dialog = shown;
        });
        QDBusConnection client = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("chooser-test-client"));
        QVERIFY(client.isConnected());

        const QString handle = QStringLiteral("/org/freedesktop/portal/desktop/request/1_1/test");
        QDBusMessage call = QDBusMessage::createMethodCall(bus.baseService(),
                                                           FileChooserPortal::objectPath(),
                                                           QStringLiteral("org.freedesktop.impl.portal.FileChooser"),
                                                           QStringLiteral("OpenFile"));
        const QVariantMap options{
            {QStringLiteral("current_folder"), QByteArray(QFile::encodeName(m_root.path()) + QByteArrayView("", 1))},
            {QStringLiteral("filters"), QVariant::fromValue(FilterList{globFilter(QStringLiteral("Text"), QStringLiteral("*.txt"))})},
        };
        call << QVariant::fromValue(QDBusObjectPath(handle)) << QStringLiteral("org.example.App") << QString() << QStringLiteral("Pick a text file") << options;
        QDBusPendingCall pending = client.asyncCall(call);
        QTRY_VERIFY(dialog);
        QCOMPARE(dialog->windowTitle(), QStringLiteral("Pick a text file"));
        QCOMPARE(portal.pendingRequests(), 1);
        waitForItems(*dialog, 3);
        dialog->view()->selectUrls({url(QStringLiteral("b.txt"))});
        dialog->accept();
        QTRY_VERIFY(pending.isFinished());
        QCOMPARE(pending.reply().type(), QDBusMessage::ReplyMessage);
        QCOMPARE(pending.reply().signature(), QStringLiteral("ua{sv}"));
        QCOMPARE(pending.reply().arguments().at(0).toUInt(), uint(Accepted));
        const QVariantMap results = replyResults(pending.reply());
        QCOMPARE(results.value(QStringLiteral("uris")).toStringList(), QStringList{url(QStringLiteral("b.txt")).toString()});
        QCOMPARE(qdbus_cast<Filter>(results.value(QStringLiteral("current_filter")).value<QDBusArgument>()).name, QStringLiteral("Text"));
        QCOMPARE(portal.pendingRequests(), 0);

        // Cancelling replies with the cancelled response.
        dialog = nullptr;
        pending = client.asyncCall(call);
        QTRY_VERIFY(dialog);
        dialog->reject();
        QTRY_VERIFY(pending.isFinished());
        QCOMPARE(pending.reply().arguments().at(0).toUInt(), uint(Cancelled));

        // Closing the request closes the dialog and ends the request.
        dialog = nullptr;
        pending = client.asyncCall(call);
        QTRY_VERIFY(dialog);
        QPointer<ChooserDialog> shown = dialog;
        const QDBusPendingCall close = client.asyncCall(
            QDBusMessage::createMethodCall(bus.baseService(), handle, QStringLiteral("org.freedesktop.impl.portal.Request"), QStringLiteral("Close")));
        QTRY_VERIFY(pending.isFinished());
        QCOMPARE(pending.reply().arguments().at(0).toUInt(), uint(Ended));
        QTRY_VERIFY(close.isFinished());
        QCOMPARE(close.reply().type(), QDBusMessage::ReplyMessage);
        QTRY_VERIFY(!shown);

        bus.unregisterObject(FileChooserPortal::objectPath());
        QDBusConnection::disconnectFromBus(QStringLiteral("chooser-test-client"));
    }
};

QTEST_MAIN(ChooserDialogTest)

#include "chooserdialogtest.moc"
