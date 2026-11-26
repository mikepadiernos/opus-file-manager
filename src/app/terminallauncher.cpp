// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminallauncher.h"

#include "fileoperations.h"

#include <KIO/CommandLauncherJob>
#include <KLocalizedString>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDeadlineTimer>
#include <QTimer>
#include <QUrl>
#include <QWidget>

using TerminalProviders::Provider;

namespace
{
enum class Attempt {
    Done,
    Retry,
};

bool isNotReady(const QDBusError &error)
{
    switch (error.type()) {
    case QDBusError::ServiceUnknown:
    case QDBusError::UnknownObject:
    case QDBusError::UnknownInterface:
    case QDBusError::UnknownMethod:
        return true;
    default:
        return false;
    }
}

// Yakuake registers its service before its session object, so a Yakuake that
// is still starting can answer that the object is unknown.
Attempt addYakuakeSession(const QString &directory, bool retryWhenNotReady, const TerminalLauncher::Report &report)
{
    QDBusInterface sessions(TerminalProviders::yakuakeService(), QStringLiteral("/yakuake/sessions"), QStringLiteral("org.kde.yakuake"));
    sessions.setTimeout(5000);
    const QDBusReply<int> session = sessions.call(QStringLiteral("addSession"));
    if (!session.isValid()) {
        if (retryWhenNotReady && isNotReady(session.error())) {
            return Attempt::Retry;
        }
        report(i18nc("@info:status %1 is the reason", "Could not open a Yakuake session: %1", session.error().message()));
        return Attempt::Done;
    }
    int terminal = -1;
    const QDBusReply<QString> terminals = sessions.call(QStringLiteral("terminalIdsForSessionId"), session.value());
    if (terminals.isValid()) {
        terminal = TerminalProviders::firstYakuakeTerminalId(terminals.value());
    }
    if (terminal < 0) {
        const QDBusReply<int> active = sessions.call(QStringLiteral("activeTerminalId"));
        terminal = active.isValid() ? active.value() : -1;
    }
    if (terminal < 0) {
        report(i18nc("@info:status", "Yakuake opened a session but did not report its terminal, so it starts in Yakuake's default folder."));
        return Attempt::Done;
    }
    const QDBusReply<void> ran = sessions.call(QStringLiteral("runCommandInTerminal"), terminal, TerminalProviders::yakuakeChangeDirectoryCommand(directory));
    if (!ran.isValid()) {
        report(i18nc("@info:status %1 is the reason", "Yakuake opened a session but could not change its folder: %1", ran.error().message()));
        return Attempt::Done;
    }
    report(i18nc("@info:status %1 is a folder", "Opened a Yakuake session in %1. Use Yakuake's shortcut to show it if it is hidden.", directory));
    return Attempt::Done;
}

void openYakuake(const QString &directory, QObject *context, const TerminalLauncher::Report &report)
{
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus) {
        report(i18nc("@info:status", "Yakuake cannot be reached because the D-Bus session bus is unavailable."));
        return;
    }
    if (bus->isServiceRegistered(TerminalProviders::yakuakeService())) {
        addYakuakeSession(directory, false, report);
        return;
    }
    auto *poll = new QTimer(context);
    auto finish = [poll] {
        poll->stop();
        poll->deleteLater();
    };
    const QDeadlineTimer deadline(TerminalLauncher::yakuakeStartTimeoutMs);
    poll->setInterval(250);
    QObject::connect(poll, &QTimer::timeout, poll, [directory, report, deadline, finish] {
        const bool lastAttempt = deadline.hasExpired();
        QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
        if (interface && interface->isServiceRegistered(TerminalProviders::yakuakeService())) {
            if (addYakuakeSession(directory, !lastAttempt, report) == Attempt::Done) {
                finish();
            }
            return;
        }
        if (lastAttempt) {
            report(i18nc("@info:status %1 is a number of seconds", "Yakuake did not start within %1 seconds.", TerminalLauncher::yakuakeStartTimeoutMs / 1000));
            finish();
        }
    });
    auto *job = new KIO::CommandLauncherJob(TerminalProviders::executable(Provider::Yakuake), {}, context);
    job->setDesktopName(TerminalProviders::desktopName(Provider::Yakuake));
    QObject::connect(job, &KJob::result, poll, [report, finish](KJob *finished) {
        if (finished->error()) {
            report(i18nc("@info:status %1 is the reason", "Could not start Yakuake: %1", finished->errorString()));
            finish();
        }
    });
    poll->start();
    job->start();
}
}

void TerminalLauncher::open(Provider provider, const QUrl &folder, QWidget *window, const Report &report)
{
    const QString directory = TerminalProviders::localDirectory(folder);
    if (directory.isEmpty()) {
        report(i18nc("@info:status", "Terminals can only start in an existing local folder."));
        return;
    }
    if (provider == Provider::SystemDefault) {
        FileOperations::openTerminal(QUrl::fromLocalFile(directory), window);
        return;
    }
    const QString unavailable = TerminalProviders::unavailableReason(provider);
    if (!unavailable.isEmpty()) {
        report(unavailable);
        return;
    }
    QObject *context = window ? static_cast<QObject *>(window) : QCoreApplication::instance();
    if (provider == Provider::Yakuake) {
        openYakuake(directory, context, report);
        return;
    }
    auto *job = new KIO::CommandLauncherJob(TerminalProviders::executable(provider), TerminalProviders::launchArguments(provider, directory), context);
    job->setDesktopName(TerminalProviders::desktopName(provider));
    job->setWorkingDirectory(directory);
    QObject::connect(job, &KJob::result, context, [provider, report](KJob *finished) {
        if (finished->error()) {
            report(i18nc("@info:status %1 is a terminal application, %2 the reason",
                         "Could not start %1: %2",
                         TerminalProviders::name(provider),
                         finished->errorString()));
        }
    });
    job->start();
}
