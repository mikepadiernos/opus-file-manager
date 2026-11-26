// SPDX-License-Identifier: GPL-3.0-or-later

#include "packagetools.h"

#include <KIO/CommandLauncherJob>

#include <QPointer>
#include <QProcess>
#include <QStandardPaths>

bool SystemPackageTools::hasProgram(const QString &program) const
{
    return !QStandardPaths::findExecutable(program).isEmpty();
}

void SystemPackageTools::run(const AppPackages::Command &command, const QString &workingDirectory, QObject *context, std::function<void(const Result &)> done)
{
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    if (!workingDirectory.isEmpty()) {
        process->setWorkingDirectory(workingDirectory);
    }
    QPointer<QObject> guard(context);
    auto finish = [process, guard, done](const Result &result) {
        process->deleteLater();
        if (guard) {
            done(result);
        }
    };
    connect(process, &QProcess::finished, this, [process, finish](int exitCode, QProcess::ExitStatus status) {
        finish({true, status == QProcess::NormalExit ? exitCode : -1, QString::fromLocal8Bit(process->readAll())});
    });
    connect(process, &QProcess::errorOccurred, this, [process, finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finish({false, -1, process->errorString()});
        }
    });
    process->start(command.program, command.arguments);
}

void SystemPackageTools::launch(const AppPackages::Command &command, QObject *context, std::function<void(const QString &)> done)
{
    auto *job = new KIO::CommandLauncherJob(command.program, command.arguments, this);
    QPointer<QObject> guard(context);
    connect(job, &KJob::result, this, [job, guard, done] {
        if (guard) {
            done(job->error() ? job->errorString() : QString());
        }
    });
    job->start();
}
