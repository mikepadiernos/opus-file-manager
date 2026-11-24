// SPDX-License-Identifier: GPL-3.0-or-later

#include "colorschemes.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QStandardPaths>
#include <QTest>

#include <algorithm>
#include <cmath>

namespace
{
double luminance(const QColor &color)
{
    const auto channel = [](double value) {
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) + 0.0722 * channel(color.blueF());
}

double contrast(const QColor &first, const QColor &second)
{
    const auto [darker, lighter] = std::minmax(luminance(first), luminance(second));
    return (lighter + 0.05) / (darker + 0.05);
}
}

class ColorSchemesTest : public QObject
{
    Q_OBJECT

private:
    QString m_installed;
    QPalette m_platform;

    void install(const QString &fileName, const QString &name, const QString &window)
    {
        KConfig config(m_installed + QLatin1Char('/') + fileName, KConfig::SimpleConfig);
        config.group(QStringLiteral("General")).writeEntry("Name", name);
        config.group(QStringLiteral("Colors:Window")).writeEntry("BackgroundNormal", window);
        QVERIFY(config.sync());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
        m_installed = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/color-schemes");
        QDir(m_installed).removeRecursively();
        QVERIFY(QDir().mkpath(m_installed));
        m_platform = QGuiApplication::palette();
    }

    void cleanupTestCase()
    {
        QDir(m_installed).removeRecursively();
    }

    void cleanup()
    {
        ColorSchemes::apply(QString());
    }

    void presetsComeBeforeInstalledSchemesSortedByName()
    {
        install(QStringLiteral("ZetaTest.colors"), QStringLiteral("Zeta Test"), QStringLiteral("1,2,3"));
        install(QStringLiteral("AlphaTest.colors"), QStringLiteral("alpha test"), QStringLiteral("4,5,6"));
        const QList<ColorSchemes::Scheme> schemes = ColorSchemes::available();
        QVERIFY(schemes.size() >= 4);
        QCOMPARE(schemes.at(0).id, QLatin1String(ColorSchemes::LightPreset));
        QCOMPARE(schemes.at(1).id, QLatin1String(ColorSchemes::DarkPreset));
        QVERIFY(schemes.at(0).preset && schemes.at(1).preset);
        QStringList installed;
        for (const ColorSchemes::Scheme &scheme : schemes) {
            if (scheme.id.endsWith(QLatin1String("Test"))) {
                QVERIFY(!scheme.preset);
                installed.append(scheme.name);
            }
        }
        QCOMPARE(installed, QStringList({QStringLiteral("alpha test"), QStringLiteral("Zeta Test")}));
    }

    void presetsApplyOnlyToTheApplicationPalette()
    {
        QVERIFY(ColorSchemes::apply(QLatin1String(ColorSchemes::DarkPreset)));
        const QString path = qApp->property("KDE_COLOR_SCHEME_PATH").toString();
        QVERIFY(path.startsWith(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
        QVERIFY(QFile::exists(path));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Window), QColor(32, 35, 38));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Base), QColor(27, 30, 32));

        QVERIFY(ColorSchemes::apply(QLatin1String(ColorSchemes::LightPreset)));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Window), QColor(239, 240, 241));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Base), QColor(255, 255, 255));
    }

    void presetsKeepTextAndSelectionsReadable_data()
    {
        QTest::addColumn<QString>("preset");
        QTest::newRow("light") << QString::fromLatin1(ColorSchemes::LightPreset);
        QTest::newRow("dark") << QString::fromLatin1(ColorSchemes::DarkPreset);
    }

    void presetsKeepTextAndSelectionsReadable()
    {
        QFETCH(QString, preset);
        QVERIFY(ColorSchemes::apply(preset));
        const QPalette palette = QGuiApplication::palette();
        QVERIFY(contrast(palette.color(QPalette::Active, QPalette::Text), palette.color(QPalette::Active, QPalette::Base)) >= 4.5);
        QVERIFY(contrast(palette.color(QPalette::Active, QPalette::WindowText), palette.color(QPalette::Active, QPalette::Window)) >= 4.5);
        QVERIFY(contrast(palette.color(QPalette::Active, QPalette::HighlightedText), palette.color(QPalette::Active, QPalette::Highlight)) >= 4.5);
    }

    void installedSchemesApply()
    {
        install(QStringLiteral("CustomTest.colors"), QStringLiteral("Custom Test"), QStringLiteral("10,20,30"));
        QVERIFY(ColorSchemes::apply(QStringLiteral("CustomTest")));
        QCOMPARE(qApp->property("KDE_COLOR_SCHEME_PATH").toString(), m_installed + QStringLiteral("/CustomTest.colors"));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Window), QColor(10, 20, 30));

        // Edits to an installed scheme apply the next time it is applied.
        install(QStringLiteral("CustomTest.colors"), QStringLiteral("Custom Test"), QStringLiteral("40,50,60"));
        QVERIFY(ColorSchemes::apply(QStringLiteral("CustomTest")));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Window), QColor(40, 50, 60));
    }

    void followingPlasmaRestoresThePlatformPalette()
    {
        QVERIFY(ColorSchemes::apply(QLatin1String(ColorSchemes::DarkPreset)));
        QVERIFY(ColorSchemes::apply(QString()));
        QVERIFY(!qApp->property("KDE_COLOR_SCHEME_PATH").isValid());
        QCOMPARE(QGuiApplication::palette(), m_platform);
        QVERIFY(!QCoreApplication::testAttribute(Qt::AA_SetPalette));
    }

    void unavailableSchemesFollowPlasma()
    {
        QVERIFY(ColorSchemes::apply(QLatin1String(ColorSchemes::DarkPreset)));
        QVERIFY(!ColorSchemes::apply(QStringLiteral("MissingTest")));
        QVERIFY(ColorSchemes::path(QStringLiteral("MissingTest")).isEmpty());
        QVERIFY(!qApp->property("KDE_COLOR_SCHEME_PATH").isValid());
        QCOMPARE(QGuiApplication::palette(), m_platform);
    }

    void highContrastPreferenceFollowsPlasma()
    {
        QVERIFY(ColorSchemes::apply(QLatin1String(ColorSchemes::DarkPreset), {}, false));
        QVERIFY(ColorSchemes::apply(QLatin1String(ColorSchemes::DarkPreset), {QStringLiteral("Window=#102030")}, true));
        QVERIFY(!qApp->property("KDE_COLOR_SCHEME_PATH").isValid());
        QCOMPARE(QGuiApplication::palette(), m_platform);
        QVERIFY(ColorSchemes::apply(QStringLiteral("MissingTest"), {}, true));
    }

    void customColoursRoundTripAndSkipInvalidEntries()
    {
        const ColorSchemes::Overrides overrides = ColorSchemes::parseOverrides({QStringLiteral("Text=#112233"),
                                                                                QStringLiteral(" Window = #AABBCC "),
                                                                                QStringLiteral("Unknown=#000000"),
                                                                                QStringLiteral("Base=not a colour"),
                                                                                QStringLiteral("=#000000"),
                                                                                QStringLiteral("Highlight")});
        QCOMPARE(overrides.size(), 2);
        QCOMPARE(overrides.value(QPalette::Text), QColor(0x11, 0x22, 0x33));
        QCOMPARE(overrides.value(QPalette::Window), QColor(0xaa, 0xbb, 0xcc));
        QCOMPARE(ColorSchemes::serializeOverrides(overrides), QStringList({QStringLiteral("Window=#aabbcc"), QStringLiteral("Text=#112233")}));
    }

    void customColoursLayerOverTheScheme()
    {
        const QStringList custom{QStringLiteral("Window=#102030"), QStringLiteral("Highlight=#405060")};
        QVERIFY(ColorSchemes::apply(QLatin1String(ColorSchemes::DarkPreset), custom));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Window), QColor(0x10, 0x20, 0x30));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Inactive, QPalette::Highlight), QColor(0x40, 0x50, 0x60));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Base), QColor(27, 30, 32));

        // Following Plasma, only the custom roles are resolved.
        QVERIFY(ColorSchemes::apply(QString(), custom));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Window), QColor(0x10, 0x20, 0x30));
        QCOMPARE(QGuiApplication::palette().color(QPalette::Active, QPalette::Base), m_platform.color(QPalette::Active, QPalette::Base));
        QVERIFY(QGuiApplication::palette().isBrushSet(QPalette::Active, QPalette::Window));
        QVERIFY(!QGuiApplication::palette().isBrushSet(QPalette::Active, QPalette::Base));
    }

    void schemePalettesCanBeReadWithoutApplying()
    {
        const QPalette dark = ColorSchemes::palette(QLatin1String(ColorSchemes::DarkPreset));
        QCOMPARE(dark.color(QPalette::Active, QPalette::Window), QColor(32, 35, 38));
        QCOMPARE(QGuiApplication::palette(), m_platform);
        QVERIFY(ColorSchemes::palette(QString()).color(QPalette::Active, QPalette::Window).isValid());
    }

    void presetsPassEveryContrastCheck_data()
    {
        presetsKeepTextAndSelectionsReadable_data();
    }

    void presetsPassEveryContrastCheck()
    {
        QFETCH(QString, preset);
        QStringList failures;
        for (const ColorSchemes::ContrastIssue &issue : ColorSchemes::contrastIssues(ColorSchemes::palette(preset))) {
            failures.append(QStringLiteral("%1 (%2)").arg(issue.pair).arg(issue.ratio));
        }
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QStringLiteral(", "))));
    }

    void contrastIsMeasuredForChangedRoles()
    {
        QCOMPARE(ColorSchemes::contrastRatio(Qt::black, Qt::white), 21.0);
        QCOMPARE(ColorSchemes::contrastRatio(Qt::white, Qt::white), 1.0);

        QPalette palette = ColorSchemes::palette(QLatin1String(ColorSchemes::LightPreset));
        palette = ColorSchemes::withOverrides(palette, {{QPalette::Text, QColor(200, 200, 200)}});
        const QList<ColorSchemes::ContrastIssue> issues = ColorSchemes::contrastIssues(palette, {QPalette::Text});
        QCOMPARE(issues.size(), 2);
        for (const ColorSchemes::ContrastIssue &issue : issues) {
            QCOMPARE(issue.minimum, 4.5);
            QVERIFY(issue.ratio < 4.5);
        }
        QVERIFY(ColorSchemes::contrastIssues(palette, {QPalette::ToolTipBase}).isEmpty());
    }
};

QTEST_MAIN(ColorSchemesTest)

#include "colorschemestest.moc"
