// SPDX-License-Identifier: GPL-3.0-or-later

#include "executables.h"

#include <KShell>

#include <QMimeDatabase>

bool Executables::isProgram(const QString &mimeType, bool executableBit)
{
    if (!executableBit) {
        return false;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    return type.isValid()
        && (type.inherits(QStringLiteral("application/x-executable")) || type.inherits(QStringLiteral("application/x-sharedlib"))
            || type.inherits(QStringLiteral("application/x-shellscript")) || type.inherits(QStringLiteral("text/plain")));
}

QString Executables::terminalCommand(const QString &program, bool keepOpen)
{
    const QString quoted = KShell::quoteArg(program);
    if (!keepOpen) {
        return quoted;
    }
    return QStringLiteral("sh -c ") + KShell::quoteArg(quoted + QStringLiteral("; exec \"${SHELL:-sh}\""));
}
