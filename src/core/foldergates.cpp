// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldergates.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace
{
const QLatin1String Group("FolderGates");
const QLatin1String GatesKey("Gates");
const QLatin1String Scheme("pbkdf2-sha256");
constexpr int SaltBytes = 16;
constexpr int KeyBytes = 32;
constexpr int MaximumIterations = 10000000;
/// Bounds the canonical path cache, which views fill as they filter rows.
constexpr int MaximumCachedPaths = 20000;

bool isWithin(const QString &path, const QString &folder)
{
    return path == folder || (path.startsWith(folder) && path.at(folder.size()) == u'/');
}

bool isStrictlyWithin(const QString &path, const QString &folder)
{
    return path.size() > folder.size() && path.startsWith(folder) && path.at(folder.size()) == u'/';
}

QByteArray derive(const QString &password, const QByteArray &salt, int iterations)
{
    const QByteArray data = password.normalized(QString::NormalizationForm_C).toUtf8();
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, data, salt, iterations, KeyBytes);
}

bool equalInConstantTime(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char difference = 0;
    for (qsizetype i = 0; i < a.size(); ++i) {
        difference |= static_cast<unsigned char>(a.at(i)) ^ static_cast<unsigned char>(b.at(i));
    }
    return difference == 0;
}

bool isArchiveScheme(const QString &scheme)
{
    static const QStringList schemes{
        QStringLiteral("zip"),
        QStringLiteral("tar"),
        QStringLiteral("sevenz"),
        QStringLiteral("krarc"),
        QStringLiteral("ar"),
        QStringLiteral("iso"),
        QStringLiteral("archive"),
    };
    return schemes.contains(scheme);
}
}

FolderGates *FolderGates::instance()
{
    static FolderGates *gates = new FolderGates(KSharedConfig::openConfig());
    return gates;
}

FolderGates::FolderGates(KSharedConfigPtr config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
{
    load();
    connect(this, &FolderGates::changed, this, [this] {
        m_canonical.clear();
    });
}

void FolderGates::load()
{
    m_gates.clear();
    const QStringList entries = m_config->group(Group).readEntry(GatesKey, QStringList());
    for (const QString &entry : entries) {
        // Each entry is the path, a tab and the verifier, which has no tabs.
        const qsizetype tab = entry.lastIndexOf(u'\t');
        const QString path = tab > 0 ? QDir::cleanPath(entry.left(tab)) : QString();
        if (path.isEmpty() || !QDir::isAbsolutePath(path) || path == QLatin1String("/")) {
            continue;
        }
        // A damaged verifier still gates its folder; it cannot be unlocked.
        m_gates.insert(path, entry.mid(tab + 1));
    }
}

QString FolderGates::save()
{
    QStringList entries;
    for (auto it = m_gates.cbegin(); it != m_gates.cend(); ++it) {
        entries.append(it.key() + u'\t' + it.value());
    }
    entries.sort();
    KConfigGroup group = m_config->group(Group);
    if (entries.isEmpty()) {
        group.deleteEntry(GatesKey);
    } else {
        group.writeEntry(GatesKey, entries);
    }
    if (!m_config->sync()) {
        return i18nc("@info:status password-gated folders", "The password settings could not be saved.");
    }
    return {};
}

QStringList FolderGates::folders() const
{
    QStringList result = m_gates.keys();
    result.sort();
    return result;
}

bool FolderGates::isGate(const QString &folder) const
{
    return m_gates.contains(folder);
}

bool FolderGates::isGateLocked(const QString &folder) const
{
    return m_gates.contains(folder) && !m_unlocked.contains(folder);
}

bool FolderGates::hasLocked() const
{
    return m_unlocked.size() < m_gates.size();
}

bool FolderGates::hasUnlocked() const
{
    return !m_unlocked.isEmpty();
}

QString FolderGates::resolve(const QUrl &url) const
{
    const QString path = localPath(url);
    if (path.isEmpty()) {
        return {};
    }
    const auto cached = m_canonical.constFind(path);
    if (cached != m_canonical.cend()) {
        return *cached;
    }
    if (m_canonical.size() >= MaximumCachedPaths) {
        m_canonical.clear();
    }
    const QString canonical = canonicalPath(path);
    m_canonical.insert(path, canonical);
    return canonical;
}

bool FolderGates::isLocked(const QUrl &url) const
{
    return !lockedGateFor(url).isEmpty();
}

QString FolderGates::lockedGateFor(const QUrl &url) const
{
    if (!hasLocked()) {
        return {};
    }
    const QString path = resolve(url);
    if (path.isEmpty()) {
        return {};
    }
    QString outermost;
    for (auto it = m_gates.cbegin(); it != m_gates.cend(); ++it) {
        if (!m_unlocked.contains(it.key()) && isWithin(path, it.key()) && (outermost.isEmpty() || it.key().size() < outermost.size())) {
            outermost = it.key();
        }
    }
    return outermost;
}

bool FolderGates::hidesContents(const QUrl &url) const
{
    if (!hasLocked()) {
        return false;
    }
    const QString path = resolve(url);
    if (path.isEmpty()) {
        return false;
    }
    for (auto it = m_gates.cbegin(); it != m_gates.cend(); ++it) {
        if (!m_unlocked.contains(it.key()) && isStrictlyWithin(path, it.key())) {
            return true;
        }
    }
    return false;
}

QStringList FolderGates::lockedWithin(const QUrl &url) const
{
    if (!hasLocked()) {
        return {};
    }
    const QString path = resolve(url);
    if (path.isEmpty()) {
        return {};
    }
    QStringList result;
    for (auto it = m_gates.cbegin(); it != m_gates.cend(); ++it) {
        if (!m_unlocked.contains(it.key()) && (path == QLatin1String("/") || isWithin(it.key(), path))) {
            result.append(it.key());
        }
    }
    result.sort();
    return result;
}

bool FolderGates::touchesLocked(const QUrl &url) const
{
    return isLocked(url) || !lockedWithin(url).isEmpty();
}

QString FolderGates::gateAt(const QUrl &url) const
{
    if (m_gates.isEmpty()) {
        return {};
    }
    const QString path = resolve(url);
    return m_gates.contains(path) ? path : QString();
}

QString FolderGates::protect(const QString &folder, const QString &password)
{
    if (password.isEmpty()) {
        return i18nc("@info:status password-gated folders", "Enter a password.");
    }
    const QFileInfo info(folder);
    if (folder.isEmpty() || !QDir::isAbsolutePath(folder) || !info.isDir()) {
        return i18nc("@info:status password-gated folders", "Only existing local folders can be protected with a password.");
    }
    const QString path = canonicalPath(folder);
    const QString home = canonicalPath(QDir::homePath());
    if (path == QLatin1String("/") || isWithin(home, path)) {
        return i18nc("@info:status password-gated folders", "The root folder, your home folder and the folders that contain it cannot be protected.");
    }
    if (m_gates.contains(path)) {
        return i18nc("@info:status password-gated folders", "This folder is already protected with a password.");
    }
    m_gates.insert(path, makeVerifier(password, m_iterations));
    const QString error = save();
    if (!error.isEmpty()) {
        m_gates.remove(path);
        return error;
    }
    Q_EMIT changed();
    return {};
}

QString FolderGates::errorForVerifier(const QString &folder, const QString &password) const
{
    if (!m_gates.contains(folder)) {
        return i18nc("@info:status password-gated folders", "This folder is not protected with a password.");
    }
    switch (verify(m_gates.value(folder), password)) {
    case Check::Unlocked:
        return {};
    case Check::Wrong:
        return i18nc("@info:status password-gated folders", "The password is incorrect.");
    case Check::Damaged:
        break;
    }
    return i18nc("@info:status password-gated folders", "The stored password for this folder is damaged, so the folder stays locked.");
}

QString FolderGates::changePassword(const QString &folder, const QString &oldPassword, const QString &newPassword)
{
    QString error = errorForVerifier(folder, oldPassword);
    if (!error.isEmpty()) {
        return error;
    }
    if (newPassword.isEmpty()) {
        return i18nc("@info:status password-gated folders", "Enter a new password.");
    }
    const QString previous = m_gates.value(folder);
    m_gates.insert(folder, makeVerifier(newPassword, m_iterations));
    error = save();
    if (!error.isEmpty()) {
        m_gates.insert(folder, previous);
        return error;
    }
    Q_EMIT changed();
    return {};
}

QString FolderGates::removeProtection(const QString &folder, const QString &password)
{
    QString error = errorForVerifier(folder, password);
    if (!error.isEmpty()) {
        return error;
    }
    const QString previous = m_gates.take(folder);
    error = save();
    if (!error.isEmpty()) {
        m_gates.insert(folder, previous);
        return error;
    }
    m_unlocked.remove(folder);
    Q_EMIT changed();
    return {};
}

FolderGates::Check FolderGates::unlock(const QString &folder, const QString &password)
{
    if (!m_gates.contains(folder)) {
        return Check::Damaged;
    }
    const Check check = verify(m_gates.value(folder), password);
    if (check == Check::Unlocked && !m_unlocked.contains(folder)) {
        m_unlocked.insert(folder);
        Q_EMIT changed();
    }
    return check;
}

void FolderGates::lock(const QString &folder)
{
    if (m_unlocked.remove(folder)) {
        Q_EMIT changed();
    }
}

void FolderGates::lockAll()
{
    if (!m_unlocked.isEmpty()) {
        m_unlocked.clear();
        Q_EMIT changed();
    }
}

void FolderGates::relocate(const QString &from, const QString &to)
{
    const QString source = QDir::cleanPath(from);
    const QString destination = canonicalPath(to);
    if (source.isEmpty() || destination.isEmpty() || source == destination) {
        return;
    }
    QHash<QString, QString> moved;
    QSet<QString> unlocked;
    for (auto it = m_gates.begin(); it != m_gates.end();) {
        if (isWithin(it.key(), source)) {
            const QString path = destination + it.key().mid(source.size());
            moved.insert(path, it.value());
            if (m_unlocked.remove(it.key())) {
                unlocked.insert(path);
            }
            it = m_gates.erase(it);
        } else {
            ++it;
        }
    }
    if (moved.isEmpty()) {
        return;
    }
    m_gates.insert(moved);
    m_unlocked.unite(unlocked);
    save();
    Q_EMIT changed();
}

void FolderGates::setIterations(int iterations)
{
    m_iterations = std::clamp(iterations, 1, MaximumIterations);
}

QString FolderGates::makeVerifier(const QString &password, int iterations)
{
    QByteArray salt(SaltBytes, Qt::Uninitialized);
    QRandomGenerator::system()->generate(salt.begin(), salt.end());
    const QByteArray hash = derive(password, salt, iterations);
    return Scheme + u'$' + QString::number(iterations) + u'$' + QString::fromLatin1(salt.toBase64()) + u'$' + QString::fromLatin1(hash.toBase64());
}

FolderGates::Check FolderGates::verify(const QString &verifier, const QString &password)
{
    const QStringList parts = verifier.split(u'$');
    if (parts.size() != 4 || parts.at(0) != Scheme) {
        return Check::Damaged;
    }
    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    const auto decode = [](const QString &text) {
        return QByteArray::fromBase64Encoding(text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    };
    auto salt = decode(parts.at(2));
    auto hash = decode(parts.at(3));
    if (!ok || iterations < 1 || iterations > MaximumIterations || !salt || salt.decoded.isEmpty() || !hash || hash.decoded.size() != KeyBytes) {
        return Check::Damaged;
    }
    return equalInConstantTime(derive(password, salt.decoded, iterations), hash.decoded) ? Check::Unlocked : Check::Wrong;
}

QString FolderGates::localPath(const QUrl &url)
{
    if (!url.isValid()) {
        return {};
    }
    const QString scheme = url.scheme();
    if (url.isLocalFile()) {
        return QDir::cleanPath(url.toLocalFile());
    }
    if (scheme == QLatin1String("admin") || isArchiveScheme(scheme)) {
        const QString path = url.path();
        return QDir::isAbsolutePath(path) ? QDir::cleanPath(path) : QString();
    }
    if (scheme == QLatin1String("filenamesearch")) {
        return localPath(QUrl(QUrlQuery(url).queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded)));
    }
    return {};
}

QString FolderGates::canonicalPath(const QString &path)
{
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) {
        return {};
    }
    QString existing = QDir::cleanPath(path);
    QString rest;
    while (true) {
        const QString canonical = QFileInfo(existing).canonicalFilePath();
        if (!canonical.isEmpty()) {
            return rest.isEmpty() ? canonical : QDir::cleanPath(canonical + u'/' + rest);
        }
        if (existing == QLatin1String("/")) {
            return QDir::cleanPath(path);
        }
        const qsizetype slash = existing.lastIndexOf(u'/');
        const QString name = existing.mid(slash + 1);
        rest = rest.isEmpty() ? name : name + u'/' + rest;
        existing = slash > 0 ? existing.left(slash) : QStringLiteral("/");
    }
}
