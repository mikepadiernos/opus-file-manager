// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityreporting.h"

#include "activityusage.h"
#include "appidentity.h"
#include "mounttable.h"
#include "settings.h"

#include <PlasmaActivities/ResourceInstance>

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace
{
ActivityReporting::Reporter &customReporter()
{
    static ActivityReporting::Reporter reporter;
    return reporter;
}

/// Temporary files and open encrypted folders are never reported.
QStringList excludedFolders()
{
    QStringList folders = {QDir::tempPath()};
    QFile mountInfo(QStringLiteral("/proc/self/mountinfo"));
    if (mountInfo.open(QIODevice::ReadOnly)) {
        folders += ActivityUsage::encryptedMountPoints(MountTable::parseMountInfo(mountInfo.readAll()));
    }
    return folders;
}
}

namespace ActivityReporting
{
void reportUsed(const QUrl &url)
{
    if (!Settings::reportActivityUsage()) {
        return;
    }
    const Reporter &custom = customReporter();
    if (!custom && QStandardPaths::isTestModeEnabled()) {
        return;
    }
    const QUrl reportable = ActivityUsage::reportableUrl(url, excludedFolders());
    if (reportable.isEmpty()) {
        return;
    }
    if (custom) {
        custom(reportable);
    } else {
        KActivities::ResourceInstance::notifyAccessed(reportable, AppIdentity::desktopFileName());
    }
}

void setReporter(Reporter reporter)
{
    customReporter() = std::move(reporter);
}
}
