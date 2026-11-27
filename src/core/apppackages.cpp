// SPDX-License-Identifier: GPL-3.0-or-later

#include "apppackages.h"
#include "settings.h"

#include <KConfig>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace AppPackages
{
namespace
{
const QString FlatpakTool = QStringLiteral("flatpak");
const QString SnapTool = QStringLiteral("snap");
const QString DiscoverTool = QStringLiteral("plasma-discover");
const QString AppImageUpdateTool = QStringLiteral("appimageupdatetool");

Kind kindForMimeType(const QString &mimeType)
{
    if (mimeType.isEmpty()) {
        return Kind::None;
    }
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    const QString name = type.isValid() ? type.name() : mimeType;
    if (name == QLatin1String("application/vnd.appimage") || name == QLatin1String("application/x-iso9660-appimage")) {
        return Kind::AppImage;
    }
    if (name == QLatin1String("application/vnd.flatpak")) {
        return Kind::FlatpakBundle;
    }
    if (name == QLatin1String("application/vnd.flatpak.ref")) {
        return Kind::FlatpakRef;
    }
    if (name == QLatin1String("application/vnd.flatpak.repo")) {
        return Kind::FlatpakRepo;
    }
    if (name == QLatin1String("application/vnd.snap")) {
        return Kind::SnapFile;
    }
    return Kind::None;
}

/// The first path component of @p path below @p root, or an empty string
/// when @p path is not inside it.
QString childOf(const QString &path, const QString &root)
{
    if (root.isEmpty() || path.isEmpty()) {
        return {};
    }
    const QString cleanRoot = QDir::cleanPath(root);
    for (const QString &candidate : {QDir::cleanPath(path), QFileInfo(path).canonicalFilePath()}) {
        if (candidate.isEmpty() || !candidate.startsWith(cleanRoot + QLatin1Char('/'))) {
            continue;
        }
        return candidate.mid(cleanRoot.size() + 1).section(QLatin1Char('/'), 0, 0);
    }
    return {};
}

/// The installation root, with its canonical path, so a symbolic link such
/// as /snap pointing to /var/lib/snapd/snap still matches.
QStringList roots(const QString &root)
{
    QStringList result{root};
    const QString canonical = QFileInfo(root).canonicalFilePath();
    if (!canonical.isEmpty() && canonical != QDir::cleanPath(root)) {
        result.append(canonical);
    }
    return result;
}

QString childOfAny(const QString &path, const QString &root)
{
    for (const QString &candidate : roots(root)) {
        const QString child = childOf(path, candidate);
        if (!child.isEmpty()) {
            return child;
        }
    }
    return {};
}

QString flatpakApp(const QString &path, const QString &installation)
{
    return installation.isEmpty() ? QString() : childOfAny(path, installation + QStringLiteral("/app"));
}

QString snapName(const QString &path, const Locations &locations)
{
    const QString name = childOfAny(path, locations.snaps);
    // /snap/bin holds launchers, not a snap.
    return name == QLatin1String("bin") || name.startsWith(QLatin1Char('.')) ? QString() : name;
}

bool flatpakInstalled(const QString &installation, const QString &id, bool runtime)
{
    return !installation.isEmpty() && !id.isEmpty() && QFileInfo(installation + (runtime ? QStringLiteral("/runtime/") : QStringLiteral("/app/")) + id).isDir();
}

QString scope(bool systemWide)
{
    return systemWide ? QStringLiteral("--system") : QStringLiteral("--user");
}

bool isInstalledApp(const Package &package)
{
    return package.installed
        && (package.kind == Kind::FlatpakApp || package.kind == Kind::FlatpakRef || package.kind == Kind::SnapApp || package.kind == Kind::SnapFile);
}

bool isFlatpak(Kind kind)
{
    return kind == Kind::FlatpakBundle || kind == Kind::FlatpakRef || kind == Kind::FlatpakRepo || kind == Kind::FlatpakApp;
}

bool isSnap(Kind kind)
{
    return kind == Kind::SnapFile || kind == Kind::SnapApp;
}

/// The identifier libappimage gives an AppImage: the MD5 of its file URL.
QString appImageKey(const QString &appImage)
{
    const QFileInfo info(appImage);
    const QString path = info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
    const QByteArray url = QUrl::fromLocalFile(path).toEncoded();
    return QString::fromLatin1(QCryptographicHash::hash(url, QCryptographicHash::Md5).toHex());
}

QString sanitizedName(const QString &name)
{
    QString result = name.toLower();
    result.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")), QStringLiteral("-"));
    result.remove(QRegularExpression(QStringLiteral("^[-.]+|-+$")));
    return result.isEmpty() ? QStringLiteral("appimage") : result;
}

/// Quotes @p program for a desktop entry's Exec key, as the Desktop Entry
/// Specification describes.
QString quotedExec(const QString &program)
{
    QString escaped;
    for (const QChar c : program) {
        if (c == QLatin1Char('"') || c == QLatin1Char('`') || c == QLatin1Char('$') || c == QLatin1Char('\\')) {
            escaped += QLatin1Char('\\');
        }
        escaped += c;
    }
    escaped.replace(QLatin1Char('%'), QStringLiteral("%%"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}
}

Locations Locations::system()
{
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return {data + QStringLiteral("/flatpak"),
            QStringLiteral("/var/lib/flatpak"),
            QStringLiteral("/snap"),
            data + QStringLiteral("/applications"),
            data + QStringLiteral("/opus-file-manager/appimage-icons")};
}

bool enabled(Kind kind)
{
    if (kind == Kind::AppImage) {
        return Settings::appImageActions();
    }
    if (isFlatpak(kind)) {
        return Settings::flatpakActions();
    }
    if (isSnap(kind)) {
        return Settings::snapActions();
    }
    return false;
}

bool readsEmbeddedDetails(const Package &package)
{
    // Type 1 AppImages are ISO images without an extracting runtime.
    return package.kind == Kind::AppImage && package.appImageType == 2 && Settings::appImageReadDetails();
}

bool isPackage(const QString &path, const QString &mimeType, const Locations &locations)
{
    if (path.isEmpty()) {
        return false;
    }
    const Kind kind = kindForMimeType(mimeType);
    if (kind != Kind::None) {
        return enabled(kind);
    }
    return (enabled(Kind::FlatpakApp) && (!flatpakApp(path, locations.userFlatpak).isEmpty() || !flatpakApp(path, locations.systemFlatpak).isEmpty()))
        || (enabled(Kind::SnapApp) && !snapName(path, locations).isEmpty());
}

Package identify(const QString &path, const QString &mimeType, const Locations &locations)
{
    Package package;
    package.path = path;
    const QFileInfo info(path);
    package.kind = kindForMimeType(mimeType);
    switch (package.kind) {
    case Kind::AppImage:
        package.name = info.completeBaseName();
        package.appImageType = appImageType(path);
        if (package.appImageType == 0 && QMimeDatabase().mimeTypeForName(mimeType).name() == QLatin1String("application/x-iso9660-appimage")) {
            package.appImageType = 1;
        }
        package.integrated = !integrationFiles(path, locations).isEmpty();
        return package;
    case Kind::FlatpakBundle:
        package.name = info.completeBaseName();
        return package;
    case Kind::FlatpakRef: {
        const KConfig config(path, KConfig::SimpleConfig);
        const KConfigGroup group = config.group(QStringLiteral("Flatpak Ref"));
        package.id = group.readEntry("Name");
        package.name = group.readEntry("Title", package.id);
        package.branch = group.readEntry("Branch");
        package.source = group.readEntry("Url");
        package.comment = group.readEntry("Comment");
        package.runtime = group.readEntry("IsRuntime", false);
        if (package.name.isEmpty()) {
            package.name = info.completeBaseName();
        }
        if (flatpakInstalled(locations.userFlatpak, package.id, package.runtime)) {
            package.installed = true;
        } else if (flatpakInstalled(locations.systemFlatpak, package.id, package.runtime)) {
            package.installed = true;
            package.systemWide = true;
        }
        return package;
    }
    case Kind::FlatpakRepo: {
        const KConfig config(path, KConfig::SimpleConfig);
        const KConfigGroup group = config.group(QStringLiteral("Flatpak Repo"));
        package.id = sanitizedName(info.completeBaseName());
        package.name = group.readEntry("Title", info.completeBaseName());
        package.source = group.readEntry("Url");
        package.comment = group.readEntry("Comment");
        return package;
    }
    case Kind::SnapFile:
        // Snap files are named name_revision.snap.
        package.id = info.completeBaseName().section(QLatin1Char('_'), 0, 0);
        package.name = package.id;
        package.installed = !package.id.isEmpty() && QFileInfo(locations.snaps + QLatin1Char('/') + package.id).isDir();
        return package;
    case Kind::FlatpakApp:
    case Kind::SnapApp:
    case Kind::None:
        break;
    }

    for (const bool systemWide : {false, true}) {
        const QString installation = systemWide ? locations.systemFlatpak : locations.userFlatpak;
        const QString id = flatpakApp(path, installation);
        if (id.isEmpty()) {
            continue;
        }
        package.kind = Kind::FlatpakApp;
        package.id = id;
        package.installed = flatpakInstalled(installation, id, false);
        package.systemWide = systemWide;
        const QString desktopFile = installation + QStringLiteral("/exports/share/applications/") + id + QStringLiteral(".desktop");
        package.name = QFileInfo::exists(desktopFile) ? KDesktopFile(desktopFile).readName() : QString();
        if (package.name.isEmpty()) {
            package.name = id;
        }
        return package;
    }
    const QString snap = snapName(path, locations);
    if (!snap.isEmpty()) {
        package.kind = Kind::SnapApp;
        package.id = snap;
        package.name = snap;
        package.installed = QFileInfo(locations.snaps + QLatin1Char('/') + snap).isDir();
    }
    return package;
}

QString kindName(Kind kind)
{
    switch (kind) {
    case Kind::AppImage:
        return i18nc("@item package type", "AppImage");
    case Kind::FlatpakBundle:
        return i18nc("@item package type", "Flatpak bundle");
    case Kind::FlatpakRef:
        return i18nc("@item package type", "Flatpak reference");
    case Kind::FlatpakRepo:
        return i18nc("@item package type", "Flatpak repository");
    case Kind::FlatpakApp:
        return i18nc("@item package type", "Installed Flatpak application");
    case Kind::SnapFile:
        return i18nc("@item package type", "Snap package");
    case Kind::SnapApp:
        return i18nc("@item package type", "Installed snap");
    case Kind::None:
        break;
    }
    return {};
}

QList<Action> actions(const Package &package)
{
    if (!enabled(package.kind)) {
        return {};
    }
    switch (package.kind) {
    case Kind::AppImage:
        if (package.integrated) {
            return {Action::Run, Action::RemoveIntegration, Action::Update};
        }
        if (!Settings::appImageMenuIntegration()) {
            return {Action::Run, Action::Update};
        }
        return {Action::Run, Action::Integrate, Action::Update};
    case Kind::FlatpakBundle:
        return {Action::Install, Action::OpenInDiscover};
    case Kind::FlatpakRef:
        if (!package.installed) {
            return {Action::Install, Action::OpenInDiscover};
        }
        if (package.runtime) {
            return {Action::Update, Action::Uninstall};
        }
        return {Action::Run, Action::Update, Action::Uninstall, Action::OpenInDiscover};
    case Kind::FlatpakRepo:
        return {Action::Install};
    case Kind::FlatpakApp:
    case Kind::SnapApp:
        if (!package.installed) {
            return {};
        }
        return {Action::Run, Action::Update, Action::Uninstall, Action::OpenInDiscover};
    case Kind::SnapFile:
        if (package.installed) {
            return {Action::Run, Action::Install, Action::Uninstall, Action::OpenInDiscover};
        }
        return {Action::Install, Action::OpenInDiscover};
    case Kind::None:
        break;
    }
    return {};
}

bool changesSystem(Action action)
{
    return action != Action::Run && action != Action::OpenInDiscover;
}

QString tool(const Package &package, Action action)
{
    if (action == Action::OpenInDiscover) {
        return DiscoverTool;
    }
    if (isFlatpak(package.kind)) {
        return FlatpakTool;
    }
    if (isSnap(package.kind)) {
        return SnapTool;
    }
    if (package.kind == Kind::AppImage && action == Action::Update) {
        return AppImageUpdateTool;
    }
    return {};
}

Command command(const Package &package, Action action, bool systemWide)
{
    if (package.kind == Kind::None) {
        return {};
    }
    if (action == Action::OpenInDiscover) {
        if (isInstalledApp(package) && !package.id.isEmpty()) {
            const QString scheme = isSnap(package.kind) ? QStringLiteral("snap://") : QStringLiteral("appstream://");
            return {DiscoverTool, {QStringLiteral("--application"), scheme + package.id}};
        }
        return {DiscoverTool, {QStringLiteral("--local-filename"), package.path}};
    }

    if (package.kind == Kind::AppImage) {
        if (action == Action::Run) {
            return {package.path, {}};
        }
        if (action == Action::Update) {
            return {AppImageUpdateTool, {package.path}};
        }
        return {};
    }

    if (isFlatpak(package.kind)) {
        const QString installed = scope(package.systemWide);
        switch (action) {
        case Action::Install:
            if (package.kind == Kind::FlatpakBundle) {
                return {FlatpakTool,
                        {QStringLiteral("install"), scope(systemWide), QStringLiteral("--noninteractive"), QStringLiteral("--bundle"), package.path}};
            }
            if (package.kind == Kind::FlatpakRef) {
                return {FlatpakTool,
                        {QStringLiteral("install"), scope(systemWide), QStringLiteral("--noninteractive"), QStringLiteral("--from"), package.path}};
            }
            if (package.kind == Kind::FlatpakRepo) {
                return {
                    FlatpakTool,
                    {QStringLiteral("remote-add"), scope(systemWide), QStringLiteral("--if-not-exists"), QStringLiteral("--from"), package.id, package.path}};
            }
            return {};
        case Action::Run:
            return package.installed ? Command{FlatpakTool, {QStringLiteral("run"), package.id}} : Command{};
        case Action::Update:
            return package.installed ? Command{FlatpakTool, {QStringLiteral("update"), installed, QStringLiteral("--noninteractive"), package.id}} : Command{};
        case Action::Uninstall:
            return package.installed ? Command{FlatpakTool, {QStringLiteral("uninstall"), installed, QStringLiteral("--noninteractive"), package.id}}
                                     : Command{};
        default:
            return {};
        }
    }

    switch (action) {
    case Action::Install:
        // A local snap file has no store signature; snapd installs it only
        // with --dangerous, and never with classic or devmode confinement here.
        return package.kind == Kind::SnapFile ? Command{SnapTool, {QStringLiteral("install"), QStringLiteral("--dangerous"), package.path}} : Command{};
    case Action::Run:
        return package.installed ? Command{SnapTool, {QStringLiteral("run"), package.id}} : Command{};
    case Action::Update:
        return package.kind == Kind::SnapApp ? Command{SnapTool, {QStringLiteral("refresh"), package.id}} : Command{};
    case Action::Uninstall:
        return package.installed ? Command{SnapTool, {QStringLiteral("remove"), package.id}} : Command{};
    default:
        return {};
    }
}

QList<Command> detailsCommands(const Package &package)
{
    switch (package.kind) {
    case Kind::FlatpakRef:
    case Kind::FlatpakApp: {
        if (!package.installed || package.id.isEmpty()) {
            return {};
        }
        const QString installed = scope(package.systemWide);
        QList<Command> commands{{FlatpakTool, {QStringLiteral("info"), installed, package.id}}};
        if (!package.runtime) {
            commands.append({FlatpakTool, {QStringLiteral("info"), installed, QStringLiteral("--show-permissions"), package.id}});
        }
        return commands;
    }
    case Kind::SnapFile:
        if (!package.installed) {
            return {{SnapTool, {QStringLiteral("info"), package.path}}};
        }
        return {{SnapTool, {QStringLiteral("info"), package.path}}, {SnapTool, {QStringLiteral("connections"), package.id}}};
    case Kind::SnapApp:
        if (!package.installed) {
            return {};
        }
        return {{SnapTool, {QStringLiteral("info"), package.id}}, {SnapTool, {QStringLiteral("connections"), package.id}}};
    default:
        return {};
    }
}

int appImageType(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray header = file.read(11);
    if (header.size() < 11
        || !header.startsWith("\x7f"
                              "ELF")
        || header.at(8) != 'A' || header.at(9) != 'I') {
        return 0;
    }
    const int type = header.at(10);
    return type == 1 || type == 2 ? type : 0;
}

Command extractCommand(const QString &appImage, const QString &pattern)
{
    return {appImage, {QStringLiteral("--appimage-extract"), pattern}};
}

QStringList integrationFiles(const QString &appImage, const Locations &locations)
{
    const QDir applications(locations.applications);
    const QStringList names = applications.entryList({QStringLiteral("appimagekit_%1-*.desktop").arg(appImageKey(appImage))}, QDir::Files, QDir::Name);
    QStringList files;
    for (const QString &name : names) {
        files.append(applications.filePath(name));
    }
    return files;
}

QString writeIntegration(const QString &appImage, const QString &embeddedDesktopFile, const QString &icon, const Locations &locations)
{
    const QString key = appImageKey(appImage);
    QString name = QFileInfo(appImage).completeBaseName();
    QString comment;
    QStringList categories;
    bool terminal = false;
    QString fieldCode;
    if (!embeddedDesktopFile.isEmpty() && QFileInfo::exists(embeddedDesktopFile)) {
        const KDesktopFile desktop(embeddedDesktopFile);
        const KConfigGroup group = desktop.desktopGroup();
        if (!desktop.readName().isEmpty()) {
            name = desktop.readName();
        }
        comment = desktop.readComment();
        categories = group.readXdgListEntry("Categories");
        terminal = group.readEntry("Terminal", false);
        const QString exec = group.readEntry("Exec");
        for (const QString &code : {QStringLiteral("%U"), QStringLiteral("%u"), QStringLiteral("%F"), QStringLiteral("%f")}) {
            if (exec.contains(code)) {
                fieldCode = code;
                break;
            }
        }
    }

    if (!QDir().mkpath(locations.applications)) {
        return i18nc("@info", "Could not create the folder %1.", locations.applications);
    }
    const QString previousError = removeIntegration(appImage, locations);
    if (!previousError.isEmpty()) {
        return previousError;
    }

    QString iconEntry = QStringLiteral("application-x-executable");
    if (!icon.isEmpty() && QFileInfo::exists(icon)) {
        if (!QDir().mkpath(locations.icons)) {
            return i18nc("@info", "Could not create the folder %1.", locations.icons);
        }
        const QMimeType type = QMimeDatabase().mimeTypeForFile(icon);
        const QString suffix = type.preferredSuffix().isEmpty() ? QStringLiteral("png") : type.preferredSuffix();
        const QString target = locations.icons + QStringLiteral("/appimagekit_") + key + QLatin1Char('.') + suffix;
        if (!QFile::copy(QFileInfo(icon).canonicalFilePath(), target)) {
            return i18nc("@info", "Could not copy the icon to %1.", target);
        }
        iconEntry = target;
    }

    const QString file = locations.applications + QStringLiteral("/appimagekit_") + key + QLatin1Char('-') + sanitizedName(name) + QStringLiteral(".desktop");
    KConfig config(file, KConfig::SimpleConfig);
    KConfigGroup group = config.group(QStringLiteral("Desktop Entry"));
    group.writeEntry("Type", "Application");
    group.writeEntry("Name", name);
    if (!comment.isEmpty()) {
        group.writeEntry("Comment", comment);
    }
    QString exec = quotedExec(QFileInfo(appImage).absoluteFilePath());
    if (!fieldCode.isEmpty()) {
        exec += QLatin1Char(' ') + fieldCode;
    }
    group.writeEntry("Exec", exec);
    group.writeEntry("TryExec", QFileInfo(appImage).absoluteFilePath());
    group.writeEntry("Icon", iconEntry);
    if (!categories.isEmpty()) {
        group.writeXdgListEntry("Categories", categories);
    }
    group.writeEntry("Terminal", terminal);
    group.writeEntry("X-AppImage-Path", QFileInfo(appImage).absoluteFilePath());
    if (!config.sync()) {
        return i18nc("@info", "Could not write the menu entry %1.", file);
    }
    return {};
}

QString removeIntegration(const QString &appImage, const Locations &locations)
{
    for (const QString &file : integrationFiles(appImage, locations)) {
        const QString icon = KDesktopFile(file).readIcon();
        if (!locations.icons.isEmpty() && icon.startsWith(QDir::cleanPath(locations.icons) + QLatin1Char('/')) && QFileInfo::exists(icon)
            && !QFile::remove(icon)) {
            return i18nc("@info", "Could not remove the icon %1.", icon);
        }
        if (!QFile::remove(file)) {
            return i18nc("@info", "Could not remove the menu entry %1.", file);
        }
    }
    return {};
}
}
