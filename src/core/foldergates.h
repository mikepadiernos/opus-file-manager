// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KSharedConfig>

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class QUrl;

/**
 * Password-gated folders: local folders that Plasma Opus shows only after
 * their password is entered. The gate is a convenience inside Plasma Opus,
 * not encryption or access control; other applications and the filesystem
 * are unaffected, and no file permissions or contents change.
 *
 * Each gate is stored in the FolderGates group of the main configuration as
 * its canonical path with a salted PBKDF2-HMAC-SHA256 verifier; passwords
 * are never stored. Unlocked gates are remembered in memory only, until
 * they are locked again or Plasma Opus quits.
 */
class FolderGates : public QObject
{
    Q_OBJECT

public:
    /// The result of checking a password.
    enum class Check {
        /// The password matches.
        Unlocked,
        /// The password does not match.
        Wrong,
        /// The stored verifier cannot be read, so the gate stays locked.
        Damaged,
    };

    /// The PBKDF2 iteration count for new verifiers.
    static constexpr int DefaultIterations = 600000;

    /// The gates of Plasma Opus's own configuration.
    static FolderGates *instance();

    explicit FolderGates(KSharedConfigPtr config, QObject *parent = nullptr);

    /// The canonical paths of every gate, sorted.
    QStringList folders() const;
    /// Whether the canonical local @p folder is a gate.
    bool isGate(const QString &folder) const;
    /// Whether the gate at the canonical @p folder is locked.
    bool isGateLocked(const QString &folder) const;
    /// Whether any gate is locked.
    bool hasLocked() const;
    /// Whether any gate is unlocked.
    bool hasUnlocked() const;

    /// Whether @p url is a locked gate or lies inside one.
    bool isLocked(const QUrl &url) const;
    /// The outermost locked gate at or above @p url; empty when there is none.
    QString lockedGateFor(const QUrl &url) const;
    /// Whether @p url lies strictly inside a locked gate, so views hide it.
    bool hidesContents(const QUrl &url) const;
    /// The locked gates at or below @p url, such as when it is copied or moved.
    QStringList lockedWithin(const QUrl &url) const;
    /// Whether @p url is a locked gate, inside one or contains one.
    bool touchesLocked(const QUrl &url) const;
    /// The canonical local path of a gate at @p url, or an empty string.
    QString gateAt(const QUrl &url) const;

    /// Protects @p folder with @p password; returns an error message or an empty string.
    QString protect(const QString &folder, const QString &password);
    /// Changes a gate's password; returns an error message or an empty string.
    QString changePassword(const QString &folder, const QString &oldPassword, const QString &newPassword);
    /// Removes a gate after checking its password; returns an error message or an empty string.
    QString removeProtection(const QString &folder, const QString &password);
    /// Unlocks a gate when @p password matches.
    Check unlock(const QString &folder, const QString &password);
    /// Locks a gate again.
    void lock(const QString &folder);
    /// Locks every gate.
    void lockAll();
    /// Moves the gates at or below @p from to @p to, such as after a rename or move in Plasma Opus.
    void relocate(const QString &from, const QString &to);

    /// Sets the PBKDF2 iteration count for new verifiers; for tests.
    void setIterations(int iterations);

    /// A verifier for @p password: pbkdf2-sha256$iterations$salt$hash, in Base64.
    static QString makeVerifier(const QString &password, int iterations = DefaultIterations);
    /// Checks @p password against @p verifier, comparing in constant time.
    static Check verify(const QString &verifier, const QString &password);
    /**
     * The local path that @p url names: file: and admin: URLs, archive URLs
     * by the path of the archive and entry, and filenamesearch: URLs by the
     * folder they search. Empty for other URLs.
     */
    static QString localPath(const QUrl &url);
    /// @p path with symbolic links resolved, through its nearest existing ancestor when it is missing.
    static QString canonicalPath(const QString &path);

Q_SIGNALS:
    /// Gates were added, removed, moved, locked or unlocked.
    void changed();

private:
    QString resolve(const QUrl &url) const;
    QString errorForVerifier(const QString &folder, const QString &password) const;
    void load();
    QString save();

    KSharedConfigPtr m_config;
    /// Canonical path to verifier.
    QHash<QString, QString> m_gates;
    QSet<QString> m_unlocked;
    int m_iterations = DefaultIterations;
    mutable QHash<QString, QString> m_canonical;
};
