// SPDX-License-Identifier: GPL-3.0-or-later

#include "themeprofiles.h"
#include "colorschemes.h"
#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class ThemeProfilesTest : public QObject
{
    Q_OBJECT

private:
    static QByteArray document(const QJsonObject &appearance, int version = ThemeProfiles::Version, const QString &name = QStringLiteral("Night"))
    {
        QJsonObject object;
        object.insert(QStringLiteral("format"), QLatin1String(ThemeProfiles::Format));
        object.insert(QStringLiteral("version"), version);
        object.insert(QStringLiteral("name"), name);
        object.insert(QStringLiteral("appearance"), appearance);
        return QJsonDocument(object).toJson();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Settings::self()->setDefaults();
        Settings::self()->save();
        KSharedConfig::openConfig()->deleteGroup(QLatin1String(ThemeProfiles::Group));
    }

    void entriesLeaveOutRulesAndTheHighContrastSafeguard()
    {
        const QStringList entries = ThemeProfiles::entries(Settings::self());
        QVERIFY(entries.contains(QStringLiteral("ColorScheme")));
        QVERIFY(entries.contains(QStringLiteral("TabShape")));
        QVERIFY(!entries.contains(QStringLiteral("FolderColorRules")));
        QVERIFY(!entries.contains(QStringLiteral("SuspendEffectsInHighContrast")));
        QVERIFY(!entries.contains(QStringLiteral("DimInactivePane")));
    }

    void exportedProfilesRoundTrip()
    {
        Settings::setColorScheme(QLatin1String(ColorSchemes::DarkPreset));
        Settings::setCornerRadius(Settings::EnumCornerRadius::Large);
        Settings::setSpacingScale(150);
        Settings::setPaneTintColor(QColor(200, 10, 20));
        Settings::setBackgroundBlur(true);
        Settings::setTreeLinePalette({QStringLiteral("#ff0000"), QStringLiteral("#00ff00")});
        Settings::setCustomColors({QStringLiteral("Window=#101010")});
        Settings::setTabFont(QStringLiteral("Noto Sans,14,-1,5,400,0,0,0,0,0,0,0,0,0,0,1"));
        const ThemeProfiles::Profile profile{QString(), QStringLiteral("  Night  "), ThemeProfiles::capture(Settings::self())};

        const QByteArray json = ThemeProfiles::toJson(Settings::self(), profile);
        const QJsonObject object = QJsonDocument::fromJson(json).object();
        QCOMPARE(object.value(QStringLiteral("version")).toInt(), ThemeProfiles::Version);
        QCOMPARE(object.value(QStringLiteral("name")).toString(), QStringLiteral("Night"));
        const QJsonObject appearance = object.value(QStringLiteral("appearance")).toObject();
        QCOMPARE(appearance.value(QStringLiteral("CornerRadius")).toString(), QStringLiteral("Large"));
        QCOMPARE(appearance.value(QStringLiteral("PaneTintColor")).toString(), QStringLiteral("#c80a14"));
        QVERIFY(!appearance.contains(QStringLiteral("FolderColorRules")));

        QString error;
        QStringList warnings;
        const auto read = ThemeProfiles::fromJson(Settings::self(), json, &error, &warnings);
        QVERIFY2(read, qPrintable(error));
        QVERIFY(warnings.isEmpty());
        QCOMPARE(read->name, QStringLiteral("Night"));
        QCOMPARE(read->values, profile.values);
    }

    void missingEntriesUseDefaultsAndUnknownOnesWarn()
    {
        QString error;
        QStringList warnings;
        const auto read = ThemeProfiles::fromJson(Settings::self(),
                                                  document({{QStringLiteral("TabShape"), QStringLiteral("Square")}, {QStringLiteral("Future"), 3}}),
                                                  &error,
                                                  &warnings);
        QVERIFY2(read, qPrintable(error));
        QCOMPARE(read->values.value(QStringLiteral("TabShape")).toInt(), int(Settings::EnumTabShape::Square));
        QCOMPARE(read->values.value(QStringLiteral("SpacingScale")).toInt(), 100);
        QCOMPARE(read->values.size(), ThemeProfiles::entries(Settings::self()).size());
        QCOMPARE(warnings.size(), 1);
        QVERIFY(warnings.constFirst().contains(QStringLiteral("Future")));
    }

    void rejectsInvalidProfiles_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("mentions");
        QTest::newRow("not json") << QByteArray("{") << QString();
        QTest::newRow("other format") << QByteArray(R"({"format":"other","version":1,"name":"A","appearance":{}})") << QString();
        QTest::newRow("newer version") << document({}, ThemeProfiles::Version + 1) << QStringLiteral("newer");
        QTest::newRow("empty name") << document({}, ThemeProfiles::Version, QStringLiteral("  ")) << QString();
        QTest::newRow("long name") << document({}, ThemeProfiles::Version, QString(ThemeProfiles::MaximumNameLength + 1, QLatin1Char('a'))) << QString();
        QTest::newRow("control name") << document({}, ThemeProfiles::Version, QStringLiteral("A\nB")) << QString();
        QTest::newRow("unknown choice") << document({{QStringLiteral("TabShape"), QStringLiteral("Hexagon")}}) << QStringLiteral("TabShape");
        QTest::newRow("out of range") << document({{QStringLiteral("SpacingScale"), 500}}) << QStringLiteral("SpacingScale");
        QTest::newRow("fraction") << document({{QStringLiteral("SpacingScale"), 120.5}}) << QStringLiteral("SpacingScale");
        QTest::newRow("wrong type") << document({{QStringLiteral("BackgroundBlur"), QStringLiteral("yes")}}) << QStringLiteral("BackgroundBlur");
        QTest::newRow("bad colour") << document({{QStringLiteral("PaneTintColor"), QStringLiteral("url(x)")}}) << QStringLiteral("PaneTintColor");
        QTest::newRow("bad accent") << document({{QStringLiteral("AccentColor"), QStringLiteral("nope")}}) << QStringLiteral("AccentColor");
        QTest::newRow("bad custom colour") << document({{QStringLiteral("CustomColors"), QJsonArray{QStringLiteral("Unknown=#fff")}}})
                                           << QStringLiteral("CustomColors");
        QTest::newRow("empty palette") << document({{QStringLiteral("TreeLinePalette"), QJsonArray{}}}) << QStringLiteral("TreeLinePalette");
        QTest::newRow("scheme path") << document({{QStringLiteral("ColorScheme"), QStringLiteral("../../etc/passwd")}}) << QStringLiteral("ColorScheme");
        QTest::newRow("too large") << QByteArray(ThemeProfiles::MaximumFileSize + 1, ' ') << QString();
    }

    void rejectsInvalidProfiles()
    {
        QFETCH(QByteArray, json);
        QFETCH(QString, mentions);
        QString error;
        QVERIFY(!ThemeProfiles::fromJson(Settings::self(), json, &error));
        QVERIFY(!error.isEmpty());
        if (!mentions.isEmpty()) {
            QVERIFY2(error.contains(mentions), qPrintable(error));
        }
    }

    void importRefusesLargeFilesAndExportWritesThem()
    {
        QTemporaryDir dir;
        const QString large = dir.filePath(QStringLiteral("large.json"));
        QFile file(large);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(ThemeProfiles::MaximumFileSize + 10, ' '));
        file.close();
        QString error;
        QVERIFY(!ThemeProfiles::importFile(Settings::self(), large, &error));
        QVERIFY(!error.isEmpty());

        const QString path = dir.filePath(QStringLiteral("night.opus-theme.json"));
        ThemeProfiles::Profile profile{QString(), QStringLiteral("Night"), ThemeProfiles::defaults(Settings::self())};
        profile.values.insert(QStringLiteral("Animations"), int(Settings::EnumAnimations::Off));
        QVERIFY(ThemeProfiles::exportFile(Settings::self(), profile, path).isEmpty());
        const auto read = ThemeProfiles::importFile(Settings::self(), path, &error);
        QVERIFY2(read, qPrintable(error));
        QCOMPARE(read->values, profile.values);
    }

    void reportsUnavailableSchemesAndFontsButKeepsThem()
    {
        QVariantMap values = ThemeProfiles::defaults(Settings::self());
        QVERIFY(ThemeProfiles::unavailable(values).isEmpty());
        values.insert(QStringLiteral("ColorScheme"), QStringLiteral("NoSuchScheme"));
        values.insert(QStringLiteral("FileListFont"), QStringLiteral("No Such Font Family Opus,12,-1,5,400,0,0,0,0,0,0,0,0,0,0,1"));
        values.insert(QStringLiteral("TabFont"), QStringLiteral("No Such Font Family Opus,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1"));
        const QStringList missing = ThemeProfiles::unavailable(values);
        QCOMPARE(missing.size(), 2);
        QVERIFY(missing.constFirst().contains(QStringLiteral("NoSuchScheme")));
        QVERIFY(missing.constLast().contains(QStringLiteral("No Such Font Family Opus")));

        const ThemeProfiles::Profile profile{QString(), QStringLiteral("Missing"), values};
        const auto read = ThemeProfiles::fromJson(Settings::self(), ThemeProfiles::toJson(Settings::self(), profile), nullptr);
        QVERIFY(read);
        QCOMPARE(read->values.value(QStringLiteral("ColorScheme")).toString(), QStringLiteral("NoSuchScheme"));
        QVERIFY(read->values.value(QStringLiteral("FileListFont")).toString().startsWith(QStringLiteral("No Such Font Family Opus")));
    }

    void savesAndLoadsProfiles()
    {
        KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(ThemeProfiles::Group));
        QList<ThemeProfiles::Profile> profiles{{QString(), QStringLiteral("Day"), ThemeProfiles::defaults(Settings::self())},
                                               {QString(), QStringLiteral("Night"), ThemeProfiles::defaults(Settings::self())}};
        profiles[1].values.insert(QStringLiteral("TabIndicator"), int(Settings::EnumTabIndicator::ThickLine));
        QString error;
        QVERIFY2(ThemeProfiles::save(Settings::self(), group, profiles, &error), qPrintable(error));
        QVERIFY(!profiles.constFirst().id.isEmpty());

        const QList<ThemeProfiles::Profile> loaded = ThemeProfiles::load(Settings::self(), group, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.at(1).id, profiles.at(1).id);
        QCOMPARE(loaded.at(1).values, profiles.at(1).values);
        QCOMPARE(ThemeProfiles::indexOfName(loaded, QStringLiteral(" night ")), 1);

        QList<ThemeProfiles::Profile> duplicate = profiles;
        duplicate[1].name = QStringLiteral("DAY");
        QVERIFY(!ThemeProfiles::save(Settings::self(), group, duplicate, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(ThemeProfiles::load(Settings::self(), group).size(), 2);
    }

    void loadSkipsInvalidStoredProfiles()
    {
        KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(ThemeProfiles::Group));
        group.writeEntry("Profiles",
                         QByteArray(R"([{"id":"8d9c4f0e-6a7b-4c1d-9e2f-3a4b5c6d7e8f","name":"Good","appearance":{"TabShape":"Flat"}},)"
                                    R"({"id":"bad","name":"No id","appearance":{}},)"
                                    R"({"id":"0f9c4f0e-6a7b-4c1d-9e2f-3a4b5c6d7e8f","name":"Bad value","appearance":{"TabShape":"Hexagon"}}])"));
        QString error;
        const QList<ThemeProfiles::Profile> loaded = ThemeProfiles::load(Settings::self(), group, &error);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.constFirst().values.value(QStringLiteral("TabShape")).toInt(), int(Settings::EnumTabShape::Flat));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(ThemeProfilesTest)

#include "themeprofilestest.moc"
