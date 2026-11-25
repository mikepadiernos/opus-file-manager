// SPDX-License-Identifier: GPL-3.0-or-later

#include "customstylesheet.h"
#include "effectiveappearance.h"

#include "appearancedefaults.h"
#include "settings.h"
#include "stylesheets.h"

#include <KLocalizedString>

#include <QWidget>

namespace
{
int s_parseWarnings = 0;
QtMessageHandler s_previousHandler = nullptr;

void collectParseWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (type == QtWarningMsg && message.startsWith(QLatin1String("Could not parse"))) {
        ++s_parseWarnings;
        return;
    }
    if (s_previousHandler) {
        s_previousHandler(type, context, message);
    }
}
}

QString CustomStyleSheet::error(const QString &sheet)
{
    if (const QString problem = StyleSheets::check(sheet); !problem.isEmpty()) {
        return problem;
    }
    if (sheet.trimmed().isEmpty()) {
        return {};
    }
    // Qt reports stylesheets it cannot parse only as warnings, while
    // polishing a widget that uses them.
    s_parseWarnings = 0;
    s_previousHandler = qInstallMessageHandler(collectParseWarnings);
    {
        QWidget probe;
        probe.setStyleSheet(sheet);
        probe.ensurePolished();
    }
    qInstallMessageHandler(s_previousHandler);
    s_previousHandler = nullptr;
    if (s_parseWarnings > 0) {
        return i18nc("@info", "Qt could not read the stylesheet. Check that each rule has a selector followed by declarations in braces.");
    }
    return {};
}

QString CustomStyleSheet::configured(QString *error)
{
    if (error) {
        error->clear();
    }
    const QString sheet = EffectiveAppearance::customStyleSheet();
    if (!EffectiveAppearance::useCustomStyleSheet() || sheet.trimmed().isEmpty() || AppearanceDefaults::effectsSuspended()) {
        return {};
    }
    if (const QString problem = CustomStyleSheet::error(sheet); !problem.isEmpty()) {
        if (error) {
            *error = problem;
        }
        return {};
    }
    return sheet;
}
