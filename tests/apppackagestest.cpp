// SPDX-License-Identifier: GPL-3.0-or-later

#include "apppackages.h"

#include <KConfig>
#include <KConfigGroup>
#include <KDesktopFile>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace AppPackages;

namespace
{
const QString AppImageType = QStringLiteral("application/vnd.appimage");
const QString FlatpakRefType = QStringLiteral("application/vnd.flatpak.ref");

void writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
}
}

class AppPackagesTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    Locations m_locations;

    QString path(const QString &relative) const
    {
        return m_dir.filePath(relative);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        for (const QString &entry : QDir(m_dir.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            QFileInfo(m_dir.filePath(entry)).isDir() ? QDir(m_dir.filePath(entry)).removeRecursively() : QFile::remove(m_dir.filePath(entry));
        }
        m_locations = {path(QStringLiteral("user-flatpak")),
                       path(QStringLiteral("system-flatpak")),
                       path(QStringLiteral("snap")),
                       path(QStringLiteral("applications")),
                       path(QStringLiteral("icons"))};
    }

    void recognisesPackageFiles()
    {
        QVERIFY(isPackage(path(QStringLiteral("a.AppImage")), AppImageType, m_locations));
        QVERIFY(isPackage(path(QStringLiteral("a.appimage")), QStringLiteral("application/x-iso9660-appimage"), m_locations));
        QVERIFY(isPackage(path(QStringLiteral("a.flatpak")), QStringLiteral("application/vnd.flatpak"), m_locations));
        // shared-mime-info keeps the old xdg-app name as an alias.
        QVERIFY(isPackage(path(QStringLiteral("a.xdgapp")), QStringLiteral("application/vnd.xdgapp"), m_locations));
        QVERIFY(isPackage(path(QStringLiteral("a.flatpakref")), FlatpakRefType, m_locations));
        QVERIFY(isPackage(path(QStringLiteral("a.flatpakrepo")), QStringLiteral("application/vnd.flatpak.repo"), m_locations));
        QVERIFY(isPackage(path(QStringLiteral("a.snap")), QStringLiteral("application/vnd.snap"), m_locations));
        QVERIFY(!isPackage(path(QStringLiteral("a.zip")), QStringLiteral("application/zip"), m_locations));
        QVERIFY(!isPackage(QString(), AppImageType, m_locations));
    }

    void recognisesInstalledApplicationFolders()
    {
        QDir().mkpath(m_locations.userFlatpak + QStringLiteral("/app/org.kde.kate/current"));
        QDir().mkpath(m_locations.systemFlatpak + QStringLiteral("/app/org.kde.krita"));
        QDir().mkpath(m_locations.snaps + QStringLiteral("/hello/42"));
        QDir().mkpath(m_locations.snaps + QStringLiteral("/bin"));
        writeFile(m_locations.systemFlatpak + QStringLiteral("/exports/share/applications/org.kde.krita.desktop"),
                  "[Desktop Entry]\nType=Application\nName=Krita\nExec=krita\n");

        const QString folder = QStringLiteral("inode/directory");
        Package kate = identify(m_locations.userFlatpak + QStringLiteral("/app/org.kde.kate/current"), folder, m_locations);
        QCOMPARE(kate.kind, Kind::FlatpakApp);
        QCOMPARE(kate.id, QStringLiteral("org.kde.kate"));
        QCOMPARE(kate.name, QStringLiteral("org.kde.kate"));
        QVERIFY(kate.installed);
        QVERIFY(!kate.systemWide);

        const Package krita = identify(m_locations.systemFlatpak + QStringLiteral("/app/org.kde.krita"), folder, m_locations);
        QCOMPARE(krita.kind, Kind::FlatpakApp);
        QCOMPARE(krita.name, QStringLiteral("Krita"));
        QVERIFY(krita.systemWide);

        const Package hello = identify(m_locations.snaps + QStringLiteral("/hello/42"), folder, m_locations);
        QCOMPARE(hello.kind, Kind::SnapApp);
        QCOMPARE(hello.id, QStringLiteral("hello"));

        QVERIFY(!isPackage(m_locations.snaps + QStringLiteral("/bin"), folder, m_locations));
        QVERIFY(!isPackage(m_locations.snaps, folder, m_locations));
        QVERIFY(!isPackage(m_locations.userFlatpak + QStringLiteral("/app"), folder, m_locations));
        QVERIFY(!isPackage(m_locations.userFlatpak + QStringLiteral("/runtime/org.kde.Platform"), folder, m_locations));
        QCOMPARE(identify(path(QStringLiteral("elsewhere")), folder, m_locations).kind, Kind::None);
    }

    void readsFlatpakReferences()
    {
        const QString ref = path(QStringLiteral("kate.flatpakref"));
        writeFile(ref,
                  "[Flatpak Ref]\nName=org.kde.kate\nBranch=stable\nTitle=Kate\nUrl=https://dl.flathub.org/repo/\n"
                  "Comment=Advanced text editor\nIsRuntime=false\n");
        Package package = identify(ref, FlatpakRefType, m_locations);
        QCOMPARE(package.kind, Kind::FlatpakRef);
        QCOMPARE(package.id, QStringLiteral("org.kde.kate"));
        QCOMPARE(package.name, QStringLiteral("Kate"));
        QCOMPARE(package.branch, QStringLiteral("stable"));
        QCOMPARE(package.source, QStringLiteral("https://dl.flathub.org/repo/"));
        QCOMPARE(package.comment, QStringLiteral("Advanced text editor"));
        QVERIFY(!package.installed);
        QCOMPARE(actions(package), (QList<Action>{Action::Install, Action::OpenInDiscover}));

        QDir().mkpath(m_locations.systemFlatpak + QStringLiteral("/app/org.kde.kate"));
        package = identify(ref, FlatpakRefType, m_locations);
        QVERIFY(package.installed);
        QVERIFY(package.systemWide);
        QCOMPARE(actions(package), (QList<Action>{Action::Run, Action::Update, Action::Uninstall, Action::OpenInDiscover}));
        QCOMPARE(command(package, Action::Uninstall),
                 (Command{QStringLiteral("flatpak"),
                          {QStringLiteral("uninstall"), QStringLiteral("--system"), QStringLiteral("--noninteractive"), QStringLiteral("org.kde.kate")}}));
        QCOMPARE(command(package, Action::OpenInDiscover),
                 (Command{QStringLiteral("plasma-discover"), {QStringLiteral("--application"), QStringLiteral("appstream://org.kde.kate")}}));
        QCOMPARE(detailsCommands(package).size(), 2);
        QCOMPARE(detailsCommands(package).last().arguments,
                 (QStringList{QStringLiteral("info"), QStringLiteral("--system"), QStringLiteral("--show-permissions"), QStringLiteral("org.kde.kate")}));
    }

    void readsRuntimeReferences()
    {
        const QString ref = path(QStringLiteral("platform.flatpakref"));
        writeFile(ref, "[Flatpak Ref]\nName=org.kde.Platform\nBranch=6.8\nIsRuntime=true\n");
        QDir().mkpath(m_locations.userFlatpak + QStringLiteral("/runtime/org.kde.Platform"));
        const Package package = identify(ref, FlatpakRefType, m_locations);
        QVERIFY(package.runtime);
        QVERIFY(package.installed);
        QCOMPARE(package.name, QStringLiteral("org.kde.Platform"));
        QCOMPARE(actions(package), (QList<Action>{Action::Update, Action::Uninstall}));
        QCOMPARE(detailsCommands(package).size(), 1);
    }

    void buildsFlatpakInstallCommands()
    {
        const QString bundle = path(QStringLiteral("app.flatpak"));
        const Package package = identify(bundle, QStringLiteral("application/vnd.flatpak"), m_locations);
        QCOMPARE(package.kind, Kind::FlatpakBundle);
        QCOMPARE(tool(package, Action::Install), QStringLiteral("flatpak"));
        QCOMPARE(command(package, Action::Install),
                 (Command{QStringLiteral("flatpak"),
                          {QStringLiteral("install"), QStringLiteral("--user"), QStringLiteral("--noninteractive"), QStringLiteral("--bundle"), bundle}}));
        QCOMPARE(command(package, Action::Install, true).arguments.at(1), QStringLiteral("--system"));
        QCOMPARE(command(package, Action::OpenInDiscover), (Command{QStringLiteral("plasma-discover"), {QStringLiteral("--local-filename"), bundle}}));
        QVERIFY(command(package, Action::Uninstall).isEmpty());
        QVERIFY(detailsCommands(package).isEmpty());

        const QString repo = path(QStringLiteral("Flathub Beta.flatpakrepo"));
        writeFile(repo, "[Flatpak Repo]\nTitle=Flathub beta\nUrl=https://dl.flathub.org/beta-repo/\n");
        const Package remote = identify(repo, QStringLiteral("application/vnd.flatpak.repo"), m_locations);
        QCOMPARE(remote.name, QStringLiteral("Flathub beta"));
        QCOMPARE(remote.id, QStringLiteral("flathub-beta"));
        QCOMPARE(actions(remote), QList<Action>{Action::Install});
        QCOMPARE(command(remote, Action::Install).arguments,
                 (QStringList{QStringLiteral("remote-add"),
                              QStringLiteral("--user"),
                              QStringLiteral("--if-not-exists"),
                              QStringLiteral("--from"),
                              QStringLiteral("flathub-beta"),
                              repo}));
    }

    void buildsSnapCommands()
    {
        const QString file = path(QStringLiteral("hello_42.snap"));
        Package package = identify(file, QStringLiteral("application/vnd.snap"), m_locations);
        QCOMPARE(package.kind, Kind::SnapFile);
        QCOMPARE(package.id, QStringLiteral("hello"));
        QVERIFY(!package.installed);
        QCOMPARE(actions(package), (QList<Action>{Action::Install, Action::OpenInDiscover}));
        QCOMPARE(command(package, Action::Install), (Command{QStringLiteral("snap"), {QStringLiteral("install"), QStringLiteral("--dangerous"), file}}));
        QCOMPARE(detailsCommands(package), (QList<Command>{{QStringLiteral("snap"), {QStringLiteral("info"), file}}}));

        QDir().mkpath(m_locations.snaps + QStringLiteral("/hello"));
        package = identify(file, QStringLiteral("application/vnd.snap"), m_locations);
        QVERIFY(package.installed);
        QVERIFY(actions(package).contains(Action::Uninstall));
        QCOMPARE(command(package, Action::Uninstall), (Command{QStringLiteral("snap"), {QStringLiteral("remove"), QStringLiteral("hello")}}));

        const Package app = identify(m_locations.snaps + QStringLiteral("/hello"), QStringLiteral("inode/directory"), m_locations);
        QCOMPARE(command(app, Action::Update), (Command{QStringLiteral("snap"), {QStringLiteral("refresh"), QStringLiteral("hello")}}));
        QCOMPARE(command(app, Action::Run), (Command{QStringLiteral("snap"), {QStringLiteral("run"), QStringLiteral("hello")}}));
        QCOMPARE(command(app, Action::OpenInDiscover).arguments.last(), QStringLiteral("snap://hello"));
        QCOMPARE(detailsCommands(app).last(), (Command{QStringLiteral("snap"), {QStringLiteral("connections"), QStringLiteral("hello")}}));
    }

    void readsTheAppImageHeader()
    {
        const QString type2 = path(QStringLiteral("type2.AppImage"));
        writeFile(type2,
                  QByteArray("\x7f"
                             "ELF\x02\x01\x01\x00"
                             "AI\x02",
                             11)
                      + QByteArray(32, '\0'));
        QCOMPARE(appImageType(type2), 2);
        const QString type1 = path(QStringLiteral("type1.AppImage"));
        writeFile(type1,
                  QByteArray("\x7f"
                             "ELF\x02\x01\x01\x00"
                             "AI\x01",
                             11));
        QCOMPARE(appImageType(type1), 1);
        const QString plain = path(QStringLiteral("plain"));
        writeFile(plain,
                  QByteArray("\x7f"
                             "ELF\x02\x01\x01\x00\x00\x00\x00",
                             11));
        QCOMPARE(appImageType(plain), 0);
        QCOMPARE(appImageType(path(QStringLiteral("missing"))), 0);

        const Package package = identify(type2, AppImageType, m_locations);
        QCOMPARE(package.kind, Kind::AppImage);
        QCOMPARE(package.name, QStringLiteral("type2"));
        QCOMPARE(package.appImageType, 2);
        QCOMPARE(actions(package), (QList<Action>{Action::Run, Action::Integrate, Action::Update}));
        QCOMPARE(command(package, Action::Run), (Command{type2, {}}));
        QCOMPARE(tool(package, Action::Run), QString());
        QCOMPARE(tool(package, Action::Update), QStringLiteral("appimageupdatetool"));
        QVERIFY(command(package, Action::Integrate).isEmpty());
        QCOMPARE(extractCommand(type2, QStringLiteral("*.desktop")), (Command{type2, {QStringLiteral("--appimage-extract"), QStringLiteral("*.desktop")}}));
        QVERIFY(changesSystem(Action::Integrate));
        QVERIFY(!changesSystem(Action::Run));
        QVERIFY(!changesSystem(Action::OpenInDiscover));
    }

    void integratesAppImagesIntoTheMenu()
    {
        const QString appImage = path(QStringLiteral("My \"App\" $1.AppImage"));
        writeFile(appImage,
                  QByteArray("\x7f"
                             "ELF\x02\x01\x01\x00"
                             "AI\x02",
                             11));
        const QString embedded = path(QStringLiteral("squashfs-root/myapp.desktop"));
        writeFile(embedded,
                  "[Desktop Entry]\nType=Application\nName=My App\nComment=Does things\nExec=AppRun %F\nCategories=Graphics;Viewer;\n"
                  "Terminal=false\nIcon=myapp\n");
        const QString icon = path(QStringLiteral("squashfs-root/myapp.png"));
        writeFile(icon, QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="));

        QVERIFY(integrationFiles(appImage, m_locations).isEmpty());
        QCOMPARE(writeIntegration(appImage, embedded, icon, m_locations), QString());
        const QStringList files = integrationFiles(appImage, m_locations);
        QCOMPARE(files.size(), 1);
        QVERIFY(QFileInfo(files.first()).fileName().startsWith(QStringLiteral("appimagekit_")));
        QVERIFY(files.first().endsWith(QStringLiteral("-my-app.desktop")));

        const KDesktopFile desktop(files.first());
        QCOMPARE(desktop.readName(), QStringLiteral("My App"));
        QCOMPARE(desktop.readComment(), QStringLiteral("Does things"));
        const KConfigGroup group = desktop.desktopGroup();
        QCOMPARE(group.readEntry("Exec"), QStringLiteral("\"%1\" %F").arg(path(QStringLiteral("My \\\"App\\\" \\$1.AppImage"))));
        QCOMPARE(group.readEntry("TryExec"), appImage);
        QCOMPARE(group.readXdgListEntry("Categories"), (QStringList{QStringLiteral("Graphics"), QStringLiteral("Viewer")}));
        const QString copiedIcon = desktop.readIcon();
        QVERIFY(copiedIcon.startsWith(m_locations.icons));
        QVERIFY(copiedIcon.endsWith(QStringLiteral(".png")));
        QVERIFY(QFileInfo::exists(copiedIcon));
        QVERIFY(identify(appImage, AppImageType, m_locations).integrated);
        QVERIFY(actions(identify(appImage, AppImageType, m_locations)).contains(Action::RemoveIntegration));

        // Integrating again replaces the entry instead of adding another.
        QCOMPARE(writeIntegration(appImage, QString(), QString(), m_locations), QString());
        QCOMPARE(integrationFiles(appImage, m_locations).size(), 1);
        QVERIFY(!QFileInfo::exists(copiedIcon));
        const KDesktopFile plain(integrationFiles(appImage, m_locations).first());
        QCOMPARE(plain.readName(), QStringLiteral("My \"App\" $1"));
        QCOMPARE(plain.readIcon(), QStringLiteral("application-x-executable"));

        // Entries for other AppImages are left alone.
        const QString other = path(QStringLiteral("Other.AppImage"));
        writeFile(other, "");
        QCOMPARE(writeIntegration(other, QString(), QString(), m_locations), QString());

        QCOMPARE(removeIntegration(appImage, m_locations), QString());
        QVERIFY(integrationFiles(appImage, m_locations).isEmpty());
        QCOMPARE(integrationFiles(other, m_locations).size(), 1);
        QVERIFY(!identify(appImage, AppImageType, m_locations).integrated);
    }

    void namesKinds()
    {
        QVERIFY(kindName(Kind::None).isEmpty());
        QVERIFY(!kindName(Kind::AppImage).isEmpty());
        QVERIFY(!kindName(Kind::SnapApp).isEmpty());
    }

    void usesTheSystemLocations()
    {
        const Locations locations = Locations::system();
        QCOMPARE(locations.systemFlatpak, QStringLiteral("/var/lib/flatpak"));
        QCOMPARE(locations.snaps, QStringLiteral("/snap"));
        QVERIFY(locations.applications.endsWith(QStringLiteral("/applications")));
        QVERIFY(locations.userFlatpak.endsWith(QStringLiteral("/flatpak")));
    }
};

QTEST_GUILESS_MAIN(AppPackagesTest)

#include "apppackagestest.moc"
