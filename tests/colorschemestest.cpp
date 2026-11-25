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

    void readableColoursKeepTheirHueAndReachTheMinimum()
    {
        const QColor blue(0x3d, 0xae, 0xe9);
        QCOMPARE(ColorSchemes::readable(blue, Qt::black), blue);
        QVERIFY(!ColorSchemes::readable(QColor(), Qt::white).isValid());
        QCOMPARE(ColorSchemes::readable(QColor(0x3d, 0xae, 0xe9, 0x40), Qt::black).alpha(), 255);

        const QColor pale(0xf0, 0xf0, 0xd0);
        const QColor onWhite = ColorSchemes::readable(pale, Qt::white);
        QVERIFY(ColorSchemes::contrastRatio(onWhite, Qt::white) >= 3.0);
        QVERIFY(onWhite.lightness() < pale.lightness());
        QVERIFY(onWhite.blue() < onWhite.red());

        const QColor dark(0x20, 0x10, 0x40);
        const QColor onBlack = ColorSchemes::readable(dark, Qt::black, 4.5);
        QVERIFY(ColorSchemes::contrastRatio(onBlack, Qt::black) >= 4.5);
        QVERIFY(onBlack.lightness() > dark.lightness());
    }

    void accentKeepsSelectionTextReadable()
    {
        const QPalette light = ColorSchemes::palette(QLatin1String(ColorSchemes::LightPreset));
        QVERIFY(ColorSchemes::accentOverrides(light, QColor()).isEmpty());

        const ColorSchemes::Overrides dark = ColorSchemes::accentOverrides(light, QColor(0x20, 0x30, 0x60, 0x80));
        QCOMPARE(dark.keys().size(), ColorSchemes::accentRoles().size());
        QCOMPARE(dark.value(QPalette::Highlight), QColor(0x20, 0x30, 0x60));
        QCOMPARE(dark.value(QPalette::Accent), QColor(0x20, 0x30, 0x60));
        QVERIFY(ColorSchemes::contrastRatio(dark.value(QPalette::HighlightedText), dark.value(QPalette::Highlight)) >= 4.5);

        const ColorSchemes::Overrides pale = ColorSchemes::accentOverrides(light, QColor(0xff, 0xe0, 0x80));
        QCOMPARE(pale.value(QPalette::HighlightedText), QColor(Qt::black));
    }

    void customColoursTakePrecedenceOverTheAccent()
    {
        const QString scheme = QLatin1String(ColorSchemes::DarkPreset);
        const QStringList custom{QStringLiteral("Highlight=#405060")};
        QCOMPARE(ColorSchemes::withAccent(scheme, QString(), custom), custom);
        QCOMPARE(ColorSchemes::withAccent(scheme, QStringLiteral("not a colour"), custom), custom);

        const QStringList combined = ColorSchemes::withAccent(scheme, QStringLiteral("#e06020"), custom);
        const ColorSchemes::Overrides overrides = ColorSchemes::parseOverrides(combined);
        QCOMPARE(overrides.value(QPalette::Highlight), QColor(0x40, 0x50, 0x60));
        QCOMPARE(overrides.value(QPalette::Accent), QColor(0xe0, 0x60, 0x20));
        QVERIFY(overrides.contains(QPalette::HighlightedText));
    }

    void plasmaAccentIsLayeredOverAppLocalSchemes()
    {
        const QColor plasma = ColorSchemes::palette(QString()).color(QPalette::Active, QPalette::Accent);
        QCOMPARE(ColorSchemes::accentColor(QLatin1String(ColorSchemes::PlasmaAccent)), plasma);
        QVERIFY(!ColorSchemes::accentColor(QString()).isValid());
        QCOMPARE(ColorSchemes::accentColor(QStringLiteral("#e06020")), QColor(0xe0, 0x60, 0x20));

        // Following Plasma's scheme already follows its accent.
        const QStringList custom{QStringLiteral("Window=#405060")};
        QCOMPARE(ColorSchemes::withAccent(QString(), QLatin1String(ColorSchemes::PlasmaAccent), custom), custom);
        const ColorSchemes::Overrides overrides =
            ColorSchemes::parseOverrides(ColorSchemes::withAccent(QLatin1String(ColorSchemes::DarkPreset), QLatin1String(ColorSchemes::PlasmaAccent), custom));
        QCOMPARE(overrides.value(QPalette::Accent), QColor(plasma.rgb()));
        QCOMPARE(overrides.value(QPalette::Highlight), QColor(plasma.rgb()));
        QCOMPARE(overrides.value(QPalette::Window), QColor(0x40, 0x50, 0x60));
    }

    void paneTintKeepsViewTextReadable()
    {
        const QPalette light = ColorSchemes::palette(QLatin1String(ColorSchemes::LightPreset));
        QVERIFY(ColorSchemes::paneTint(light, QColor(), 20).isEmpty());
        QVERIFY(ColorSchemes::paneTint(light, Qt::blue, 0).isEmpty());

        const ColorSchemes::Overrides gentle = ColorSchemes::paneTint(light, QColor(0, 0, 255), 10);
        QCOMPARE(gentle.keys().size(), 2);
        const QColor base = light.color(QPalette::Active, QPalette::Base);
        QVERIFY(qAbs(gentle.value(QPalette::Base).blue() - (base.blue() + (255 - base.blue()) / 10)) <= 1);
        QVERIFY(gentle.value(QPalette::Base).red() < base.red());

        // Dark text on a light scheme: a strong dark tint is weakened until text stays readable.
        const ColorSchemes::Overrides strong = ColorSchemes::paneTint(light, QColor(0, 0, 0), 50);
        const QColor text = light.color(QPalette::Active, QPalette::Text);
        QVERIFY(ColorSchemes::contrastRatio(text, strong.value(QPalette::Base)) >= 4.5);
        QVERIFY(ColorSchemes::contrastRatio(text, strong.value(QPalette::AlternateBase)) >= 4.5);
        QVERIFY(strong.value(QPalette::Base) != base);
        const QColor fullStrength(base.red() / 2, base.green() / 2, base.blue() / 2);
        QVERIFY(strong.value(QPalette::Base).lightness() > fullStrength.lightness());

        // The effective strength reports how far the tint was weakened.
        QCOMPARE(ColorSchemes::paneTintStrength(light, QColor(0, 0, 255), 10), 10);
        const int weakened = ColorSchemes::paneTintStrength(light, QColor(0, 0, 0), 50);
        QVERIFY(weakened > 0 && weakened < 50);
        QCOMPARE(ColorSchemes::paneTintStrength(light, QColor(), 20), 0);
        QCOMPARE(ColorSchemes::paneTintStrength(light, Qt::blue, 0), 0);
    }

    void translucentBackgroundsKeepTheirColour()
    {
        const QPalette light = ColorSchemes::palette(QLatin1String(ColorSchemes::LightPreset));
        QVERIFY(ColorSchemes::translucent(light, {QPalette::Window}, 100).isEmpty());
        const ColorSchemes::Overrides overrides = ColorSchemes::translucent(light, {QPalette::Window, QPalette::Base}, 50);
        QCOMPARE(overrides.size(), 2);
        QCOMPARE(overrides.value(QPalette::Window).alpha(), 128);
        QCOMPARE(overrides.value(QPalette::Window).rgb(), light.color(QPalette::Active, QPalette::Window).rgb());
        QCOMPARE(overrides.value(QPalette::Base).rgb(), light.color(QPalette::Active, QPalette::Base).rgb());
    }
};

QTEST_MAIN(ColorSchemesTest)

#include "colorschemestest.moc"
