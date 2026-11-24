// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearancedefaults.h"
#include "colorschemes.h"
#include "settings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QScopeGuard>
#include <QStandardPaths>
#include <QTest>

class AppearanceDefaultsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        ColorSchemes::setHighContrastPreferredForTesting(std::nullopt);
        Settings::self()->setDefaults();
        Settings::self()->save();
    }

    void resetsAppearanceButKeepsFolderColourRulesAndOtherGroups()
    {
        const QStringList rules{QStringLiteral("name:*-archive=#da4453")};
        Settings::setColorScheme(QLatin1String(ColorSchemes::DarkPreset));
        Settings::setCornerRadius(Settings::EnumCornerRadius::Large);
        Settings::setSpacingScale(150);
        Settings::setPaneTintColor(QColor(200, 0, 0));
        Settings::setBackgroundBlur(true);
        Settings::setTreeLinePalette({QStringLiteral("#ff0000")});
        Settings::setFileListFont(QStringLiteral("Noto Sans,14,-1,5,400,0,0,0,0,0,0,0,0,0,0,1"));
        Settings::setFolderColorRules(rules);
        Settings::setDimInactivePane(true);

        const QStringList changed = AppearanceDefaults::reset(Settings::self());
        QCOMPARE(changed.size(), 7);
        for (const QString &name : {QStringLiteral("ColorScheme"), QStringLiteral("CornerRadius"), QStringLiteral("FileListFont")}) {
            QVERIFY2(changed.contains(name), qPrintable(name));
        }
        QVERIFY(Settings::colorScheme().isEmpty());
        QCOMPARE(int(Settings::cornerRadius()), int(Settings::EnumCornerRadius::FollowStyle));
        QCOMPARE(Settings::spacingScale(), 100);
        QCOMPARE(Settings::paneTintColor(), QColor(61, 174, 233));
        QVERIFY(!Settings::backgroundBlur());
        QCOMPARE(Settings::treeLinePalette().size(), 6);
        QVERIFY(Settings::fileListFont().isEmpty());
        QCOMPARE(Settings::folderColorRules(), rules);
        QVERIFY(Settings::dimInactivePane());

        // Saving removes the entries, so Plasma's defaults are followed if they change.
        Settings::self()->save();
        const KConfigGroup themes = KSharedConfig::openConfig()->group(QLatin1String(AppearanceDefaults::Group));
        QVERIFY(!themes.hasKey("CornerRadius"));
        QVERIFY(themes.hasKey("FolderColorRules"));

        QVERIFY(AppearanceDefaults::reset(Settings::self()).isEmpty());
    }

    void effectsAreSuspendedOnlyWithHighContrastAndTheSetting()
    {
        auto restore = qScopeGuard([] {
            ColorSchemes::setHighContrastPreferredForTesting(std::nullopt);
        });
        ColorSchemes::setHighContrastPreferredForTesting(false);
        QVERIFY(!AppearanceDefaults::effectsSuspended());
        ColorSchemes::setHighContrastPreferredForTesting(true);
        QVERIFY(Settings::suspendEffectsInHighContrast());
        QVERIFY(AppearanceDefaults::effectsSuspended());
        Settings::setSuspendEffectsInHighContrast(false);
        QVERIFY(!AppearanceDefaults::effectsSuspended());
    }

    void workspaceOverridesAreDisabledOnlyForTheSession()
    {
        QVERIFY(!AppearanceDefaults::workspaceOverridesDisabled());
        AppearanceDefaults::setWorkspaceOverridesDisabled(true);
        QVERIFY(AppearanceDefaults::workspaceOverridesDisabled());
        AppearanceDefaults::setWorkspaceOverridesDisabled(false);
        QVERIFY(!AppearanceDefaults::workspaceOverridesDisabled());
    }
};

QTEST_GUILESS_MAIN(AppearanceDefaultsTest)

#include "appearancedefaultstest.moc"
