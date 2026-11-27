// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileencryption.h"

#include <KFileUtils>
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace FileEncryption
{
namespace
{
const QLatin1String GnupgPrefix("gpg: ");
const QByteArray StatusPrefix("[GNUPG:] ");

QString suffixOf(const QString &fileName)
{
    const qsizetype dot = fileName.lastIndexOf(QLatin1Char('.'));
    return dot <= 0 ? QString() : fileName.mid(dot + 1).toLower();
}

bool isOpenPgpSuffix(const QString &suffix)
{
    return suffix == QLatin1String("gpg") || suffix == QLatin1String("pgp") || suffix == QLatin1String("asc") || suffix == QLatin1String("sig");
}

QString withoutSuffix(const QString &fileName)
{
    return fileName.left(fileName.lastIndexOf(QLatin1Char('.')));
}

// Marks a decrypted copy before its extension, such as `tar.gz`, so it still opens with the same application.
QString markedName(const QString &fileName)
{
    QString extension = QMimeDatabase().suffixForFileName(fileName);
    if (extension.isEmpty() || !fileName.endsWith(QLatin1Char('.') + extension, Qt::CaseInsensitive)) {
        const qsizetype dot = fileName.lastIndexOf(QLatin1Char('.'));
        extension = dot <= 0 ? QString() : fileName.mid(dot + 1);
    }
    qsizetype stem = extension.isEmpty() ? fileName.size() : fileName.size() - extension.size() - 1;
    if (stem <= 0) {
        stem = fileName.size();
    }
    return i18nc("@item decrypted copy of a file, %1 is its name without the extension, %2 the extension with its dot, or nothing",
                 "%1 (decrypted)%2",
                 fileName.left(stem),
                 fileName.mid(stem));
}

// The kind of the first OpenPGP packet in @p data, from its tag.
Kind packetKind(const QByteArray &data)
{
    if (data.isEmpty()) {
        return Kind::None;
    }
    const auto first = static_cast<unsigned char>(data.at(0));
    if (!(first & 0x80)) {
        return Kind::None;
    }
    const int tag = (first & 0x40) ? (first & 0x3f) : ((first >> 2) & 0x0f);
    switch (tag) {
    case 1: // Public-key encrypted session key
    case 3: // Symmetric-key encrypted session key
        return Kind::Encrypted;
    case 2: // Signature
        return Kind::Signature;
    case 4: // One-pass signature
    case 8: // Compressed data, which `gpg --sign` writes
        return Kind::Signed;
    default:
        return Kind::None;
    }
}

// The kind of an ASCII-armoured message, decoding the start of its data.
Kind armoredMessageKind(const QByteArray &head)
{
    const QList<QByteArray> lines = head.split('\n');
    bool inBody = false;
    // The last line may be cut off, so it is only decoded when nothing else is left.
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (!inBody) {
            inBody = line.isEmpty();
            continue;
        }
        if (line.isEmpty() || line.startsWith('-')) {
            break;
        }
        const QByteArray usable = line.left(line.size() - line.size() % 4);
        const Kind kind = packetKind(QByteArray::fromBase64(usable));
        return kind == Kind::None ? Kind::Encrypted : kind;
    }
    return Kind::Encrypted;
}

QString unescapeColons(const QByteArray &field)
{
    QByteArray result;
    result.reserve(field.size());
    for (qsizetype i = 0; i < field.size(); ++i) {
        if (field.at(i) == '\\' && i + 3 < field.size() && field.at(i + 1) == 'x') {
            bool ok = false;
            const int value = field.mid(i + 2, 2).toInt(&ok, 16);
            if (ok) {
                result.append(static_cast<char>(value));
                i += 3;
                continue;
            }
        }
        result.append(field.at(i));
    }
    return QString::fromUtf8(result);
}

QDateTime statusTime(const QByteArray &value)
{
    bool ok = false;
    const qint64 seconds = value.toLongLong(&ok);
    if (ok) {
        return seconds > 0 ? QDateTime::fromSecsSinceEpoch(seconds) : QDateTime();
    }
    QDateTime time = QDateTime::fromString(QString::fromLatin1(value), QStringLiteral("yyyyMMdd'T'HHmmss"));
    time.setTimeZone(QTimeZone::UTC);
    return time;
}
}

bool hasCandidateSuffix(const QString &fileName)
{
    const QString suffix = suffixOf(fileName);
    return isOpenPgpSuffix(suffix) || suffix == QLatin1String("aes");
}

Kind identify(const QString &fileName, const QByteArray &head)
{
    const QString suffix = suffixOf(fileName);
    if (head.isEmpty()) {
        if (suffix == QLatin1String("gpg") || suffix == QLatin1String("pgp")) {
            return Kind::Encrypted;
        }
        if (suffix == QLatin1String("sig")) {
            return Kind::Signature;
        }
        return suffix == QLatin1String("aes") ? Kind::AesCrypt : Kind::None;
    }
    // "AES" followed by the format version, 0 to 3.
    if (head.size() >= 4 && head.startsWith("AES") && static_cast<unsigned char>(head.at(3)) <= 3) {
        return Kind::AesCrypt;
    }
    QByteArray text = head;
    if (text.startsWith("\xef\xbb\xbf")) {
        text.remove(0, 3);
    }
    text = text.trimmed();
    if (text.startsWith("-----BEGIN PGP SIGNED MESSAGE-----")) {
        return Kind::Signed;
    }
    if (text.startsWith("-----BEGIN PGP SIGNATURE-----")) {
        return Kind::Signature;
    }
    if (text.startsWith("-----BEGIN PGP MESSAGE-----")) {
        return armoredMessageKind(text);
    }
    return isOpenPgpSuffix(suffix) ? packetKind(head) : Kind::None;
}

Kind identifyFile(const QString &path)
{
    const QFileInfo info(path);
    if (!hasCandidateSuffix(info.fileName()) || !info.isFile()) {
        return Kind::None;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Kind::None;
    }
    const QByteArray head = file.read(HeadSize);
    return head.isEmpty() ? Kind::None : identify(info.fileName(), head);
}

QString kindText(Kind kind)
{
    switch (kind) {
    case Kind::Encrypted:
        return i18nc("@info:file metadata", "Encrypted with OpenPGP");
    case Kind::Signed:
        return i18nc("@info:file metadata", "Signed with OpenPGP");
    case Kind::Signature:
        return i18nc("@info:file metadata", "OpenPGP signature of another file");
    case Kind::AesCrypt:
        return i18nc("@info:file metadata", "Encrypted with AES Crypt");
    case Kind::None:
        break;
    }
    return {};
}

QString findExecutable(Tool tool)
{
    switch (tool) {
    case Tool::GnuPG: {
        const QString gpg = QStandardPaths::findExecutable(QStringLiteral("gpg"));
        return gpg.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("gpg2")) : gpg;
    }
    case Tool::Kleopatra:
        return QStandardPaths::findExecutable(QStringLiteral("kleopatra"));
    case Tool::AesCrypt:
        return QStandardPaths::findExecutable(QStringLiteral("aescrypt"));
    }
    return {};
}

QString missingText(Tool tool)
{
    switch (tool) {
    case Tool::GnuPG:
        return i18nc("@info", "GnuPG is not installed. Install the gnupg package from your distribution.");
    case Tool::Kleopatra:
        return i18nc("@info", "Kleopatra is not installed. Install it from Discover or your distribution.");
    case Tool::AesCrypt:
        return i18nc("@info", "The aescrypt command was not found. Install AES Crypt's command-line tool from aescrypt.com or your distribution.");
    }
    return {};
}

QStringList listKeysArguments(bool secret)
{
    return {QStringLiteral("--batch"),
            QStringLiteral("--no-tty"),
            QStringLiteral("--with-colons"),
            QStringLiteral("--fixed-list-mode"),
            QStringLiteral("--with-fingerprint"),
            secret ? QStringLiteral("--list-secret-keys") : QStringLiteral("--list-keys")};
}

QList<Key> parseKeys(const QByteArray &colons)
{
    QList<Key> keys;
    bool inKey = false;
    for (const QByteArray &line : colons.split('\n')) {
        const QList<QByteArray> fields = line.trimmed().split(':');
        const QByteArray &type = fields.first();
        if (type == "pub" || type == "sec") {
            inKey = fields.size() > 11 && fields.at(4).size() == 16;
            if (!inKey) {
                continue;
            }
            Key key;
            const char validity = fields.at(1).isEmpty() ? '-' : fields.at(1).at(0);
            key.validity = QChar::fromLatin1(validity);
            key.keyId = QString::fromLatin1(fields.at(4));
            const QByteArray capabilities = fields.at(11);
            key.canEncrypt = capabilities.contains('E');
            key.canSign = capabilities.contains('S');
            // Invalid, expired, revoked or disabled.
            key.unusable = capabilities.contains('D') || QByteArray("ierd").contains(validity);
            // A secret key whose primary part is only a stub or on a missing card.
            if (type == "sec" && fields.size() > 14 && fields.at(14) == "#") {
                key.unusable = true;
            }
            keys.append(key);
        } else if (!inKey) {
            continue;
        } else if (type == "fpr" && fields.size() > 9 && keys.last().fingerprint.isEmpty()) {
            keys.last().fingerprint = QString::fromLatin1(fields.at(9));
        } else if (type == "uid" && fields.size() > 9 && !fields.at(1).startsWith('r')) {
            keys.last().userIds.append(unescapeColons(fields.at(9)));
        } else if (type == "sub" || type == "ssb") {
            // Capabilities of subkeys are already summarized on the primary key.
            continue;
        }
    }
    keys.removeIf([](const Key &key) {
        return key.fingerprint.isEmpty();
    });
    return keys;
}

QString keyText(const Key &key)
{
    const QString user = key.userIds.isEmpty() ? i18nc("@item a key without a user ID", "No user ID") : key.userIds.first();
    return i18nc("@item key user ID and key ID", "%1 (%2)", user, key.keyId);
}

bool isValid(const Key &key)
{
    return !key.unusable && (key.validity == QLatin1Char('f') || key.validity == QLatin1Char('u'));
}

QString outputName(Operation operation, const QString &fileName, const Options &options)
{
    const QString suffix = suffixOf(fileName);
    const auto stripped = [&fileName, &options](bool strip) {
        const QString base = strip ? withoutSuffix(fileName) : QString();
        if (options.markDecrypted) {
            return markedName(base.isEmpty() ? fileName : base);
        }
        return base.isEmpty() ? fileName + QStringLiteral(".out") : base;
    };
    switch (operation) {
    case Operation::Encrypt:
        return fileName + (options.armor ? QStringLiteral(".asc") : options.pgpSuffix ? QStringLiteral(".pgp") : QStringLiteral(".gpg"));
    case Operation::Sign:
        return fileName + (options.armor ? QStringLiteral(".asc") : QStringLiteral(".sig"));
    case Operation::Decrypt:
        return stripped(suffix == QLatin1String("gpg") || suffix == QLatin1String("pgp") || suffix == QLatin1String("asc"));
    case Operation::AesEncrypt:
        return fileName + QStringLiteral(".aes");
    case Operation::AesDecrypt:
        return stripped(suffix == QLatin1String("aes"));
    case Operation::Verify:
        break;
    }
    return {};
}

QString uniquePath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        return path;
    }
    const QString folder = info.absolutePath();
    return QDir(folder).filePath(KFileUtils::suggestName(QUrl::fromLocalFile(folder), info.fileName()));
}

bool verificationFiles(const QString &path, QString *signature, QString *data)
{
    switch (identifyFile(path)) {
    case Kind::Signature:
        *signature = path;
        *data = withoutSuffix(path);
        return true;
    case Kind::Signed:
        *signature = path;
        data->clear();
        return true;
    default:
        break;
    }
    for (const QLatin1String suffix : {QLatin1String(".sig"), QLatin1String(".asc")}) {
        const QString candidate = path + suffix;
        if (identifyFile(candidate) == Kind::Signature) {
            *signature = candidate;
            *data = path;
            return true;
        }
    }
    return false;
}

QStringList gpgArguments(Operation operation, const Options &options, const QString &input, const QString &output)
{
    QStringList arguments{QStringLiteral("--batch"), QStringLiteral("--no-tty"), QStringLiteral("--status-fd"), QStringLiteral("1")};
    switch (operation) {
    case Operation::Encrypt:
        if (options.armor) {
            arguments.append(QStringLiteral("--armor"));
        }
        if (!options.recipients.isEmpty()) {
            arguments.append(QStringLiteral("--encrypt"));
            for (const QString &recipient : options.recipients) {
                arguments << QStringLiteral("--recipient") << recipient;
            }
        }
        if (options.password) {
            arguments.append(QStringLiteral("--symmetric"));
        }
        arguments << QStringLiteral("--output") << output << QStringLiteral("--") << input;
        return arguments;
    case Operation::Sign:
        arguments.append(QStringLiteral("--detach-sign"));
        if (options.armor) {
            arguments.append(QStringLiteral("--armor"));
        }
        if (!options.signer.isEmpty()) {
            arguments << QStringLiteral("--local-user") << options.signer;
        }
        arguments << QStringLiteral("--output") << output << QStringLiteral("--") << input;
        return arguments;
    case Operation::Decrypt:
        arguments << QStringLiteral("--output") << output << QStringLiteral("--decrypt") << QStringLiteral("--") << input;
        return arguments;
    case Operation::Verify:
        arguments << QStringLiteral("--verify") << QStringLiteral("--") << input;
        if (!output.isEmpty()) {
            arguments.append(output);
        }
        return arguments;
    case Operation::AesEncrypt:
    case Operation::AesDecrypt:
        break;
    }
    return {};
}

QStringList kleopatraArguments(bool decrypt, const QStringList &paths)
{
    // The same commands as Kleopatra's own service menus.
    return QStringList{decrypt ? QStringLiteral("--decrypt-verify") : QStringLiteral("--encrypt-sign"), QStringLiteral("--")} + paths;
}

QStringList aesCryptArguments(bool encrypt, const QString &keyFile, const QString &input, const QString &output)
{
    return {encrypt ? QStringLiteral("-e") : QStringLiteral("-d"), QStringLiteral("-k"), keyFile, QStringLiteral("-o"), output, input};
}

QList<Signature> parseSignatures(const QByteArray &status)
{
    QList<Signature> signatures;
    bool open = false;
    const auto begin = [&] {
        signatures.append(Signature());
        open = true;
    };
    for (const QByteArray &rawLine : status.split('\n')) {
        const QByteArray line = rawLine.trimmed();
        if (!line.startsWith(StatusPrefix)) {
            continue;
        }
        const QByteArray rest = line.mid(StatusPrefix.size());
        const QList<QByteArray> fields = rest.split(' ');
        const QByteArray &keyword = fields.first();
        if (keyword == "NEWSIG") {
            begin();
            continue;
        }
        static const QList<QPair<QByteArray, Signature::Status>> results{
            {"GOODSIG", Signature::Status::Good},
            {"BADSIG", Signature::Status::Bad},
            {"EXPSIG", Signature::Status::ExpiredSignature},
            {"EXPKEYSIG", Signature::Status::ExpiredKey},
            {"REVKEYSIG", Signature::Status::RevokedKey},
        };
        for (const auto &[name, result] : results) {
            if (keyword != name || fields.size() < 2) {
                continue;
            }
            if (!open || signatures.last().status != Signature::Status::Unknown) {
                begin();
            }
            Signature &signature = signatures.last();
            signature.status = result;
            signature.keyId = QString::fromLatin1(fields.at(1));
            const qsizetype nameStart = rest.indexOf(' ', keyword.size() + 1);
            if (nameStart > 0) {
                signature.signer = QString::fromUtf8(QByteArray::fromPercentEncoding(rest.mid(nameStart + 1)));
            }
        }
        if (keyword == "ERRSIG" && fields.size() > 6) {
            if (!open || signatures.last().status != Signature::Status::Unknown) {
                begin();
            }
            Signature &signature = signatures.last();
            signature.keyId = QString::fromLatin1(fields.at(1));
            signature.status = fields.at(6) == "9" ? Signature::Status::MissingKey : Signature::Status::Error;
            signature.created = statusTime(fields.at(5));
            if (fields.size() > 7 && fields.at(7) != "-") {
                signature.fingerprint = QString::fromLatin1(fields.at(7));
            }
        } else if (keyword == "VALIDSIG" && open && fields.size() > 3) {
            Signature &signature = signatures.last();
            signature.fingerprint = QString::fromLatin1(fields.size() > 10 ? fields.at(10) : fields.at(1));
            signature.created = statusTime(fields.at(3));
        } else if (keyword.startsWith("TRUST_") && open) {
            static const QList<QPair<QByteArray, Signature::Trust>> trusts{
                {"TRUST_UNDEFINED", Signature::Trust::Undefined},
                {"TRUST_NEVER", Signature::Trust::Never},
                {"TRUST_MARGINAL", Signature::Trust::Marginal},
                {"TRUST_FULLY", Signature::Trust::Full},
                {"TRUST_ULTIMATE", Signature::Trust::Ultimate},
            };
            for (const auto &[name, trust] : trusts) {
                if (keyword == name) {
                    signatures.last().trust = trust;
                }
            }
        }
    }
    signatures.removeIf([](const Signature &signature) {
        return signature.status == Signature::Status::Unknown;
    });
    return signatures;
}

QString signatureText(const Signature &signature)
{
    const QString signer = signature.signer.isEmpty() ? signature.keyId : signature.signer;
    switch (signature.status) {
    case Signature::Status::Good:
        switch (signature.trust) {
        case Signature::Trust::Full:
        case Signature::Trust::Ultimate:
            return i18nc("@info %1 is a key's user ID", "Good signature by %1", signer);
        case Signature::Trust::Never:
            return i18nc("@info %1 is a key's user ID", "Good signature by %1, but the key is marked as not trusted", signer);
        default:
            return i18nc("@info %1 is a key's user ID", "Good signature by %1, but the key is not certified as theirs", signer);
        }
    case Signature::Status::Bad:
        return i18nc("@info %1 is a key's user ID", "Bad signature by %1: the file or the signature has been changed", signer);
    case Signature::Status::ExpiredSignature:
        return i18nc("@info %1 is a key's user ID", "Expired signature by %1", signer);
    case Signature::Status::ExpiredKey:
        return i18nc("@info %1 is a key's user ID", "Signature by %1, whose key has expired", signer);
    case Signature::Status::RevokedKey:
        return i18nc("@info %1 is a key's user ID", "Signature by %1, whose key has been revoked", signer);
    case Signature::Status::MissingKey:
        return i18nc("@info %1 is a key ID", "Signature by unknown key %1; import the signer's public key to check it", signer);
    case Signature::Status::Error:
    case Signature::Status::Unknown:
        break;
    }
    return i18nc("@info %1 is a key ID", "The signature by key %1 could not be checked", signer);
}

QString lastError(const QByteArray &standardError)
{
    const QStringList lines = QString::fromLocal8Bit(standardError).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (auto line = lines.crbegin(); line != lines.crend(); ++line) {
        QString text = line->trimmed();
        if (text.isEmpty()) {
            continue;
        }
        if (text.startsWith(GnupgPrefix)) {
            text.remove(0, GnupgPrefix.size());
        }
        return text;
    }
    return {};
}
}

using namespace FileEncryption;

FileEncryptionRunner::FileEncryptionRunner(QObject *parent)
    : QObject(parent)
{
}

FileEncryptionRunner::~FileEncryptionRunner()
{
    if (m_lister) {
        m_lister->disconnect(this);
        m_lister->kill();
        m_lister->waitForFinished(1000);
    }
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1000);
        if (!m_output.isEmpty()) {
            QFile::remove(m_output);
        }
    }
    clearKeyFile();
}

void FileEncryptionRunner::listKeys(bool secret)
{
    const QString gpg = findExecutable(Tool::GnuPG);
    if (gpg.isEmpty()) {
        QTimer::singleShot(0, this, [this] {
            Q_EMIT keysListed({}, missingText(Tool::GnuPG));
        });
        return;
    }
    if (m_lister) {
        m_lister->disconnect(this);
        m_lister->kill();
        m_lister->deleteLater();
    }
    auto *lister = new QProcess(this);
    m_lister = lister;
    lister->setStandardInputFile(QProcess::nullDevice());
    connect(lister, &QProcess::finished, this, [this, lister](int exitCode, QProcess::ExitStatus status) {
        m_lister = nullptr;
        lister->deleteLater();
        const QList<Key> keys = parseKeys(lister->readAllStandardOutput());
        QString error;
        if (status != QProcess::NormalExit || (exitCode != 0 && keys.isEmpty())) {
            error = lastError(lister->readAllStandardError());
            if (error.isEmpty()) {
                error = i18nc("@info", "GnuPG could not list keys.");
            }
        }
        Q_EMIT keysListed(keys, error);
    });
    connect(lister, &QProcess::errorOccurred, this, [this, lister](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        m_lister = nullptr;
        lister->deleteLater();
        Q_EMIT keysListed({}, i18nc("@info", "GnuPG could not be started."));
    });
    lister->start(gpg, listKeysArguments(secret));
}

void FileEncryptionRunner::start(Operation operation, const QStringList &paths, const Options &options, const QString &password)
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_operation = operation;
    m_options = options;
    m_paths = paths;
    m_index = -1;
    m_canceled = false;
    clearKeyFile();
    if ((operation == Operation::AesEncrypt || operation == Operation::AesDecrypt) && !password.isEmpty()) {
        // A key file in a private folder keeps the password out of the command line, where other users could read it.
        const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        m_keyDir =
            std::make_unique<QTemporaryDir>(QDir(runtime.isEmpty() ? QDir::tempPath() : runtime).filePath(QStringLiteral("opus-file-manager-aes-XXXXXX")));
        QFile key(m_keyDir->filePath(QStringLiteral("password")));
        if (m_keyDir->isValid() && key.open(QIODevice::WriteOnly | QIODevice::NewOnly, QFileDevice::ReadOwner | QFileDevice::WriteOwner)
            && key.write(password.toUtf8()) == password.toUtf8().size()) {
            m_keyFile = key.fileName();
        } else {
            m_keyDir.reset();
        }
    }
    QMetaObject::invokeMethod(this, &FileEncryptionRunner::next, Qt::QueuedConnection);
}

void FileEncryptionRunner::cancel()
{
    m_canceled = true;
    if (m_process) {
        m_process->kill();
    }
}

bool FileEncryptionRunner::isRunning() const
{
    return m_running;
}

void FileEncryptionRunner::next()
{
    ++m_index;
    if (m_canceled || m_index >= m_paths.size()) {
        const bool canceled = m_canceled;
        m_paths.clear();
        m_index = -1;
        m_running = false;
        clearKeyFile();
        Q_EMIT finished(canceled);
        return;
    }
    const QString path = m_paths.at(m_index);
    Q_EMIT fileStarted(m_index, path);
    const auto fail = [this, &path](const QString &message) {
        Outcome outcome;
        outcome.input = path;
        outcome.message = message;
        Q_EMIT fileFinished(m_index, outcome);
        QMetaObject::invokeMethod(this, &FileEncryptionRunner::next, Qt::QueuedConnection);
    };

    const bool aes = m_operation == Operation::AesEncrypt || m_operation == Operation::AesDecrypt;
    const Tool tool = aes ? Tool::AesCrypt : Tool::GnuPG;
    const QString program = findExecutable(tool);
    if (program.isEmpty()) {
        fail(missingText(tool));
        return;
    }
    if (aes && m_keyFile.isEmpty()) {
        fail(i18nc("@info", "No password was given, or it could not be passed to AES Crypt securely."));
        return;
    }
    if (m_operation == Operation::Encrypt && m_options.recipients.isEmpty() && !m_options.password) {
        fail(i18nc("@info", "Choose keys to encrypt for, or encrypt with a password."));
        return;
    }

    QStringList arguments;
    m_output.clear();
    if (m_operation == Operation::Verify) {
        QString signature;
        QString data;
        if (!verificationFiles(path, &signature, &data)) {
            fail(i18nc("@info", "No OpenPGP signature was found for this file. Select the file's .sig or .asc signature, or a signed file."));
            return;
        }
        if (!data.isEmpty() && !QFileInfo::exists(data)) {
            fail(i18nc("@info %1 is a file name", "The signed file %1 was not found next to its signature.", QFileInfo(data).fileName()));
            return;
        }
        arguments = gpgArguments(m_operation, m_options, signature, data);
    } else {
        const QFileInfo info(path);
        m_output = uniquePath(info.dir().filePath(outputName(m_operation, info.fileName(), m_options)));
        arguments =
            aes ? aesCryptArguments(m_operation == Operation::AesEncrypt, m_keyFile, path, m_output) : gpgArguments(m_operation, m_options, path, m_output);
    }

    m_process = new QProcess(this);
    m_process->setStandardInputFile(QProcess::nullDevice());
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        finishFile(exitCode, status != QProcess::NormalExit);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finishFile(-1, true);
        }
    });
    m_process->start(program, arguments);
}

void FileEncryptionRunner::finishFile(int exitCode, bool crashed)
{
    QProcess *process = m_process;
    if (!process) {
        return;
    }
    m_process = nullptr;
    process->disconnect(this);
    process->deleteLater();
    const QByteArray status = process->readAllStandardOutput();
    const QByteArray errors = process->readAllStandardError();
    const bool failedToStart = process->error() == QProcess::FailedToStart;

    Outcome outcome;
    outcome.input = m_paths.value(m_index);
    const bool aes = m_operation == Operation::AesEncrypt || m_operation == Operation::AesDecrypt;
    if (!aes) {
        outcome.signatures = parseSignatures(status);
    }
    const bool written = !m_output.isEmpty() && QFileInfo::exists(m_output);
    switch (m_operation) {
    case Operation::Verify:
        outcome.success =
            !crashed && !outcome.signatures.isEmpty() && std::all_of(outcome.signatures.cbegin(), outcome.signatures.cend(), [](const Signature &signature) {
                return signature.status == Signature::Status::Good;
            });
        break;
    case Operation::Decrypt:
        // GnuPG also exits with an error when an embedded signature cannot be checked; the signatures report that.
        outcome.success = !crashed && written
            && (exitCode == 0 || status.contains("[GNUPG:] DECRYPTION_OKAY")
                || (status.contains("[GNUPG:] PLAINTEXT") && !status.contains("[GNUPG:] DECRYPTION_FAILED")));
        break;
    default:
        outcome.success = !crashed && exitCode == 0 && written;
        break;
    }

    if (outcome.success) {
        outcome.output = m_output;
        const QString name = QFileInfo(m_output).fileName();
        switch (m_operation) {
        case Operation::Encrypt:
        case Operation::AesEncrypt:
            outcome.message = i18nc("@info %1 is a file name", "Encrypted to %1", name);
            break;
        case Operation::Sign:
            outcome.message = i18nc("@info %1 is a file name", "Signature saved as %1", name);
            break;
        case Operation::Decrypt:
        case Operation::AesDecrypt:
            outcome.message = i18nc("@info %1 is a file name", "Decrypted to %1", name);
            break;
        case Operation::Verify:
            break;
        }
    } else {
        if (written) {
            QFile::remove(m_output);
        }
        const QString program = QFileInfo(process->program()).fileName();
        if (m_canceled && crashed) {
            outcome.message = i18nc("@info", "Canceled");
        } else if (failedToStart) {
            outcome.message = i18nc("@info %1 is a command", "%1 could not be started.", program);
        } else if (crashed) {
            outcome.message = i18nc("@info %1 is a command", "%1 stopped unexpectedly.", program);
        } else if (m_operation != Operation::Verify || outcome.signatures.isEmpty()) {
            outcome.message = lastError(errors);
            if (outcome.message.isEmpty()) {
                outcome.message = m_operation == Operation::Verify ? i18nc("@info", "No valid signature was found.")
                                                                   : i18nc("@info %1 is a command", "%1 failed with exit code %2.", program, exitCode);
            }
        }
    }
    if (outcome.message.isEmpty()) {
        QStringList texts;
        for (const Signature &signature : std::as_const(outcome.signatures)) {
            texts.append(signatureText(signature));
        }
        outcome.message = texts.join(QLatin1Char('\n'));
    }
    m_output.clear();
    Q_EMIT fileFinished(m_index, outcome);
    next();
}

void FileEncryptionRunner::clearKeyFile()
{
    if (!m_keyFile.isEmpty()) {
        QFile key(m_keyFile);
        if (key.open(QIODevice::ReadWrite)) {
            key.write(QByteArray(key.size(), '\0'));
            key.flush();
        }
        m_keyFile.clear();
    }
    m_keyDir.reset();
}
