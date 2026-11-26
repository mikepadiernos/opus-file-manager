// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "runnermatches.h"

#include <QDBusArgument>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <optional>

/// One match as KRunner's org.kde.krunner1 interface returns it.
struct RemoteMatch {
    QString id;
    QString text;
    QString iconName;
    int categoryRelevance = 0;
    double relevance = 0;
    QVariantMap properties;
};
using RemoteMatches = QList<RemoteMatch>;

/// An extra action KRunner shows beside a match.
struct RemoteAction {
    QString id;
    QString text;
    QString iconName;
};
using RemoteActions = QList<RemoteAction>;

Q_DECLARE_METATYPE(RemoteMatch)
Q_DECLARE_METATYPE(RemoteAction)

QDBusArgument &operator<<(QDBusArgument &argument, const RemoteMatch &match);
const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteMatch &match);
QDBusArgument &operator<<(QDBusArgument &argument, const RemoteAction &action);
const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteAction &action);

/// Answers KRunner with saved Plasma Opus workspaces and Favorites, and
/// opens the chosen one in Plasma Opus. It reads the Plasma Opus settings
/// afresh for each KRunner session, never writes them and never creates
/// files or runs anything but Plasma Opus.
class OpusRunner : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.krunner1")

public:
    /// Starts Plasma Opus with @p arguments, passing on @p activationToken.
    using Launcher = std::function<void(const QStringList &arguments, const QString &activationToken)>;

    explicit OpusRunner(QObject *parent = nullptr);

    static void registerTypes();

    void setLauncher(Launcher launcher);

public Q_SLOTS:
    RemoteActions Actions();
    RemoteMatches Match(const QString &query);
    void Run(const QString &matchId, const QString &actionId);
    void Teardown();
    void SetActivationToken(const QString &token);

private:
    const QList<RunnerMatches::Target> &targets();

    std::optional<QList<RunnerMatches::Target>> m_targets;
    Launcher m_launcher;
    QString m_activationToken;
};
