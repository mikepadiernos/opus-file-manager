// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KAboutData>

#include <QString>

/// The application's name, description and identifiers. The desktop entry and
/// AppStream metadata repeat these strings; tests keep them consistent.
namespace AppIdentity
{
/// The component name, also the executable, translation domain and window class.
QString componentName();
/// The desktop entry, AppStream and application icon name.
QString desktopFileName();
/// The D-Bus service name that KDBusService registers.
QString dbusServiceName();
QString homepage();
QString bugAddress();

/// The translated application name.
QString displayName();
/// The translated one-line description shown in About, the desktop entry and AppStream.
QString summary();

KAboutData aboutData(const QString &version);
}
