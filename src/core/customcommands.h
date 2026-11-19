// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KConfigGroup>

#include <QMap>
#include <QStringList>

namespace CustomCommands
{
struct Command {
    QString name;
    QString program;
    QStringList arguments;
    QMap<QString, QString> defaults;
};

struct Prepared {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QString error;
};

QString validate(const Command &command);
Prepared prepare(const Command &command, const QString &folder, const QStringList &selection);
QList<Command> load(const KConfigGroup &group, QString *error);
bool save(KConfigGroup group, const QList<Command> &commands, QString *error);
}
