// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationpackagedialog.h"
#include "packagetools.h"
#include "settings.h"

#include <KDesktopFile>
#include <KMessageWidget>

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace AppPackages;

namespace
{
class FakeTools : public PackageTools
{
public:
    QStringList programs{QStringLiteral("flatpak"), QStringLiteral("snap"), QStringLiteral("plasma-discover")};
    QList<std::pair<Command, QString>> runs;
    QList<Command> launches;
    /// Results for changes, in order; details and extraction succeed with no output.
    QList<Result> results;
    QString launchError;
    /// Runs after each change, before its result is reported, to simulate its effect.
    std::function<void(const Command &, const QString &)> effect;

    bool hasProgram(const QString &program) const override
    {
        return programs.contains(program);
    }
    void run(const Command &command, const QString &workingDirectory, QObject *, std::function<void(const Result &)> done) override
    {
        runs.append({command, workingDirectory});
        if (effect) {
            effect(command, workingDirectory);
        }
        const bool query = command.arguments.value(0) == QLatin1String("info") || command.arguments.value(0) == QLatin1String("connections")
            || command.arguments.value(0) == QLatin1String("--appimage-extract");
        done(query || results.isEmpty() ? Result{true, 0, QStringLiteral("output")} : results.takeFirst());
    }
    void launch(const Command &command, QObject *, std::function<void(const QString &)> done) override
    {
        launches.append(command);
        done(launchError);
    }
};

void writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
}

QString messageText(const ApplicationPackageDialog &dialog)
{
    return dialog.findChild<KMessageWidget *>()->text();
}

QPushButton *button(const ApplicationPackageDialog &dialog, const QString &name)
{
    // Replaced buttons are deleted later, so skip hidden ones.
    const QList<QPushButton *> buttons = dialog.findChildren<QPushButton *>(name);
    for (QPushButton *candidate : buttons) {
        if (!candidate->isHidden()) {
            return candidate;
        }
    }
    return nullptr;
}
}

class ApplicationPackageDialogTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    Locations m_locations;
    QStringList m_questions;
    bool m_accept = true;

    ApplicationPackageDialog::Confirmation confirmation()
    {
        return [this](const QString &title, const QString &, const QString &) {
            m_questions.append(title);
            return m_accept;
        };
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        Settings::self()->setDefaults();
        for (const QString &entry : QDir(m_dir.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
            const QString path = m_dir.filePath(entry);
            QFileInfo(path).isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
        }
        m_locations = {m_dir.filePath(QStringLiteral("user-flatpak")),
                       m_dir.filePath(QStringLiteral("system-flatpak")),
                       m_dir.filePath(QStringLiteral("snap")),
                       m_dir.filePath(QStringLiteral("applications")),
                       m_dir.filePath(QStringLiteral("icons"))};
        m_questions.clear();
        m_accept = true;
    }

    void installsAFlatpakReferenceAfterConfirmation()
    {
        const QString ref = m_dir.filePath(QStringLiteral("kate.flatpakref"));
        writeFile(ref, "[Flatpak Ref]\nName=org.kde.kate\nBranch=stable\nTitle=Kate\nUrl=https://dl.flathub.org/repo/\n");
        FakeTools tools;
        ApplicationPackageDialog dialog(ref, QStringLiteral("application/vnd.flatpak.ref"), &tools, m_locations);
        dialog.setConfirmation(confirmation());
        QSignalSpy changed(&dialog, &ApplicationPackageDialog::packageChanged);
        QCOMPARE(dialog.package().kind, Kind::FlatpakRef);
        QVERIFY(button(dialog, QStringLiteral("packageInstall")));
        QVERIFY(button(dialog, QStringLiteral("packageOpenInDiscover")));
        QVERIFY(!button(dialog, QStringLiteral("packageUninstall")));
        // Nothing is queried for an application that is not installed.
        QVERIFY(tools.runs.isEmpty());

        m_accept = false;
        dialog.trigger(Action::Install);
        QCOMPARE(m_questions, QStringList{QStringLiteral("Install Flatpak")});
        QVERIFY(tools.runs.isEmpty());

        m_accept = true;
        tools.results = {{true, 1, QStringLiteral("Looking for matches\nerror: Unable to load summary from remote flathub")}};
        dialog.trigger(Action::Install);
        QCOMPARE(tools.runs.size(), 1);
        QCOMPARE(tools.runs.first().first.arguments,
                 (QStringList{QStringLiteral("install"), QStringLiteral("--user"), QStringLiteral("--noninteractive"), QStringLiteral("--from"), ref}));
        QVERIFY(messageText(dialog).contains(QStringLiteral("Could not install Kate")));
        QVERIFY(messageText(dialog).contains(QStringLiteral("Unable to load summary")));
        QVERIFY(dialog.findChild<QPlainTextEdit *>()->toPlainText().contains(QStringLiteral("$ flatpak install --user")));
        QCOMPARE(changed.count(), 0);
        QVERIFY(!dialog.isBusy());

        tools.runs.clear();
        dialog.findChild<QCheckBox *>(QStringLiteral("packageSystemWide"))->setChecked(true);
        tools.effect = [this](const Command &command, const QString &) {
            if (command.arguments.value(0) == QLatin1String("install")) {
                QDir().mkpath(m_locations.systemFlatpak + QStringLiteral("/app/org.kde.kate"));
            }
        };
        dialog.trigger(Action::Install);
        QCOMPARE(tools.runs.first().first.arguments.at(1), QStringLiteral("--system"));
        QCOMPARE(changed.count(), 1);
        QVERIFY(messageText(dialog).contains(QStringLiteral("Installed Kate")));
        QVERIFY(dialog.package().installed);
        QVERIFY(dialog.package().systemWide);
        // After installing, the details and permissions are read.
        QCOMPARE(tools.runs.size(), 3);
        QCOMPARE(tools.runs.at(2).first.arguments.at(2), QStringLiteral("--show-permissions"));
        QTRY_VERIFY(button(dialog, QStringLiteral("packageUninstall")));
        QVERIFY(!button(dialog, QStringLiteral("packageInstall")));

        tools.runs.clear();
        tools.effect = [this](const Command &command, const QString &) {
            if (command.arguments.value(0) == QLatin1String("uninstall")) {
                QDir(m_locations.systemFlatpak + QStringLiteral("/app/org.kde.kate")).removeRecursively();
            }
        };
        button(dialog, QStringLiteral("packageUninstall"))->click();
        QCOMPARE(tools.runs.first().first.arguments,
                 (QStringList{QStringLiteral("uninstall"), QStringLiteral("--system"), QStringLiteral("--noninteractive"), QStringLiteral("org.kde.kate")}));
        QVERIFY(!dialog.package().installed);
        QCOMPARE(changed.count(), 2);
    }

    void readsDetailsOfInstalledApplications()
    {
        QDir().mkpath(m_locations.snaps + QStringLiteral("/hello/42"));
        FakeTools tools;
        ApplicationPackageDialog dialog(m_locations.snaps + QStringLiteral("/hello/42"), QStringLiteral("inode/directory"), &tools, m_locations);
        QCOMPARE(dialog.package().kind, Kind::SnapApp);
        QCOMPARE(tools.runs.size(), 2);
        QCOMPARE(tools.runs.last().first, (Command{QStringLiteral("snap"), {QStringLiteral("connections"), QStringLiteral("hello")}}));
        QVERIFY(dialog.findChild<QPlainTextEdit *>()->toPlainText().contains(QStringLiteral("$ snap info hello")));
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("packageSystemWide"))->isHidden());

        dialog.trigger(Action::Run);
        QCOMPARE(tools.launches, (QList<Command>{{QStringLiteral("snap"), {QStringLiteral("run"), QStringLiteral("hello")}}}));
        tools.launchError = QStringLiteral("not found");
        dialog.trigger(Action::OpenInDiscover);
        QCOMPARE(tools.launches.last().arguments.last(), QStringLiteral("snap://hello"));
        QVERIFY(messageText(dialog).contains(QStringLiteral("Could not open Discover: not found")));
    }

    void reportsMissingTools()
    {
        const QString snap = m_dir.filePath(QStringLiteral("hello_42.snap"));
        writeFile(snap, "hsqs");
        FakeTools tools;
        tools.programs = {QStringLiteral("flatpak")};
        ApplicationPackageDialog dialog(snap, QStringLiteral("application/vnd.snap"), &tools, m_locations);
        dialog.setConfirmation(confirmation());
        QPushButton *install = button(dialog, QStringLiteral("packageInstall"));
        QVERIFY(install);
        QVERIFY(!install->isEnabled());
        QVERIFY(install->toolTip().contains(QStringLiteral("snap")));
        QVERIFY(!button(dialog, QStringLiteral("packageOpenInDiscover"))->isEnabled());
        QVERIFY(button(dialog, QStringLiteral("packageOpenInDiscover"))->toolTip().contains(QStringLiteral("Discover")));
        QVERIFY(tools.runs.isEmpty());

        dialog.trigger(Action::Install);
        QVERIFY(m_questions.isEmpty());
        QVERIFY(tools.runs.isEmpty());
        QVERIFY(messageText(dialog).contains(QStringLiteral("needs snap")));

        tools.programs.append(QStringLiteral("snap"));
        dialog.trigger(Action::Install);
        QCOMPARE(m_questions, QStringList{QStringLiteral("Install Snap")});
        QCOMPARE(tools.runs.first().first, (Command{QStringLiteral("snap"), {QStringLiteral("install"), QStringLiteral("--dangerous"), snap}}));
        // Then the installed package's details are read.
        QCOMPARE(tools.runs.last().first, (Command{QStringLiteral("snap"), {QStringLiteral("info"), snap}}));
    }

    void runsAndIntegratesAppImages()
    {
        const QString appImage = m_dir.filePath(QStringLiteral("MyApp-1.0-x86_64.AppImage"));
        writeFile(appImage,
                  QByteArray("\x7f"
                             "ELF\x02\x01\x01\x00"
                             "AI\x02",
                             11));
        QFile::setPermissions(appImage, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        FakeTools tools;
        tools.programs.clear();
        ApplicationPackageDialog dialog(appImage, QStringLiteral("application/vnd.appimage"), &tools, m_locations);
        dialog.setConfirmation(confirmation());
        QCOMPARE(dialog.package().appImageType, 2);
        QVERIFY(button(dialog, QStringLiteral("packageRun"))->isEnabled());
        QVERIFY(!button(dialog, QStringLiteral("packageUpdate"))->isEnabled());
        QVERIFY(button(dialog, QStringLiteral("packageUpdate"))->toolTip().contains(QStringLiteral("appimageupdatetool")));

        m_accept = false;
        dialog.trigger(Action::Run);
        QCOMPARE(m_questions, QStringList{QStringLiteral("Mark as Executable")});
        QVERIFY(!QFileInfo(appImage).isExecutable());
        QVERIFY(tools.launches.isEmpty());

        m_accept = true;
        dialog.trigger(Action::Run);
        QVERIFY(QFileInfo(appImage).isExecutable());
        QCOMPARE(tools.launches, (QList<Command>{{appImage, {}}}));

        tools.effect = [](const Command &command, const QString &workingDirectory) {
            const QString root = workingDirectory + QStringLiteral("/squashfs-root");
            const QString pattern = command.arguments.value(1);
            QDir().mkpath(root);
            if (pattern == QLatin1String("*.desktop")) {
                writeFile(root + QStringLiteral("/myapp.desktop"), "[Desktop Entry]\nType=Application\nName=My App\nExec=AppRun %U\nIcon=myapp\n");
            } else if (pattern == QLatin1String(".DirIcon")) {
                QFile::link(QStringLiteral("myapp.png"), root + QStringLiteral("/.DirIcon"));
            } else if (pattern == QLatin1String("myapp.png")) {
                writeFile(root + QStringLiteral("/myapp.png"),
                          QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="));
            }
        };
        m_questions.clear();
        dialog.trigger(Action::Integrate);
        QCOMPARE(m_questions, QStringList{QStringLiteral("Add to Application Menu")});
        QCOMPARE(tools.runs.size(), 3);
        QCOMPARE(tools.runs.at(2).first, (Command{appImage, {QStringLiteral("--appimage-extract"), QStringLiteral("myapp.png")}}));
        QVERIFY2(messageText(dialog).contains(QStringLiteral("Added My App")) || messageText(dialog).contains(QStringLiteral("Added MyApp")),
                 qPrintable(messageText(dialog)));
        QVERIFY(dialog.package().integrated);
        const QStringList entries = integrationFiles(appImage, m_locations);
        QCOMPARE(entries.size(), 1);
        const KDesktopFile desktop(entries.first());
        QCOMPARE(desktop.readName(), QStringLiteral("My App"));
        QVERIFY(desktop.readIcon().startsWith(m_locations.icons));
        QVERIFY(QFileInfo::exists(desktop.readIcon()));
        QTRY_VERIFY(button(dialog, QStringLiteral("packageRemoveIntegration")));

        dialog.trigger(Action::RemoveIntegration);
        QVERIFY(integrationFiles(appImage, m_locations).isEmpty());
        QVERIFY(!dialog.package().integrated);
        QVERIFY(messageText(dialog).contains(QStringLiteral("Removed")));
    }

    void integratesWithoutExtractingType1AppImages()
    {
        const QString appImage = m_dir.filePath(QStringLiteral("Old.AppImage"));
        writeFile(appImage,
                  QByteArray("\x7f"
                             "ELF\x02\x01\x01\x00"
                             "AI\x01",
                             11));
        FakeTools tools;
        ApplicationPackageDialog dialog(appImage, QStringLiteral("application/x-iso9660-appimage"), &tools, m_locations);
        dialog.setConfirmation(confirmation());
        dialog.trigger(Action::Integrate);
        // Type 1 AppImages cannot extract files, so nothing is run and no executable bit is needed.
        QVERIFY(tools.runs.isEmpty());
        QCOMPARE(m_questions, QStringList{QStringLiteral("Add to Application Menu")});
        QCOMPARE(integrationFiles(appImage, m_locations).size(), 1);
        QCOMPARE(KDesktopFile(integrationFiles(appImage, m_locations).first()).readIcon(), QStringLiteral("application-x-executable"));
    }

    void followsThePackageSettings()
    {
        const QString appImage = m_dir.filePath(QStringLiteral("MyApp.AppImage"));
        writeFile(appImage,
                  QByteArray("\x7f"
                             "ELF\x02\x01\x01\x00"
                             "AI\x02",
                             11));
        QFile::setPermissions(appImage, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        FakeTools tools;

        // Without reading its details, a type 2 AppImage is neither run nor marked executable.
        Settings::setAppImageReadDetails(false);
        {
            ApplicationPackageDialog dialog(appImage, QStringLiteral("application/vnd.appimage"), &tools, m_locations);
            dialog.setConfirmation(confirmation());
            dialog.trigger(Action::Integrate);
            QCOMPARE(m_questions, QStringList{QStringLiteral("Add to Application Menu")});
            QVERIFY(tools.runs.isEmpty());
            QVERIFY(!QFileInfo(appImage).isExecutable());
            QVERIFY2(messageText(dialog).contains(QStringLiteral("Added MyApp")), qPrintable(messageText(dialog)));
            QCOMPARE(KDesktopFile(integrationFiles(appImage, m_locations).first()).readIcon(), QStringLiteral("application-x-executable"));
        }

        // Without menu integration, an existing entry can still be removed.
        Settings::setAppImageMenuIntegration(false);
        {
            ApplicationPackageDialog dialog(appImage, QStringLiteral("application/vnd.appimage"), &tools, m_locations);
            dialog.setConfirmation(confirmation());
            QVERIFY(button(dialog, QStringLiteral("packageRun")));
            QVERIFY(button(dialog, QStringLiteral("packageRemoveIntegration")));
            dialog.trigger(Action::RemoveIntegration);
            QVERIFY(integrationFiles(appImage, m_locations).isEmpty());
            QTRY_VERIFY(!button(dialog, QStringLiteral("packageRemoveIntegration")));
            QVERIFY(!button(dialog, QStringLiteral("packageIntegrate")));
            dialog.trigger(Action::Integrate);
            QVERIFY(integrationFiles(appImage, m_locations).isEmpty());
        }

        // A package type that is off is described without actions.
        Settings::setFlatpakActions(false);
        const QString ref = m_dir.filePath(QStringLiteral("kate.flatpakref"));
        writeFile(ref, "[Flatpak Ref]\nName=org.kde.kate\nTitle=Kate\n");
        ApplicationPackageDialog dialog(ref, QStringLiteral("application/vnd.flatpak.ref"), &tools, m_locations);
        dialog.setConfirmation(confirmation());
        QCOMPARE(dialog.package().name, QStringLiteral("Kate"));
        QVERIFY(dialog.findChildren<QPushButton *>(QStringLiteral("packageInstall")).isEmpty());
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("packageSystemWide"))->isHidden());
        QVERIFY(messageText(dialog).contains(QStringLiteral("Preferences › Applications › Packages")));
        m_questions.clear();
        dialog.trigger(Action::Install);
        QVERIFY(m_questions.isEmpty());

        // Turning them on again in Preferences offers them in the open dialog.
        const auto restore = qScopeGuard([] {
            Settings::self()->setDefaults();
            Settings::self()->save();
        });
        Settings::setFlatpakActions(true);
        Settings::self()->save();
        QTRY_VERIFY(button(dialog, QStringLiteral("packageInstall")));
        QVERIFY(!dialog.findChild<QCheckBox *>(QStringLiteral("packageSystemWide"))->isHidden());
        QVERIFY(dialog.findChild<KMessageWidget *>()->isHidden());
    }
};

QTEST_MAIN(ApplicationPackageDialogTest)

#include "applicationpackagedialogtest.moc"
