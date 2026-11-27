// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileencryptiondialog.h"
#include "folderview.h"
#include "informationpanel.h"
#include "mainwindow.h"
#include "pane.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KFileItem>
#include <KNewPasswordWidget>
#include <KPasswordLineEdit>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace FileEncryption;

namespace
{
bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeScript(const QString &path, const QByteArray &contents)
{
    return writeFile(path, contents) && QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner);
}

template<typename T>
T *child(QWidget *parent, const char *name)
{
    return parent->findChild<T *>(QLatin1String(name));
}
}

class FileEncryptionDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_home.isValid());
        m_oldGnupgHome = qgetenv("GNUPGHOME");
        m_oldPath = qgetenv("PATH");
        const QString gnupgHome = m_home.filePath(QStringLiteral("g"));
        QVERIFY(QDir().mkpath(gnupgHome));
        QFile::setPermissions(gnupgHome, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        qputenv("GNUPGHOME", gnupgHome.toLocal8Bit());

        // Fake AES Crypt and Kleopatra tools: aescrypt copies its input unless the
        // password is "wrong", and kleopatra records its arguments.
        m_bin = m_home.filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(m_bin));
        m_kleopatraLog = m_home.filePath(QStringLiteral("kleopatra.log"));
        QVERIFY(writeScript(m_bin + QStringLiteral("/aescrypt"),
                            "#!/bin/sh\n"
                            "if [ \"$(cat \"$3\")\" = wrong ]; then echo 'Error: password is incorrect' >&2; exit 1; fi\n"
                            "cp \"$6\" \"$5\"\n"));
        QVERIFY(
            writeScript(m_bin + QStringLiteral("/kleopatra"), QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$@\" > '%1'\n").arg(m_kleopatraLog).toLocal8Bit()));
        qputenv("PATH", QByteArray(m_bin.toLocal8Bit() + ':' + m_oldPath));
    }

    void cleanupTestCase()
    {
        if (!QStandardPaths::findExecutable(QStringLiteral("gpgconf")).isEmpty()) {
            QProcess::execute(QStringLiteral("gpgconf"), {QStringLiteral("--kill"), QStringLiteral("all")});
        }
        if (m_oldGnupgHome.isNull()) {
            qunsetenv("GNUPGHOME");
        } else {
            qputenv("GNUPGHOME", m_oldGnupgHome);
        }
        qputenv("PATH", m_oldPath);
    }

    void encryptsAndDecryptsWithAesCrypt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString plain = dir.filePath(QStringLiteral("photo.jpg"));
        QVERIFY(writeFile(plain, QByteArray("AES\x02\x00", 5)));

        FileEncryptionDialog encrypt(Operation::AesEncrypt, {plain});
        auto *start = child<QPushButton>(&encrypt, "encryptionStart");
        auto *passwords = child<KNewPasswordWidget>(&encrypt, "encryptionNewPassword");
        QVERIFY(start && passwords);
        QVERIFY(!start->isEnabled());
        const QList<QLineEdit *> edits = passwords->findChildren<QLineEdit *>();
        QVERIFY(edits.size() >= 2);
        for (QLineEdit *edit : edits) {
            edit->setText(QStringLiteral("correct horse"));
        }
        QTRY_VERIFY(start->isEnabled());
        QSignalSpy finished(&encrypt, &FileEncryptionDialog::finished);
        start->click();
        QVERIFY(finished.wait(30000));
        QVERIFY(!start->isEnabled());
        QVERIFY(!child<QWidget>(&encrypt, "encryptionOptions")->isEnabled());
        auto *results = child<QTreeWidget>(&encrypt, "encryptionResults");
        QCOMPARE(results->topLevelItemCount(), 1);
        QVERIFY(results->topLevelItem(0)->text(1).contains(QStringLiteral("photo.jpg.aes")));
        QCOMPARE(child<QLabel>(&encrypt, "encryptionStatus")->text(), QStringLiteral("Finished."));
        QVERIFY(QFileInfo::exists(plain + QStringLiteral(".aes")));

        FileEncryptionDialog wrong(Operation::AesDecrypt, {plain + QStringLiteral(".aes")});
        auto *edit = child<KPasswordLineEdit>(&wrong, "encryptionPasswordEdit");
        auto *wrongStart = child<QPushButton>(&wrong, "encryptionStart");
        QVERIFY(edit && !wrongStart->isEnabled());
        edit->setPassword(QStringLiteral("wrong"));
        QVERIFY(wrongStart->isEnabled());
        QSignalSpy wrongFinished(&wrong, &FileEncryptionDialog::finished);
        wrongStart->click();
        // The password is not kept in the dialog once the run starts.
        QVERIFY(edit->password().isEmpty());
        QVERIFY(wrongFinished.wait(30000));
        QCOMPARE(child<QTreeWidget>(&wrong, "encryptionResults")->topLevelItem(0)->text(1), QStringLiteral("Error: password is incorrect"));
        QCOMPARE(child<QLabel>(&wrong, "encryptionStatus")->text(), QStringLiteral("Finished: 1 of 1 file failed."));
        QCOMPARE(readFile(plain), QByteArray("AES\x02\x00", 5));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), (QStringList{QStringLiteral("photo.jpg"), QStringLiteral("photo.jpg.aes")}));
    }

    void reportsAMissingTool()
    {
        qputenv("PATH", m_home.filePath(QStringLiteral("empty")).toLocal8Bit());
        FileEncryptionDialog dialog(Operation::Encrypt, {m_home.filePath(QStringLiteral("a.txt"))});
        qputenv("PATH", QByteArray(m_bin.toLocal8Bit() + ':' + m_oldPath));
        QCOMPARE(child<QLabel>(&dialog, "encryptionStatus")->text(), missingText(Tool::GnuPG));
        QVERIFY(!child<QPushButton>(&dialog, "encryptionStart")->isEnabled());
        QVERIFY(!child<QWidget>(&dialog, "encryptionOptions")->isEnabled());
    }

    void encryptsForAKeyAndDecryptsAtOnce()
    {
        if (findExecutable(Tool::GnuPG).isEmpty()) {
            QSKIP("GnuPG is not installed");
        }
        QVERIFY(createKey());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString plain = dir.filePath(QStringLiteral("notes.txt"));
        QVERIFY(writeFile(plain, "secret\n"));

        FileEncryptionDialog encrypt(Operation::Encrypt, {plain});
        auto *keys = child<QTreeWidget>(&encrypt, "encryptionKeys");
        auto *start = child<QPushButton>(&encrypt, "encryptionStart");
        QVERIFY(keys && start && child<QCheckBox>(&encrypt, "encryptionArmor"));
        QTRY_COMPARE_WITH_TIMEOUT(keys->topLevelItemCount(), 1, 30000);
        QVERIFY(keys->topLevelItem(0)->text(0).contains(QStringLiteral("Dialog Test")));
        QVERIFY(!start->isEnabled());
        keys->topLevelItem(0)->setCheckState(0, Qt::Checked);
        QVERIFY(start->isEnabled());
        QSignalSpy finished(&encrypt, &FileEncryptionDialog::finished);
        start->click();
        QVERIFY(finished.wait(60000));
        QCOMPARE(child<QLabel>(&encrypt, "encryptionStatus")->text(), QStringLiteral("Finished."));
        const QString encrypted = plain + QStringLiteral(".gpg");
        QCOMPARE(identifyFile(encrypted), Kind::Encrypted);

        QVERIFY(QFile::remove(plain));
        FileEncryptionDialog decrypt(Operation::Decrypt, {encrypted});
        QVERIFY(!child<QWidget>(&decrypt, "encryptionOptions")->isVisibleTo(&decrypt));
        QSignalSpy decrypted(&decrypt, &FileEncryptionDialog::finished);
        QVERIFY(decrypted.wait(60000));
        QCOMPARE(readFile(plain), QByteArray("secret\n"));
    }

    void signsWithTheChosenKey()
    {
        if (findExecutable(Tool::GnuPG).isEmpty()) {
            QSKIP("GnuPG is not installed");
        }
        QVERIFY(createKey());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString data = dir.filePath(QStringLiteral("release.tar"));
        QVERIFY(writeFile(data, "release\n"));

        FileEncryptionDialog sign(Operation::Sign, {data});
        auto *signer = child<QComboBox>(&sign, "encryptionSigner");
        QVERIFY(signer);
        QTRY_COMPARE_WITH_TIMEOUT(signer->count(), 1, 30000);
        QCOMPARE(signer->currentData().toString(), m_fingerprint);
        child<QCheckBox>(&sign, "encryptionArmor")->setChecked(true);
        QSignalSpy finished(&sign, &FileEncryptionDialog::finished);
        child<QPushButton>(&sign, "encryptionStart")->click();
        QVERIFY(finished.wait(60000));
        QCOMPARE(identifyFile(data + QStringLiteral(".asc")), Kind::Signature);

        FileEncryptionDialog verify(Operation::Verify, {data});
        QSignalSpy verified(&verify, &FileEncryptionDialog::finished);
        QVERIFY(verified.wait(60000));
        QVERIFY(child<QTreeWidget>(&verify, "encryptionResults")->topLevelItem(0)->text(1).startsWith(QStringLiteral("Good signature")));
    }

    void showsTheEncryptionOfAFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString aes = dir.filePath(QStringLiteral("photo.jpg.aes"));
        const QString plain = dir.filePath(QStringLiteral("photo.jpg"));
        QVERIFY(writeFile(aes, QByteArray("AES\x02\x00", 5)));
        QVERIFY(writeFile(plain, "jpeg"));
        InformationPanel panel;
        auto *label = child<QLabel>(&panel, "informationEncryption");
        QVERIFY(label);
        panel.setItem(KFileItem(QUrl::fromLocalFile(aes)));
        QCOMPARE(label->text(), kindText(Kind::AesCrypt));
        panel.setItem(KFileItem(QUrl::fromLocalFile(plain)));
        QVERIFY(label->text().isEmpty());
    }

    void offersActionsThatSuitTheSelection()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString aes = dir.filePath(QStringLiteral("photo.jpg.aes"));
        const QString plain = dir.filePath(QStringLiteral("notes.txt"));
        QVERIFY(writeFile(aes, QByteArray("AES\x02\x00", 5)));
        QVERIFY(writeFile(plain, "notes"));
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("folder"))));

        MainWindow window;
        Pane *pane = window.activePane();
        pane->setUrl(QUrl::fromLocalFile(dir.path()));
        QTRY_COMPARE(pane->view()->itemCount(), 3);
        KActionCollection *actions = window.actionCollection();
        auto *encryption = qobject_cast<KActionMenu *>(actions->action(QStringLiteral("encryption")));
        QVERIFY(encryption);
        const auto action = [actions](const char *name) {
            return actions->action(QLatin1String(name));
        };

        pane->view()->selectUrls({QUrl::fromLocalFile(dir.filePath(QStringLiteral("folder")))});
        QTRY_VERIFY(!encryption->isEnabled());

        pane->view()->selectUrls({QUrl::fromLocalFile(aes)});
        QTRY_VERIFY(encryption->isEnabled());
        Q_EMIT encryption->menu()->aboutToShow();
        QVERIFY(action("aes_decrypt_files")->isEnabled());
        QVERIFY(action("aes_encrypt_files")->isEnabled());
        QVERIFY(!action("verify_signatures")->isEnabled());
        QVERIFY(!action("decrypt_files")->isEnabled());
        QVERIFY(action("kleopatra_encrypt_sign")->isVisible());
        QVERIFY(!action("kleopatra_decrypt_verify")->isEnabled());
        Q_EMIT encryption->menu()->aboutToHide();
        QVERIFY(action("decrypt_files")->isEnabled());

        // Decrypting a file that is not encrypted explains what to select instead of opening a dialog.
        pane->view()->selectUrls({QUrl::fromLocalFile(plain)});
        QTRY_COMPARE(pane->view()->selectedItems().size(), 1);
        action("aes_decrypt_files")->trigger();
        QVERIFY(!window.findChild<FileEncryptionDialog *>());

        action("aes_encrypt_files")->trigger();
        auto *dialog = window.findChild<FileEncryptionDialog *>();
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        QVERIFY(child<KNewPasswordWidget>(dialog, "encryptionNewPassword"));
        dialog->close();
        QTRY_VERIFY(!window.findChild<FileEncryptionDialog *>());

        action("kleopatra_encrypt_sign")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(m_kleopatraLog), 30000);
        QTRY_COMPARE(readFile(m_kleopatraLog), QByteArray("--encrypt-sign\n--\n" + plain.toLocal8Bit() + '\n'));
    }

private:
    bool createKey()
    {
        if (!m_fingerprint.isEmpty()) {
            return true;
        }
        QProcess process;
        process.setStandardInputFile(QProcess::nullDevice());
        process.start(findExecutable(Tool::GnuPG),
                      {QStringLiteral("--batch"),
                       QStringLiteral("--no-tty"),
                       QStringLiteral("--pinentry-mode"),
                       QStringLiteral("loopback"),
                       QStringLiteral("--passphrase"),
                       QString(),
                       QStringLiteral("--quick-gen-key"),
                       QStringLiteral("Dialog Test <dialog@example.org>"),
                       QStringLiteral("default"),
                       QStringLiteral("default"),
                       QStringLiteral("never")});
        if (!process.waitForFinished(120000) || process.exitCode() != 0) {
            qWarning() << process.readAllStandardError();
            return false;
        }
        FileEncryptionRunner runner;
        QSignalSpy listed(&runner, &FileEncryptionRunner::keysListed);
        runner.listKeys(true);
        if (!listed.wait(60000)) {
            return false;
        }
        const auto keys = listed.first().first().value<QList<Key>>();
        m_fingerprint = keys.isEmpty() ? QString() : keys.first().fingerprint;
        return !m_fingerprint.isEmpty();
    }

    QTemporaryDir m_home;
    QByteArray m_oldGnupgHome;
    QByteArray m_oldPath;
    QString m_bin;
    QString m_kleopatraLog;
    QString m_fingerprint;
};

QTEST_MAIN(FileEncryptionDialogTest)

#include "fileencryptiondialogtest.moc"
