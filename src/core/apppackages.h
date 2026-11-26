// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/// AppImages, Flatpak and Snap packages: identifying them from a file or an
/// installed application's folder, and the package-tool command lines that
/// run, install, update and remove them.
namespace AppPackages
{
enum class Kind {
    None,
    /// An AppImage file.
    AppImage,
    /// A single-file Flatpak bundle, `.flatpak`.
    FlatpakBundle,
    /// A Flatpak reference to an application in a remote, `.flatpakref`.
    FlatpakRef,
    /// A Flatpak remote description, `.flatpakrepo`.
    FlatpakRepo,
    /// A folder inside an installed Flatpak application.
    FlatpakApp,
    /// A snap file, `.snap`.
    SnapFile,
    /// A folder inside an installed snap, under `/snap`.
    SnapApp,
};

enum class Action {
    Run,
    Install,
    Update,
    Uninstall,
    /// Adds an AppImage to the application menu.
    Integrate,
    /// Removes an AppImage's application menu entry.
    RemoveIntegration,
    OpenInDiscover,
};

/// Where installed applications and menu entries live, so tests can use
/// temporary folders.
struct Locations {
    /// The per-user Flatpak installation, `~/.local/share/flatpak`.
    QString userFlatpak;
    /// The system Flatpak installation, `/var/lib/flatpak`.
    QString systemFlatpak;
    /// Where installed snaps are mounted, `/snap`.
    QString snaps;
    /// Where AppImage menu entries are written, `~/.local/share/applications`.
    QString applications;
    /// Where AppImage menu icons are copied.
    QString icons;

    static Locations system();
};

struct Package {
    Kind kind = Kind::None;
    /// The selected file or folder.
    QString path;
    /// The Flatpak application ID or snap name, when known.
    QString id;
    QString name;
    /// The Flatpak branch, such as stable.
    QString branch;
    /// The Flatpak remote URL, or the remote's homepage for a `.flatpakrepo`.
    QString source;
    /// A short description, from a `.flatpakref` or `.flatpakrepo`.
    QString comment;
    /// Whether a `.flatpakref` refers to a runtime rather than an application.
    bool runtime = false;
    bool installed = false;
    /// Whether an installed Flatpak is in the system installation rather than the user's.
    bool systemWide = false;
    /// The AppImage format, 1 or 2, or 0 when the file has no AppImage header.
    int appImageType = 0;
    /// Whether an AppImage has an application menu entry.
    bool integrated = false;

    bool isValid() const
    {
        return kind != Kind::None;
    }
};

/**
 * Whether @p path is a package, without reading the file, for enabling actions.
 * @param mimeType the item's MIME type; folders are recognized by their location
 */
bool isPackage(const QString &path, const QString &mimeType, const Locations &locations = Locations::system());

/// Identifies @p path, reading `.flatpakref` and `.flatpakrepo` files and the AppImage header.
Package identify(const QString &path, const QString &mimeType, const Locations &locations = Locations::system());

/// A localized name for @p kind, such as "Flatpak bundle".
QString kindName(Kind kind);

/// The actions @p package offers, whether or not the tools they need are installed.
QList<Action> actions(const Package &package);

/// Whether @p action installs, updates or removes something, and so needs confirmation.
bool changesSystem(Action action);

/**
 * The program @p action needs: flatpak, snap, plasma-discover or
 * appimageupdatetool. Empty when the package runs itself, as an AppImage
 * does, or when Plasma Opus does the work.
 */
QString tool(const Package &package, Action action);

struct Command {
    QString program;
    QStringList arguments;

    bool isEmpty() const
    {
        return program.isEmpty();
    }
    bool operator==(const Command &other) const = default;
};

/**
 * The command that performs @p action. Changes run without the tools' own
 * questions, so confirm them first.
 * @param systemWide for Flatpak installs, whether to install for all users,
 *        which asks for authorization; other Flatpak actions use the
 *        installation the application is in
 * @returns an empty command for integration, which writeIntegration() and
 *          removeIntegration() perform
 */
Command command(const Package &package, Action action, bool systemWide = false);

/// The commands that describe @p package and its permissions, such as `flatpak info`.
QList<Command> detailsCommands(const Package &package);

/// The AppImage format from the file's header: 1, 2, or 0 when it has none.
int appImageType(const QString &path);

/**
 * The command that extracts files matching @p pattern from a type 2
 * AppImage into `squashfs-root` in the working folder. It runs the
 * AppImage's own runtime.
 */
Command extractCommand(const QString &appImage, const QString &pattern);

/// The application menu entries for @p appImage, named as libappimage names them.
QStringList integrationFiles(const QString &appImage, const Locations &locations);

/**
 * Adds @p appImage to the application menu.
 * @param embeddedDesktopFile the AppImage's own desktop file, extracted, for
 *        its name, comment, categories and terminal setting; empty to use
 *        the file name
 * @param icon an extracted icon to copy; empty for a generic one
 * @returns an error message, or an empty string on success
 */
QString writeIntegration(const QString &appImage, const QString &embeddedDesktopFile, const QString &icon, const Locations &locations);

/// Removes @p appImage's application menu entries and their icons; returns an error message or an empty string.
QString removeIntegration(const QString &appImage, const Locations &locations);
}
