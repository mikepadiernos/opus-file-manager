// SPDX-License-Identifier: GPL-3.0-or-later

#include "appidentity.h"

#include <KLocalizedString>

QString AppIdentity::componentName()
{
    return QStringLiteral("opus-file-manager");
}

QString AppIdentity::desktopFileName()
{
    return QStringLiteral("org.kde.opusfilemanager");
}

QString AppIdentity::dbusServiceName()
{
    // KDBusService reverses KAboutData's default organization domain, kde.org,
    // and appends the component name.
    return QStringLiteral("org.kde.") + componentName();
}

QString AppIdentity::homepage()
{
    return QStringLiteral("https://github.com/mikepadiernos/plasma-opus");
}

QString AppIdentity::bugAddress()
{
    return QStringLiteral("https://github.com/mikepadiernos/plasma-opus/issues");
}

QString AppIdentity::displayName()
{
    return i18n("Plasma Opus");
}

QString AppIdentity::summary()
{
    return i18n("Dual-pane file manager for KDE Plasma");
}

QIcon AppIdentity::applicationIcon()
{
    return QIcon::fromTheme(desktopFileName(), QIcon(QStringLiteral(":/icons/%1.svg").arg(desktopFileName())));
}

KAboutData AppIdentity::aboutData(const QString &version)
{
    KAboutData about(componentName(), displayName(), version, summary(), KAboutLicense::GPL_V3, i18n("© 2026 Mike Padiernos"));
    about.addAuthor(i18n("Mike Padiernos"), i18n("Author"));
    about.setHomepage(homepage());
    about.setBugAddress(bugAddress().toUtf8());
    about.setDesktopFileName(desktopFileName());
    // About shows the program logo beside the name and version.
    about.setProgramLogo(applicationIcon());
    return about;
}
