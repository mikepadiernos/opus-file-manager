// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspaceappearance.h"
#include "appearancedefaults.h"
#include "effectiveappearance.h"
#include "settings.h"
#include "themeprofiles.h"
#include "workspacecatalog.h"

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace WorkspaceCatalog;

class WorkspaceAppearanceTest : public QObject
{
    Q_OBJECT

private:
    static ThemeProfiles::Profile profile()
    {
        QVariantMap values = ThemeProfiles::defaults(Settings::self());
        values.insert(QStringLiteral("CornerRadius"), int(Settings::EnumCornerRadius::Large));
        values.insert(QStringLiteral("Dividers"), int(Settings::EnumDividers::Thin));
        return {QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("Night"), values};
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
        EffectiveAppearance::setLayer({});
    }

    void catalogPersistsAppearance()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("settings"));
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString id = catalog.workspaces.first().id;
        Appearance appearance;
        appearance.themeProfileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        appearance.overrides = WorkspaceAppearance::encodeOverrides(
            Settings::self(),
            {{QStringLiteral("DefaultViewMode"), int(Settings::EnumDefaultViewMode::Cards)}, {QStringLiteral("BackgroundBlur"), true}});
        QVERIFY2(setAppearance(catalog, id, appearance, error), qPrintable(error));
        catalog.workspaces.first().behaviorSelection.mode = BehaviorMode::Selected;
        catalog.workspaces.first().behaviorSelection.appearance = false;
        QCOMPARE(appearance.overrides.value(QStringLiteral("DefaultViewMode")).toString(), QStringLiteral("Cards"));

        QVERIFY(duplicate(catalog, id, QStringLiteral("Copy"), QStringLiteral("C"), error));
        QCOMPARE(catalog.workspaces.last().appearance, appearance);
        {
            KConfig config(path, KConfig::SimpleConfig);
            QVERIFY2(save(config.group(QStringLiteral("Workspaces")), catalog, error), qPrintable(error));
        }
        KConfig config(path, KConfig::SimpleConfig);
        Catalog loaded;
        QVERIFY2(load(config.group(QStringLiteral("Workspaces")), loaded, error), qPrintable(error));
        QCOMPARE(loaded, catalog);
        QVERIFY(!loaded.workspaces.first().behaviorSelection.appearance);
    }

    void olderCatalogsApplyAttachedAppearanceByDefault()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigGroup group = config.group(QStringLiteral("Workspaces"));
        group.writeEntry("Catalog",
                         QByteArray(R"({"version":7,"workspaces":[{"id":"12f42f08-3054-4da6-8a3c-6ba332887f5c","name":"Work","shortLabel":"W",)"
                                    R"("behaviorSelection":{"mode":1,"locationPresentation":true,"searchScope":false,"layout":true,"paneTabRestoration":true},)"
                                    R"("appearance":{"themeProfileId":"not read before version 8"}}],"activeId":"","locationAssignments":{}})"));
        Catalog catalog;
        QString error;
        QVERIFY2(load(group, catalog, error), qPrintable(error));
        const Workspace &workspace = catalog.workspaces.first();
        QCOMPARE(workspace.behaviorSelection.mode, BehaviorMode::Selected);
        QVERIFY(!workspace.behaviorSelection.searchScope);
        QVERIFY(workspace.behaviorSelection.appearance);
        QVERIFY(workspace.appearance.isEmpty());

        QVERIFY(save(group, catalog, error));
        const QJsonObject saved = QJsonDocument::fromJson(group.readEntry("Catalog", QByteArray())).object();
        QVERIFY(!saved.value(QStringLiteral("workspaces")).toArray().first().toObject().contains(QStringLiteral("appearance")));
    }

    void rejectsMalformedAppearance()
    {
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString id = catalog.workspaces.first().id;
        const Catalog original = catalog;

        QVERIFY(!setAppearance(catalog, id, {QStringLiteral("not-a-uuid"), {}}, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(catalog, original);

        QJsonObject tooMany;
        for (int i = 0; i < 33; ++i) {
            tooMany.insert(QStringLiteral("Option%1").arg(i), true);
        }
        QVERIFY(!setAppearance(catalog, id, {QString(), tooMany}, error));
        QVERIFY(!setAppearance(catalog, id, {QString(), {{QStringLiteral("Dividers"), QJsonObject()}}}, error));
        QVERIFY(!setAppearance(catalog, id, {QString(), {{QStringLiteral("CustomStyleSheet"), QString(4097, QLatin1Char('x'))}}}, error));
        QVERIFY(!setAppearance(catalog, QUuid::createUuid().toString(QUuid::WithoutBraces), {}, error));
        QCOMPARE(catalog, original);
    }

    void keepsOverridesThatCannotBeAppliedNow()
    {
        Catalog catalog;
        QString error;
        QVERIFY(create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QJsonObject overrides{{QStringLiteral("FutureOption"), true},
                                    {QStringLiteral("Dividers"), QStringLiteral("Sparkly")},
                                    {QStringLiteral("BackgroundBlur"), true}};
        QVERIFY2(setAppearance(catalog, catalog.workspaces.first().id, {QString(), overrides}, error), qPrintable(error));
        QCOMPARE(catalog.workspaces.first().appearance.overrides, overrides);

        QStringList problems;
        const QVariantMap decoded = WorkspaceAppearance::decodeOverrides(Settings::self(), overrides, &problems);
        QCOMPARE(decoded, (QVariantMap{{QStringLiteral("BackgroundBlur"), true}}));
        QCOMPARE(problems.size(), 2);

        // Encoding leaves out entries a workspace cannot override.
        const QJsonObject encoded = WorkspaceAppearance::encodeOverrides(
            Settings::self(),
            {{QStringLiteral("ColorScheme"), QStringLiteral("BreezeDark")}, {QStringLiteral("Dividers"), int(Settings::EnumDividers::Hidden)}});
        QCOMPARE(encoded, (QJsonObject{{QStringLiteral("Dividers"), QStringLiteral("Hidden")}}));
    }

    void layersOverridesOverTheProfile()
    {
        const ThemeProfiles::Profile night = profile();
        Workspace workspace;
        workspace.appearance.themeProfileId = night.id;
        workspace.appearance.overrides = WorkspaceAppearance::encodeOverrides(
            Settings::self(),
            {{QStringLiteral("Dividers"), int(Settings::EnumDividers::Hidden)}, {QStringLiteral("FolderColorRules"), QStringList()}});

        const WorkspaceAppearance::Resolved resolved = WorkspaceAppearance::resolve(Settings::self(), &workspace, {night});
        QVERIFY2(resolved.problems.isEmpty(), qPrintable(resolved.problems.join(QLatin1String(", "))));
        QCOMPARE(resolved.values.value(QStringLiteral("CornerRadius")).toInt(), int(Settings::EnumCornerRadius::Large));
        QCOMPARE(resolved.values.value(QStringLiteral("Dividers")).toInt(), int(Settings::EnumDividers::Hidden));
        QVERIFY(resolved.values.contains(QStringLiteral("FolderColorRules")));
        QVERIFY(!resolved.values.contains(QStringLiteral("DefaultViewMode")));
    }

    void reportsMissingProfileAndKeepsOverrides()
    {
        WorkspaceCatalog::Appearance appearance;
        appearance.themeProfileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        appearance.overrides = {{QStringLiteral("BackgroundBlur"), true}};

        const WorkspaceAppearance::Resolved resolved = WorkspaceAppearance::resolve(Settings::self(), appearance, {profile()});
        QCOMPARE(resolved.problems.size(), 1);
        QCOMPARE(resolved.values, (QVariantMap{{QStringLiteral("BackgroundBlur"), true}}));
    }

    void layersNothingWhenAppearanceDoesNotApply()
    {
        const ThemeProfiles::Profile night = profile();
        Workspace workspace;
        workspace.appearance = {night.id, {{QStringLiteral("BackgroundBlur"), true}}};
        QVERIFY(WorkspaceAppearance::resolve(Settings::self(), nullptr, {night}).values.isEmpty());

        workspace.behaviorSelection.mode = BehaviorMode::None;
        QVERIFY(WorkspaceAppearance::resolve(Settings::self(), &workspace, {night}).values.isEmpty());

        workspace.behaviorSelection.mode = BehaviorMode::Selected;
        workspace.behaviorSelection.appearance = false;
        QVERIFY(WorkspaceAppearance::resolve(Settings::self(), &workspace, {night}).values.isEmpty());

        workspace.behaviorSelection.appearance = true;
        QVERIFY(!WorkspaceAppearance::resolve(Settings::self(), &workspace, {night}).values.isEmpty());

        AppearanceDefaults::setWorkspaceOverridesDisabled(true);
        const auto restore = qScopeGuard([] {
            AppearanceDefaults::setWorkspaceOverridesDisabled(false);
        });
        QVERIFY(WorkspaceAppearance::resolve(Settings::self(), &workspace, {night}).values.isEmpty());
        // The editor still resolves the attached appearance for its preview and problems.
        QVERIFY(!WorkspaceAppearance::resolve(Settings::self(), workspace.appearance, {night}).values.isEmpty());
    }

    void effectiveValuesFollowTheLayer()
    {
        Settings::setCornerRadius(Settings::EnumCornerRadius::Small);
        Settings::setBackgroundBlur(false);
        QCOMPARE(int(EffectiveAppearance::cornerRadius()), int(Settings::EnumCornerRadius::Small));
        QCOMPARE(int(EffectiveAppearance::defaultViewMode()), int(Settings::EnumDefaultViewMode::Details));
        QVERIFY(!EffectiveAppearance::backgroundBlur());

        EffectiveAppearance::setLayer({{QStringLiteral("CornerRadius"), int(Settings::EnumCornerRadius::Large)},
                                       {QStringLiteral("DefaultViewMode"), int(Settings::EnumDefaultViewMode::Cards)},
                                       {QStringLiteral("BackgroundBlur"), true}});
        const auto clear = qScopeGuard([] {
            EffectiveAppearance::setLayer({});
        });
        QCOMPARE(int(EffectiveAppearance::cornerRadius()), int(Settings::EnumCornerRadius::Large));
        QCOMPARE(int(EffectiveAppearance::defaultViewMode()), int(Settings::EnumDefaultViewMode::Cards));
        QVERIFY(EffectiveAppearance::backgroundBlur());
        QCOMPARE(EffectiveAppearance::customCornerRadius(), Settings::customCornerRadius());
        // The layer is never written to the global settings.
        QCOMPARE(int(Settings::cornerRadius()), int(Settings::EnumCornerRadius::Small));
        QVERIFY(!Settings::backgroundBlur());
    }
};

QTEST_GUILESS_MAIN(WorkspaceAppearanceTest)

#include "workspaceappearancetest.moc"
