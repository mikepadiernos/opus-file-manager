// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileencryption.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace FileEncryption;

namespace
{
QByteArray armored(const char *type, const QByteArray &packet)
{
    return QByteArray("-----BEGIN PGP ") + type + "-----\nComment: test\n\n" + packet.toBase64() + "\n=abcd\n-----END PGP " + type + "-----\n";
}

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

struct Run {
    QList<Outcome> outcomes;
    bool canceled = false;
};

Run run(FileEncryptionRunner &runner, Operation operation, const QStringList &paths, const Options &options = {}, const QString &password = {})
{
    Run result;
    QObject::connect(&runner, &FileEncryptionRunner::fileFinished, &runner, [&result](int, const Outcome &outcome) {
        result.outcomes.append(outcome);
    });
    QSignalSpy finished(&runner, &FileEncryptionRunner::finished);
    runner.start(operation, paths, options, password);
    if (!finished.wait(60000)) {
        qWarning() << "runner did not finish";
    }
    runner.disconnect(&runner);
    result.canceled = !finished.isEmpty() && finished.first().first().toBool();
    return result;
}
}

class FileEncryptionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void identifiesFiles_data();
    void identifiesFiles();
    void namesOutputs();
    void avoidsExistingFiles();
    void findsSignatures();
    void parsesKeys();
    void parsesSignatures();
    void describesSignatures();
    void buildsArguments();
    void readsLastError();
    void reportsMissingTools();
    void encryptsAndDecryptsWithGnuPG();
    void signsAndVerifiesWithGnuPG();
    void keepsNothingAfterFailure();
    void passesAesPasswordsThroughKeyFile();
    void removesFailedAesOutput();

private:
    bool gpg(const QStringList &arguments, QByteArray *output = nullptr);

    QTemporaryDir m_home;
    QByteArray m_oldGnupgHome;
    QByteArray m_oldPath;
    QString m_fingerprint;
};

void FileEncryptionTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_home.isValid());
    m_oldGnupgHome = qgetenv("GNUPGHOME");
    m_oldPath = qgetenv("PATH");
    const QString gnupgHome = m_home.filePath(QStringLiteral("g"));
    QVERIFY(QDir().mkpath(gnupgHome));
    QFile::setPermissions(gnupgHome, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    qputenv("GNUPGHOME", gnupgHome.toLocal8Bit());
}

void FileEncryptionTest::cleanupTestCase()
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

bool FileEncryptionTest::gpg(const QStringList &arguments, QByteArray *output)
{
    QProcess process;
    process.setStandardInputFile(QProcess::nullDevice());
    process.start(findExecutable(Tool::GnuPG), QStringList{QStringLiteral("--batch"), QStringLiteral("--no-tty")} + arguments);
    if (!process.waitForFinished(120000) || process.exitCode() != 0) {
        qWarning() << process.readAllStandardError();
        return false;
    }
    if (output) {
        *output = process.readAllStandardOutput();
    }
    return true;
}

void FileEncryptionTest::identifiesFiles_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QByteArray>("head");
    QTest::addColumn<Kind>("kind");

    QTest::newRow("aes header") << QStringLiteral("photo.jpg.aes") << QByteArray("AES\x02\x00", 5) << Kind::AesCrypt;
    QTest::newRow("aes header without suffix") << QStringLiteral("backup") << QByteArray("AES\x03\x00", 5) << Kind::AesCrypt;
    QTest::newRow("aes suffix with other data") << QStringLiteral("notes.aes") << QByteArray("plain text") << Kind::None;
    QTest::newRow("aes suffix unreadable") << QStringLiteral("notes.aes") << QByteArray() << Kind::AesCrypt;
    QTest::newRow("clear-signed") << QStringLiteral("notes.txt") << QByteArray("\n-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA256\n") << Kind::Signed;
    QTest::newRow("armoured signature") << QStringLiteral("a.tar.asc") << QByteArray("-----BEGIN PGP SIGNATURE-----\n\niQ") << Kind::Signature;
    QTest::newRow("armoured public-key message") << QStringLiteral("a.asc") << armored("MESSAGE", QByteArray("\x84\x5e\x03", 3)) << Kind::Encrypted;
    QTest::newRow("armoured password message") << QStringLiteral("a.asc") << armored("MESSAGE", QByteArray("\xc3\x0d\x04", 3)) << Kind::Encrypted;
    QTest::newRow("armoured signed message") << QStringLiteral("a.asc") << armored("MESSAGE", QByteArray("\xa3\x01\x01", 3)) << Kind::Signed;
    QTest::newRow("armoured with BOM") << QStringLiteral("a.asc") << QByteArray("\xef\xbb\xbf-----BEGIN PGP SIGNATURE-----\n") << Kind::Signature;
    QTest::newRow("binary public-key message") << QStringLiteral("a.gpg") << QByteArray("\x84\x5e\x03", 3) << Kind::Encrypted;
    QTest::newRow("binary new-format message") << QStringLiteral("a.pgp") << QByteArray("\xc1\x5e\x03", 3) << Kind::Encrypted;
    QTest::newRow("binary signature") << QStringLiteral("a.sig") << QByteArray("\x88\x75\x04", 3) << Kind::Signature;
    QTest::newRow("binary one-pass signature") << QStringLiteral("a.gpg") << QByteArray("\x90\x0d\x03", 3) << Kind::Signed;
    QTest::newRow("binary key") << QStringLiteral("key.gpg") << QByteArray("\x99\x01\x0d", 3) << Kind::None;
    QTest::newRow("png named png") << QStringLiteral("image.png") << QByteArray("\x89PNG\r\n", 6) << Kind::None;
    QTest::newRow("text named gpg") << QStringLiteral("readme.gpg") << QByteArray("hello") << Kind::None;
    QTest::newRow("gpg suffix unreadable") << QStringLiteral("a.GPG") << QByteArray() << Kind::Encrypted;
    QTest::newRow("sig suffix unreadable") << QStringLiteral("a.sig") << QByteArray() << Kind::Signature;
}

void FileEncryptionTest::identifiesFiles()
{
    QFETCH(QString, name);
    QFETCH(QByteArray, head);
    QFETCH(Kind, kind);
    QCOMPARE(identify(name, head), kind);

    QVERIFY(hasCandidateSuffix(QStringLiteral("a.GPG")));
    QVERIFY(hasCandidateSuffix(QStringLiteral("a.aes")));
    QVERIFY(!hasCandidateSuffix(QStringLiteral("a.txt")));
    QVERIFY(!hasCandidateSuffix(QStringLiteral(".gpg")));
    QVERIFY(kindText(Kind::None).isEmpty());
    QVERIFY(!kindText(kind == Kind::None ? Kind::Encrypted : kind).isEmpty());
}

void FileEncryptionTest::namesOutputs()
{
    QCOMPARE(outputName(Operation::Encrypt, QStringLiteral("a.txt")), QStringLiteral("a.txt.gpg"));
    QCOMPARE(outputName(Operation::Encrypt, QStringLiteral("a.txt"), true), QStringLiteral("a.txt.asc"));
    QCOMPARE(outputName(Operation::Sign, QStringLiteral("a.txt")), QStringLiteral("a.txt.sig"));
    QCOMPARE(outputName(Operation::Sign, QStringLiteral("a.txt"), true), QStringLiteral("a.txt.asc"));
    QCOMPARE(outputName(Operation::Decrypt, QStringLiteral("a.txt.gpg")), QStringLiteral("a.txt"));
    QCOMPARE(outputName(Operation::Decrypt, QStringLiteral("a.txt.PGP")), QStringLiteral("a.txt"));
    QCOMPARE(outputName(Operation::Decrypt, QStringLiteral("a.txt.asc")), QStringLiteral("a.txt"));
    QCOMPARE(outputName(Operation::Decrypt, QStringLiteral("message")), QStringLiteral("message.out"));
    QCOMPARE(outputName(Operation::Decrypt, QStringLiteral(".gpg")), QStringLiteral(".gpg.out"));
    QCOMPARE(outputName(Operation::AesEncrypt, QStringLiteral("a.jpg")), QStringLiteral("a.jpg.aes"));
    QCOMPARE(outputName(Operation::AesDecrypt, QStringLiteral("a.jpg.aes")), QStringLiteral("a.jpg"));
    QCOMPARE(outputName(Operation::AesDecrypt, QStringLiteral("a.jpg")), QStringLiteral("a.jpg.out"));
    QVERIFY(outputName(Operation::Verify, QStringLiteral("a.sig")).isEmpty());
}

void FileEncryptionTest::avoidsExistingFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString free = dir.filePath(QStringLiteral("report.pdf"));
    QCOMPARE(uniquePath(free), free);
    QVERIFY(writeFile(free, "x"));
    const QString other = uniquePath(free);
    QVERIFY(other != free);
    QCOMPARE(QFileInfo(other).absolutePath(), dir.path());
    QVERIFY(!QFileInfo::exists(other));
    QVERIFY(QFile::link(dir.filePath(QStringLiteral("missing")), dir.filePath(QStringLiteral("dangling"))));
    QVERIFY(uniquePath(dir.filePath(QStringLiteral("dangling"))) != dir.filePath(QStringLiteral("dangling")));
}

void FileEncryptionTest::findsSignatures()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString data = dir.filePath(QStringLiteral("release.tar"));
    const QString signature = data + QStringLiteral(".sig");
    const QString signedMessage = dir.filePath(QStringLiteral("notes.asc"));
    QVERIFY(writeFile(data, "data"));
    QVERIFY(writeFile(signature, QByteArray("\x88\x75\x04", 3)));
    QVERIFY(writeFile(signedMessage, "-----BEGIN PGP SIGNED MESSAGE-----\n"));
    QString foundSignature;
    QString foundData;
    QVERIFY(verificationFiles(signature, &foundSignature, &foundData));
    QCOMPARE(foundSignature, signature);
    QCOMPARE(foundData, data);
    foundSignature.clear();
    foundData.clear();
    QVERIFY(verificationFiles(data, &foundSignature, &foundData));
    QCOMPARE(foundSignature, signature);
    QCOMPARE(foundData, data);
    QVERIFY(verificationFiles(signedMessage, &foundSignature, &foundData));
    QCOMPARE(foundSignature, signedMessage);
    QVERIFY(foundData.isEmpty());
    QVERIFY(!verificationFiles(dir.filePath(QStringLiteral("other.txt")), &foundSignature, &foundData));
}

void FileEncryptionTest::parsesKeys()
{
    const QByteArray listing =
        "tru::1:1700000000:0:3:1:5\n"
        "pub:u:255:22:0123456789ABCDEF:1700000000:::u:::scESC:::::ed25519:::0:\n"
        "fpr:::::::::AAAA0123456789ABCDEF0123456789ABCDEF0123:\n"
        "uid:u::::1700000000::HASH::Ann Example <ann@example.org>::::::::::0:\n"
        "uid:r::::1700000000::HASH::Old Name <old@example.org>::::::::::0:\n"
        "uid:u::::1700000000::HASH::Ann \\x3a Work <ann@work.example>::::::::::0:\n"
        "sub:u:255:18:1111111111111111:1700000000::::::e:::::cv25519::\n"
        "fpr:::::::::BBBB1111111111111111111111111111111111111:\n"
        "pub:e:255:22:FEDCBA9876543210:1600000000:1650000000::-:::sc:::::ed25519:::0:\n"
        "fpr:::::::::CCCCFEDCBA9876543210FEDCBA9876543210FEDC:\n"
        "uid:e::::1600000000::HASH::Expired <exp@example.org>::::::::::0:\n"
        "pub:-:255:22:00000000000000AA:1700000000:::-:::scSD:::::ed25519:::0:\n"
        "fpr:::::::::DDDD00000000000000000000000000000000000AA:\n"
        "pub:broken\n"
        "uid:u::::::::Orphan::::\n";
    const QList<Key> keys = parseKeys(listing);
    QCOMPARE(keys.size(), 3);
    QCOMPARE(keys.at(0).fingerprint, QStringLiteral("AAAA0123456789ABCDEF0123456789ABCDEF0123"));
    QCOMPARE(keys.at(0).keyId, QStringLiteral("0123456789ABCDEF"));
    QCOMPARE(keys.at(0).userIds, (QStringList{QStringLiteral("Ann Example <ann@example.org>"), QStringLiteral("Ann : Work <ann@work.example>")}));
    QVERIFY(keys.at(0).canEncrypt);
    QVERIFY(keys.at(0).canSign);
    QVERIFY(!keys.at(0).unusable);
    QVERIFY(isValid(keys.at(0)));
    QCOMPARE(keyText(keys.at(0)), QStringLiteral("Ann Example <ann@example.org> (0123456789ABCDEF)"));
    QVERIFY(keys.at(1).unusable);
    QVERIFY(!keys.at(1).canEncrypt);
    QVERIFY(!isValid(keys.at(1)));
    QVERIFY(keys.at(2).unusable);
    QVERIFY(keys.at(2).userIds.isEmpty());

    const QList<Key> secret = parseKeys(
        "sec:u:255:22:0123456789ABCDEF:1700000000:::u:::scESC:::#:::23::0:\n"
        "fpr:::::::::AAAA0123456789ABCDEF0123456789ABCDEF0123:\n"
        "sec:u:255:22:1234567890ABCDEF:1700000000:::u:::scESC:::+:::23::0:\n"
        "fpr:::::::::EEEE1234567890ABCDEF1234567890ABCDEF1234:\n");
    QCOMPARE(secret.size(), 2);
    QVERIFY(secret.at(0).unusable);
    QVERIFY(!secret.at(1).unusable);
    QVERIFY(secret.at(1).canSign);
    QVERIFY(listKeysArguments(true).contains(QStringLiteral("--list-secret-keys")));
    QVERIFY(listKeysArguments(false).contains(QStringLiteral("--with-colons")));
}

void FileEncryptionTest::parsesSignatures()
{
    const QByteArray status =
        "[GNUPG:] NEWSIG\n"
        "[GNUPG:] KEY_CONSIDERED AAAA 0\n"
        "[GNUPG:] GOODSIG 0123456789ABCDEF Ann Example %3Cann@example.org%3E\n"
        "[GNUPG:] VALIDSIG SUBKEYFPR 2024-01-01 1704067200 0 4 0 22 10 00 PRIMARYFPR\n"
        "[GNUPG:] TRUST_ULTIMATE 0 pgp\n"
        "[GNUPG:] NEWSIG\n"
        "[GNUPG:] ERRSIG FEDCBA9876543210 22 10 00 1704067200 9 CCCCFPR\n"
        "[GNUPG:] NO_PUBKEY FEDCBA9876543210\n"
        "[GNUPG:] BADSIG 1111111111111111 Mallory\n"
        "[GNUPG:] EXPKEYSIG 2222222222222222 Old Key\n"
        "[GNUPG:] VALIDSIG FPR2 2020-01-01 20200101T120000 0 4 0 22 10 00\n"
        "[GNUPG:] TRUST_UNDEFINED 0 pgp\n"
        "gpg: noise\n";
    const QList<Signature> signatures = parseSignatures(status);
    QCOMPARE(signatures.size(), 4);
    QCOMPARE(signatures.at(0).status, Signature::Status::Good);
    QCOMPARE(signatures.at(0).signer, QStringLiteral("Ann Example <ann@example.org>"));
    QCOMPARE(signatures.at(0).keyId, QStringLiteral("0123456789ABCDEF"));
    QCOMPARE(signatures.at(0).fingerprint, QStringLiteral("PRIMARYFPR"));
    QCOMPARE(signatures.at(0).created, QDateTime::fromSecsSinceEpoch(1704067200));
    QCOMPARE(signatures.at(0).trust, Signature::Trust::Ultimate);
    QCOMPARE(signatures.at(1).status, Signature::Status::MissingKey);
    QCOMPARE(signatures.at(1).fingerprint, QStringLiteral("CCCCFPR"));
    QCOMPARE(signatures.at(2).status, Signature::Status::Bad);
    QCOMPARE(signatures.at(2).signer, QStringLiteral("Mallory"));
    QCOMPARE(signatures.at(3).status, Signature::Status::ExpiredKey);
    QCOMPARE(signatures.at(3).fingerprint, QStringLiteral("FPR2"));
    QCOMPARE(signatures.at(3).created, QDateTime(QDate(2020, 1, 1), QTime(12, 0), QTimeZone::UTC));
    QCOMPARE(signatures.at(3).trust, Signature::Trust::Undefined);
    QVERIFY(parseSignatures("[GNUPG:] NEWSIG\n[GNUPG:] TRUST_FULLY 0\n").isEmpty());
}

void FileEncryptionTest::describesSignatures()
{
    Signature signature;
    signature.keyId = QStringLiteral("0123456789ABCDEF");
    signature.status = Signature::Status::Good;
    signature.trust = Signature::Trust::Full;
    signature.signer = QStringLiteral("Ann");
    QCOMPARE(signatureText(signature), QStringLiteral("Good signature by Ann"));
    signature.trust = Signature::Trust::Undefined;
    QVERIFY(signatureText(signature).contains(QStringLiteral("not certified")));
    signature.status = Signature::Status::MissingKey;
    signature.signer.clear();
    QVERIFY(signatureText(signature).contains(QStringLiteral("0123456789ABCDEF")));
    signature.status = Signature::Status::Bad;
    QVERIFY(signatureText(signature).startsWith(QStringLiteral("Bad signature")));
}

void FileEncryptionTest::buildsArguments()
{
    Options options;
    options.recipients = {QStringLiteral("FPR1"), QStringLiteral("FPR2")};
    options.armor = true;
    QCOMPARE(gpgArguments(Operation::Encrypt, options, QStringLiteral("/a/-in"), QStringLiteral("/a/-in.asc")),
             (QStringList{QStringLiteral("--batch"),
                          QStringLiteral("--no-tty"),
                          QStringLiteral("--status-fd"),
                          QStringLiteral("1"),
                          QStringLiteral("--armor"),
                          QStringLiteral("--encrypt"),
                          QStringLiteral("--recipient"),
                          QStringLiteral("FPR1"),
                          QStringLiteral("--recipient"),
                          QStringLiteral("FPR2"),
                          QStringLiteral("--output"),
                          QStringLiteral("/a/-in.asc"),
                          QStringLiteral("--"),
                          QStringLiteral("/a/-in")}));
    Options password;
    password.password = true;
    const QStringList symmetric = gpgArguments(Operation::Encrypt, password, QStringLiteral("/a/in"), QStringLiteral("/a/in.gpg"));
    QVERIFY(symmetric.contains(QStringLiteral("--symmetric")));
    QVERIFY(!symmetric.contains(QStringLiteral("--encrypt")));
    Options signer;
    signer.signer = QStringLiteral("FPR1");
    const QStringList sign = gpgArguments(Operation::Sign, signer, QStringLiteral("/a/in"), QStringLiteral("/a/in.sig"));
    QVERIFY(sign.contains(QStringLiteral("--detach-sign")));
    QCOMPARE(sign.at(sign.indexOf(QStringLiteral("--local-user")) + 1), QStringLiteral("FPR1"));
    QCOMPARE(gpgArguments(Operation::Verify, {}, QStringLiteral("/a/in.sig"), QStringLiteral("/a/in")).mid(4),
             (QStringList{QStringLiteral("--verify"), QStringLiteral("--"), QStringLiteral("/a/in.sig"), QStringLiteral("/a/in")}));
    QCOMPARE(gpgArguments(Operation::Verify, {}, QStringLiteral("/a/in.asc"), {}).last(), QStringLiteral("/a/in.asc"));
    const QStringList decrypt = gpgArguments(Operation::Decrypt, {}, QStringLiteral("/a/in.gpg"), QStringLiteral("/a/in"));
    QVERIFY(decrypt.contains(QStringLiteral("--decrypt")));
    QCOMPARE(decrypt.last(), QStringLiteral("/a/in.gpg"));

    QCOMPARE(kleopatraArguments(false, {QStringLiteral("/a/b")}),
             (QStringList{QStringLiteral("--encrypt-sign"), QStringLiteral("--"), QStringLiteral("/a/b")}));
    QCOMPARE(kleopatraArguments(true, {QStringLiteral("/a/b.gpg")}).first(), QStringLiteral("--decrypt-verify"));
    QCOMPARE(aesCryptArguments(true, QStringLiteral("/run/key"), QStringLiteral("/a/b"), QStringLiteral("/a/b.aes")),
             (QStringList{QStringLiteral("-e"),
                          QStringLiteral("-k"),
                          QStringLiteral("/run/key"),
                          QStringLiteral("-o"),
                          QStringLiteral("/a/b.aes"),
                          QStringLiteral("/a/b")}));
}

void FileEncryptionTest::readsLastError()
{
    QCOMPARE(lastError("gpg: encrypted with cv25519 key\ngpg: decryption failed: No secret key\n\n"), QStringLiteral("decryption failed: No secret key"));
    QCOMPARE(lastError("Error: Message has been altered or password is incorrect\n"),
             QStringLiteral("Error: Message has been altered or password is incorrect"));
    QVERIFY(lastError("").isEmpty());
}

void FileEncryptionTest::reportsMissingTools()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeFile(file, "x"));
    qputenv("PATH", dir.path().toLocal8Bit());
    FileEncryptionRunner runner;
    Options options;
    options.password = true;
    const Run gnupg = run(runner, Operation::Encrypt, {file}, options);
    const Run aes = run(runner, Operation::AesEncrypt, {file}, {}, QStringLiteral("secret"));
    QSignalSpy listed(&runner, &FileEncryptionRunner::keysListed);
    runner.listKeys(false);
    QVERIFY(listed.wait());
    qputenv("PATH", m_oldPath);
    QCOMPARE(gnupg.outcomes.size(), 1);
    QVERIFY(!gnupg.outcomes.first().success);
    QCOMPARE(gnupg.outcomes.first().message, missingText(Tool::GnuPG));
    QCOMPARE(aes.outcomes.first().message, missingText(Tool::AesCrypt));
    QCOMPARE(listed.first().at(1).toString(), missingText(Tool::GnuPG));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{QStringLiteral("a.txt")});
}

void FileEncryptionTest::encryptsAndDecryptsWithGnuPG()
{
    if (findExecutable(Tool::GnuPG).isEmpty()) {
        QSKIP("GnuPG is not installed");
    }
    QVERIFY(gpg({QStringLiteral("--pinentry-mode"),
                 QStringLiteral("loopback"),
                 QStringLiteral("--passphrase"),
                 QString(),
                 QStringLiteral("--quick-gen-key"),
                 QStringLiteral("Test User <test@example.org>"),
                 QStringLiteral("default"),
                 QStringLiteral("default"),
                 QStringLiteral("never")}));

    FileEncryptionRunner runner;
    QSignalSpy listed(&runner, &FileEncryptionRunner::keysListed);
    runner.listKeys(false);
    QVERIFY(listed.wait(60000));
    const auto keys = listed.first().first().value<QList<Key>>();
    QVERIFY2(listed.first().at(1).toString().isEmpty(), qPrintable(listed.first().at(1).toString()));
    QCOMPARE(keys.size(), 1);
    QVERIFY(keys.first().canEncrypt);
    QVERIFY(isValid(keys.first()));
    QCOMPARE(keys.first().userIds, QStringList{QStringLiteral("Test User <test@example.org>")});
    m_fingerprint = keys.first().fingerprint;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString plain = dir.filePath(QStringLiteral("notes.txt"));
    const QString other = dir.filePath(QStringLiteral("-dash.txt"));
    QVERIFY(writeFile(plain, "secret notes\n"));
    QVERIFY(writeFile(other, "more\n"));
    Options options;
    options.recipients = {m_fingerprint};
    const Run encrypted = run(runner, Operation::Encrypt, {plain, other}, options);
    QCOMPARE(encrypted.outcomes.size(), 2);
    QVERIFY2(encrypted.outcomes.at(0).success, qPrintable(encrypted.outcomes.at(0).message));
    QVERIFY2(encrypted.outcomes.at(1).success, qPrintable(encrypted.outcomes.at(1).message));
    QCOMPARE(encrypted.outcomes.at(0).output, plain + QStringLiteral(".gpg"));
    QCOMPARE(identifyFile(plain + QStringLiteral(".gpg")), Kind::Encrypted);
    QCOMPARE(readFile(plain), QByteArray("secret notes\n"));

    options.armor = true;
    const Run armoured = run(runner, Operation::Encrypt, {plain}, options);
    QVERIFY(armoured.outcomes.first().success);
    QCOMPARE(armoured.outcomes.first().output, plain + QStringLiteral(".asc"));
    QCOMPARE(identifyFile(plain + QStringLiteral(".asc")), Kind::Encrypted);

    const Run decrypted = run(runner, Operation::Decrypt, {plain + QStringLiteral(".gpg")});
    QVERIFY2(decrypted.outcomes.first().success, qPrintable(decrypted.outcomes.first().message));
    const QString output = decrypted.outcomes.first().output;
    QVERIFY(output != plain);
    QCOMPARE(QFileInfo(output).absolutePath(), dir.path());
    QCOMPARE(readFile(output), QByteArray("secret notes\n"));
    QVERIFY(decrypted.outcomes.first().message.contains(QFileInfo(output).fileName()));
}

void FileEncryptionTest::signsAndVerifiesWithGnuPG()
{
    if (m_fingerprint.isEmpty()) {
        QSKIP("GnuPG is not installed or no key was created");
    }
    FileEncryptionRunner runner;
    QSignalSpy listed(&runner, &FileEncryptionRunner::keysListed);
    runner.listKeys(true);
    QVERIFY(listed.wait(60000));
    const auto secret = listed.first().first().value<QList<Key>>();
    QCOMPARE(secret.size(), 1);
    QVERIFY(secret.first().canSign);
    QVERIFY(!secret.first().unusable);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString data = dir.filePath(QStringLiteral("release.tar"));
    QVERIFY(writeFile(data, "release\n"));
    Options options;
    options.signer = m_fingerprint;
    const Run signedRun = run(runner, Operation::Sign, {data}, options);
    QVERIFY2(signedRun.outcomes.first().success, qPrintable(signedRun.outcomes.first().message));
    const QString signature = data + QStringLiteral(".sig");
    QCOMPARE(signedRun.outcomes.first().output, signature);
    QCOMPARE(identifyFile(signature), Kind::Signature);

    const Run verified = run(runner, Operation::Verify, {signature, data});
    QCOMPARE(verified.outcomes.size(), 2);
    for (const Outcome &outcome : verified.outcomes) {
        QVERIFY2(outcome.success, qPrintable(outcome.message));
        QCOMPARE(outcome.signatures.size(), 1);
        QCOMPARE(outcome.signatures.first().status, Signature::Status::Good);
        QCOMPARE(outcome.signatures.first().trust, Signature::Trust::Ultimate);
        QCOMPARE(outcome.signatures.first().fingerprint, m_fingerprint);
        QVERIFY(outcome.output.isEmpty());
        QCOMPARE(outcome.message, QStringLiteral("Good signature by Test User <test@example.org>"));
    }

    QVERIFY(writeFile(data, "tampered\n"));
    const Run tampered = run(runner, Operation::Verify, {signature});
    QVERIFY(!tampered.outcomes.first().success);
    QCOMPARE(tampered.outcomes.first().signatures.first().status, Signature::Status::Bad);
    QVERIFY(tampered.outcomes.first().message.startsWith(QStringLiteral("Bad signature")));

    QVERIFY(QFile::remove(data));
    const Run missing = run(runner, Operation::Verify, {signature});
    QVERIFY(!missing.outcomes.first().success);
    QVERIFY(missing.outcomes.first().message.contains(QStringLiteral("release.tar")));
}

void FileEncryptionTest::keepsNothingAfterFailure()
{
    if (findExecutable(Tool::GnuPG).isEmpty()) {
        QSKIP("GnuPG is not installed");
    }
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString broken = dir.filePath(QStringLiteral("broken.txt.gpg"));
    QVERIFY(writeFile(broken, QByteArray("\x84\x5e\x03garbage", 10)));
    FileEncryptionRunner runner;
    const Run decrypted = run(runner, Operation::Decrypt, {broken});
    QVERIFY(!decrypted.outcomes.first().success);
    QVERIFY(!decrypted.outcomes.first().message.isEmpty());
    QVERIFY(decrypted.outcomes.first().output.isEmpty());
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{QStringLiteral("broken.txt.gpg")});

    Options options;
    options.recipients = {QStringLiteral("0000000000000000000000000000000000000000")};
    const QString plain = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeFile(plain, "x"));
    const Run encrypted = run(runner, Operation::Encrypt, {plain}, options);
    QVERIFY(!encrypted.outcomes.first().success);
    QVERIFY(!QFileInfo::exists(plain + QStringLiteral(".gpg")));

    const Run unsigned_ = run(runner, Operation::Verify, {plain});
    QVERIFY(!unsigned_.outcomes.first().success);
    QVERIFY(unsigned_.outcomes.first().message.contains(QStringLiteral(".sig")));

    Options none;
    const Run nothing = run(runner, Operation::Encrypt, {plain}, none);
    QVERIFY(!nothing.outcomes.first().success);
}

void FileEncryptionTest::passesAesPasswordsThroughKeyFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString bin = dir.filePath(QStringLiteral("bin"));
    QVERIFY(QDir().mkpath(bin));
    const QString log = dir.filePath(QStringLiteral("log"));
    const QString script = bin + QStringLiteral("/aescrypt");
    // Records the arguments, the key file's mode and contents, then copies the input to the output.
    QVERIFY(writeFile(script,
                      QStringLiteral("#!/bin/sh\n"
                                     "printf '%s\\n' \"$@\" > '%1'\n"
                                     "stat -c %a \"$3\" >> '%1'\n"
                                     "cat \"$3\" >> '%1'\n"
                                     "cp \"$6\" \"$5\"\n")
                          .arg(log)
                          .toLocal8Bit()));
    QVERIFY(QFile::setPermissions(script, QFile::permissions(script) | QFileDevice::ExeOwner));
    qputenv("PATH", QByteArray(bin.toLocal8Bit() + ':' + m_oldPath));
    const QString plain = dir.filePath(QStringLiteral("photo.jpg"));
    QVERIFY(writeFile(plain, "jpeg"));
    FileEncryptionRunner runner;
    const QString password = QStringLiteral("pässword with spaces");
    const Run encrypted = run(runner, Operation::AesEncrypt, {plain}, {}, password);
    qputenv("PATH", m_oldPath);
    QCOMPARE(encrypted.outcomes.size(), 1);
    QVERIFY2(encrypted.outcomes.first().success, qPrintable(encrypted.outcomes.first().message));
    QCOMPARE(encrypted.outcomes.first().output, plain + QStringLiteral(".aes"));
    const QList<QByteArray> lines = readFile(log).split('\n');
    QVERIFY(lines.size() >= 8);
    QCOMPARE(lines.at(0), QByteArray("-e"));
    QCOMPARE(lines.at(1), QByteArray("-k"));
    QCOMPARE(lines.at(4), (plain + QStringLiteral(".aes")).toLocal8Bit());
    QCOMPARE(lines.at(5), plain.toLocal8Bit());
    QCOMPARE(lines.at(6), QByteArray("600"));
    QCOMPARE(lines.at(7), password.toUtf8());
    QVERIFY(!readFile(log).left(readFile(log).indexOf("600")).contains(password.toUtf8()));
    const QString keyFile = QString::fromLocal8Bit(lines.at(2));
    QVERIFY(!QFileInfo::exists(keyFile));
    QVERIFY(!QFileInfo::exists(QFileInfo(keyFile).absolutePath()));

    const Run noPassword = run(runner, Operation::AesDecrypt, {plain + QStringLiteral(".aes")});
    QVERIFY(!noPassword.outcomes.first().success);
}

void FileEncryptionTest::removesFailedAesOutput()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString bin = dir.filePath(QStringLiteral("bin"));
    QVERIFY(QDir().mkpath(bin));
    const QString script = bin + QStringLiteral("/aescrypt");
    QVERIFY(writeFile(script, "#!/bin/sh\nprintf partial > \"$5\"\necho 'Error: Message has been altered or password is incorrect' >&2\nexit 1\n"));
    QVERIFY(QFile::setPermissions(script, QFile::permissions(script) | QFileDevice::ExeOwner));
    const QString encrypted = dir.filePath(QStringLiteral("photo.jpg.aes"));
    QVERIFY(writeFile(encrypted, QByteArray("AES\x02\x00", 5)));
    const QString existing = dir.filePath(QStringLiteral("photo.jpg"));
    QVERIFY(writeFile(existing, "original"));
    qputenv("PATH", QByteArray(bin.toLocal8Bit() + ':' + m_oldPath));
    FileEncryptionRunner runner;
    const Run decrypted = run(runner, Operation::AesDecrypt, {encrypted}, {}, QStringLiteral("wrong"));
    qputenv("PATH", m_oldPath);
    QVERIFY(!decrypted.outcomes.first().success);
    QCOMPARE(decrypted.outcomes.first().message, QStringLiteral("Error: Message has been altered or password is incorrect"));
    QCOMPARE(readFile(existing), QByteArray("original"));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files), (QStringList{QStringLiteral("photo.jpg"), QStringLiteral("photo.jpg.aes")}));
}

QTEST_GUILESS_MAIN(FileEncryptionTest)

#include "fileencryptiontest.moc"
