// SPDX-License-Identifier: GPL-3.0-or-later

#include "servicemenus.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTest>

class ServiceMenusTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString folder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kio/servicemenus");
        QVERIFY(QDir().mkpath(folder));
        QFile file(folder + QStringLiteral("/opusfm-test.desktop"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(
            "[Desktop Entry]\nType=Service\nMimeType=all/allfiles;\nActions=opusfmShown;opusfmHidden;\nX-KDE-Submenu=Opus Test\n\n"
            "[Desktop Action opusfmShown]\nName=Shown Action\nIcon=edit-copy\nExec=true %F\n\n"
            "[Desktop Action opusfmHidden]\nName=Hidden Action\nNoDisplay=true\nExec=true %F\n");
    }

    void listsServiceMenuActions()
    {
        const QList<ServiceMenuEntry> entries = ServiceMenus::installed();
        const auto shown = std::find_if(entries.cbegin(), entries.cend(), [](const ServiceMenuEntry &entry) {
            return entry.key == QLatin1String("opusfmShown");
        });
        QVERIFY(shown != entries.cend());
        QCOMPARE(shown->text, QStringLiteral("Shown Action"));
        QCOMPARE(shown->submenu, QStringLiteral("Opus Test"));
        QCOMPARE(shown->iconName, QStringLiteral("edit-copy"));
        QVERIFY(shown->shownByDefault);

        QVERIFY(std::none_of(entries.cbegin(), entries.cend(), [](const ServiceMenuEntry &entry) {
            return entry.key == QLatin1String("opusfmHidden");
        }));
    }

    void entriesAreSortedByText()
    {
        const QList<ServiceMenuEntry> entries = ServiceMenus::installed();
        QVERIFY(std::is_sorted(entries.cbegin(), entries.cend(), [](const ServiceMenuEntry &a, const ServiceMenuEntry &b) {
            return QString::localeAwareCompare(a.text, b.text) < 0;
        }));
    }

    void findsDolphinsDownloadConfig()
    {
        const QString folder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/knsrcfiles");
        const QString path = folder + QStringLiteral("/servicemenu.knsrc");
        QFile::remove(path);
        const QString system = ServiceMenus::downloadConfigPath();
        // A system-wide Dolphin may provide one; otherwise there is none.
        QVERIFY(system.isEmpty() || system.endsWith(QStringLiteral("/knsrcfiles/servicemenu.knsrc")));

        QVERIFY(QDir().mkpath(folder));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[KNewStuff3]\nCategories=Dolphin Service Menus\n");
        file.close();
        QCOMPARE(ServiceMenus::downloadConfigPath(), path);
        QVERIFY(QFile::remove(path));
    }
};

QTEST_GUILESS_MAIN(ServiceMenusTest)

#include "servicemenustest.moc"
