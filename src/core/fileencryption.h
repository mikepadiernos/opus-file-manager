// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QProcess;
class QTemporaryDir;

/**
 * Encrypts, decrypts, signs and verifies files through GnuPG, hands files
 * to Kleopatra's own file actions, and encrypts and decrypts AES Crypt
 * (`.aes`) files through an installed `aescrypt` tool. Results are written
 * to new files next to the originals, which are never replaced. Passwords
 * for OpenPGP keys and password-only encryption are asked for by GnuPG's
 * own prompt; Plasma Opus only handles AES Crypt passwords, which it passes
 * through a private key file rather than the command line.
 */
namespace FileEncryption
{
enum class Kind {
    None,
    /// An OpenPGP message encrypted for keys or with a password.
    Encrypted,
    /// An OpenPGP message that is signed but not encrypted, including clear-signed text.
    Signed,
    /// A detached OpenPGP signature of another file.
    Signature,
    /// An AES Crypt file.
    AesCrypt,
};

/// How many leading bytes identify() needs.
constexpr int HeadSize = 512;

/// Whether @p fileName has a suffix that encrypted or signed files use, such as `.gpg` or `.aes`.
bool hasCandidateSuffix(const QString &fileName);
/**
 * Identifies an encrypted or signed file from its name and first bytes.
 * Binary OpenPGP data is recognized only in files with an OpenPGP suffix,
 * since its first byte alone matches many other formats.
 */
Kind identify(const QString &fileName, const QByteArray &head);
/// Identifies a local file with a candidate suffix, reading its first bytes; other files are None.
Kind identifyFile(const QString &path);
/// The translated description of @p kind, or an empty string for None.
QString kindText(Kind kind);

enum class Tool {
    GnuPG,
    Kleopatra,
    AesCrypt,
};
/// The tool's executable in PATH, or an empty string; GnuPG falls back to `gpg2`.
QString findExecutable(Tool tool);
/// The translated message shown when @p tool is missing.
QString missingText(Tool tool);

struct Key {
    QString fingerprint;
    /// The 16-digit key ID.
    QString keyId;
    /// User IDs that are not revoked, primary first.
    QStringList userIds;
    /// GnuPG's validity letter, such as `u` for ultimate or `f` for full.
    QChar validity;
    bool canEncrypt = false;
    bool canSign = false;
    /// Whether the key is revoked, expired, disabled or otherwise invalid, or a secret key is not on this computer.
    bool unusable = false;
};
/// The arguments that list public keys or, when @p secret, the keys this user can sign with.
QStringList listKeysArguments(bool secret);
/// Parses `gpg --with-colons` key listings, skipping malformed records.
QList<Key> parseKeys(const QByteArray &colons);
/// The key's primary user ID and key ID, for lists and menus.
QString keyText(const Key &key);
/// Whether the key is certified as belonging to its user ID, so GnuPG encrypts to it without asking.
bool isValid(const Key &key);

enum class Operation {
    Encrypt,
    Sign,
    Decrypt,
    Verify,
    AesEncrypt,
    AesDecrypt,
};

struct Options {
    /// Fingerprints of the keys to encrypt for.
    QStringList recipients;
    /// Encrypt with a password, which GnuPG asks for, instead of or in addition to keys.
    bool password = false;
    /// The fingerprint of the key to sign with; empty uses GnuPG's default key.
    QString signer;
    /// Write ASCII-armoured `.asc` files instead of binary ones.
    bool armor = false;
};

/// The name of the file @p operation writes for @p fileName, before avoiding existing files.
QString outputName(Operation operation, const QString &fileName, bool armor = false);
/// @p path, or a free name in the same folder when it exists.
QString uniquePath(const QString &path);
/**
 * Finds the signature and signed data for verifying @p path, which may be a
 * detached signature, a signed message or a file with a `.sig` or `.asc`
 * signature beside it. @p data is empty for a signed message.
 * @return false when no signature is found
 */
bool verificationFiles(const QString &path, QString *signature, QString *data);

/// The GnuPG arguments for @p operation; @p output is the signed data for Verify, and empty for a signed message.
QStringList gpgArguments(Operation operation, const Options &options, const QString &input, const QString &output);
/// The arguments that open Kleopatra's own sign-and-encrypt or decrypt-and-verify action for @p paths.
QStringList kleopatraArguments(bool decrypt, const QStringList &paths);
/// The `aescrypt` arguments, which read the password from @p keyFile.
QStringList aesCryptArguments(bool encrypt, const QString &keyFile, const QString &input, const QString &output);

struct Signature {
    enum class Status {
        Unknown,
        Good,
        Bad,
        ExpiredSignature,
        ExpiredKey,
        RevokedKey,
        MissingKey,
        Error,
    };
    enum class Trust {
        Unknown,
        Undefined,
        Never,
        Marginal,
        Full,
        Ultimate,
    };
    Status status = Status::Unknown;
    Trust trust = Trust::Unknown;
    QString keyId;
    QString signer;
    QString fingerprint;
    QDateTime created;
};
/// Parses the signatures reported on GnuPG's `--status-fd` output.
QList<Signature> parseSignatures(const QByteArray &status);
/// A translated one-line description of @p signature.
QString signatureText(const Signature &signature);
/// The last error message a tool wrote to standard error, without GnuPG's prefix.
QString lastError(const QByteArray &standardError);

struct Outcome {
    QString input;
    bool success = false;
    /// The file written, or an empty string.
    QString output;
    QString message;
    QList<Signature> signatures;
};
}

/**
 * Runs one operation on several files in turn, writing each result to a
 * new file and removing partial results of failed or canceled runs.
 */
class FileEncryptionRunner : public QObject
{
    Q_OBJECT

public:
    explicit FileEncryptionRunner(QObject *parent = nullptr);
    ~FileEncryptionRunner() override;

    /// Lists keys in the background, emitting keysListed().
    void listKeys(bool secret);
    /**
     * Starts @p operation on @p paths, emitting fileStarted() and
     * fileFinished() for each and finished() at the end.
     * @param password the AES Crypt password; unused by GnuPG operations
     */
    void start(FileEncryption::Operation operation, const QStringList &paths, const FileEncryption::Options &options = {}, const QString &password = {});
    /// Stops the current file and skips the rest.
    void cancel();
    bool isRunning() const;

Q_SIGNALS:
    /// @p error is empty when listing succeeded.
    void keysListed(const QList<FileEncryption::Key> &keys, const QString &error);
    void fileStarted(int index, const QString &path);
    void fileFinished(int index, const FileEncryption::Outcome &outcome);
    void finished(bool canceled);

private:
    void next();
    void finishFile(int exitCode, bool crashed);
    void clearKeyFile();

    FileEncryption::Operation m_operation = FileEncryption::Operation::Encrypt;
    FileEncryption::Options m_options;
    QStringList m_paths;
    int m_index = -1;
    bool m_canceled = false;
    bool m_running = false;
    QProcess *m_process = nullptr;
    QProcess *m_lister = nullptr;
    QString m_output;
    std::unique_ptr<QTemporaryDir> m_keyDir;
    QString m_keyFile;
};
