// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacedialog.h"
#include "favoritesmodel.h"
#include "foldersyncprofiles.h"
#include "savedsearches.h"
#include "settings.h"
#include "themeprofiles.h"
#include "workspaceappearance.h"
#include "workspaceappearanceeditor.h"
#include "workspacelocations.h"

#include <KConfig>
#include <KFilePlacesModel>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <memory>

class WorkspaceDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void cleanup()
    {
        Settings::self()->setDefaults();
    }

    void createsRenamesAndDuplicates()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        const WorkspaceCatalog::Layout layout{QByteArray("window"), {200, 300}, {500, 100}, true, false, std::nullopt, {}};
        WorkspaceDialog dialog(
            group,
            [layout](WorkspaceCatalog::Layout &captured, QString &) {
                captured = layout;
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("workspaceName"));
        auto *label = dialog.findChild<QLineEdit *>(QStringLiteral("workspaceShortLabel"));
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *create = dialog.findChild<QPushButton *>(QStringLiteral("workspaceCreate"));
        auto *rename = dialog.findChild<QPushButton *>(QStringLiteral("workspaceRename"));
        auto *duplicate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceDuplicate"));
        auto *behaviorMode = dialog.findChild<QComboBox *>(QStringLiteral("workspaceBehaviorMode"));
        auto *locationPresentation = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceLocationPresentation"));
        auto *saveBehaviors = dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveBehaviors"));
        QVERIFY(name && label && list && create && rename && duplicate && behaviorMode && locationPresentation && saveBehaviors);
        name->setText(QStringLiteral("Work"));
        label->setText(QStringLiteral("W"));
        create->click();
        QCOMPARE(list->count(), 1);
        behaviorMode->setCurrentIndex(1);
        locationPresentation->setChecked(false);
        saveBehaviors->click();
        list->setCurrentRow(0);
        name->setText(QStringLiteral("Office"));
        label->setText(QStringLiteral("O"));
        rename->click();
        name->setText(QStringLiteral("Copy"));
        label->setText(QStringLiteral("C"));
        duplicate->click();

        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::load(group, catalog, error), qPrintable(error));
        QCOMPARE(catalog.workspaces.size(), 2);
        QCOMPARE(catalog.workspaces.at(0).name, QStringLiteral("Office"));
        QCOMPARE(catalog.workspaces.at(1).name, QStringLiteral("Copy"));
        QCOMPARE(catalog.workspaces.at(1).layout, std::optional<WorkspaceCatalog::Layout>(layout));
        QVERIFY(catalog.workspaces.at(0).id != catalog.workspaces.at(1).id);
        QCOMPARE(catalog.workspaces.at(0).behaviorSelection.mode, WorkspaceCatalog::BehaviorMode::Selected);
        QVERIFY(!catalog.workspaces.at(0).behaviorSelection.locationPresentation);
        QVERIFY(!catalog.workspaces.at(1).behaviorSelection.locationPresentation);
    }

    void newWorkspacesUseDefaultBehaviors()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        Settings::setWorkspaceDefaultBehaviorMode(Settings::EnumWorkspaceDefaultBehaviorMode::Selected);
        Settings::setWorkspaceDefaultSearchScope(false);
        Settings::setWorkspaceDefaultPaneTabRestoration(false);
        Settings::setWorkspaceDefaultYaziMode(false);
        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &captured, QString &) {
                captured = {QByteArray("window"), {200, 300}, {500, 100}, true, false, std::nullopt, {}};
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("workspaceName"));
        auto *label = dialog.findChild<QLineEdit *>(QStringLiteral("workspaceShortLabel"));
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *create = dialog.findChild<QPushButton *>(QStringLiteral("workspaceCreate"));
        auto *duplicate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceDuplicate"));
        auto *behaviorMode = dialog.findChild<QComboBox *>(QStringLiteral("workspaceBehaviorMode"));
        auto *searchScope = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceSearchScope"));
        auto *yaziMode = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceYaziModeBehavior"));
        QVERIFY(name && label && list && create && duplicate && behaviorMode && searchScope && yaziMode);
        name->setText(QStringLiteral("Work"));
        label->setText(QStringLiteral("W"));
        create->click();
        QCOMPARE(list->count(), 1);
        list->setCurrentRow(0);
        QCOMPARE(behaviorMode->currentData().toInt(), int(WorkspaceCatalog::BehaviorMode::Selected));
        QVERIFY(!searchScope->isChecked());
        QVERIFY(!yaziMode->isChecked());

        // Duplicates keep the source's behaviors even when the defaults change.
        Settings::setWorkspaceDefaultBehaviorMode(Settings::EnumWorkspaceDefaultBehaviorMode::None);
        name->setText(QStringLiteral("Copy"));
        label->setText(QStringLiteral("C"));
        duplicate->click();
        name->setText(QStringLiteral("Empty"));
        label->setText(QStringLiteral("E"));
        create->click();

        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY2(WorkspaceCatalog::load(group, catalog, error), qPrintable(error));
        QCOMPARE(catalog.workspaces.size(), 3);
        WorkspaceCatalog::BehaviorSelection selected{WorkspaceCatalog::BehaviorMode::Selected, true, false, true, false};
        selected.yaziMode = false;
        QCOMPARE(catalog.workspaces.at(0).behaviorSelection, selected);
        QCOMPARE(catalog.workspaces.at(1).behaviorSelection, selected);
        QCOMPARE(catalog.workspaces.at(2).behaviorSelection.mode, WorkspaceCatalog::BehaviorMode::None);
    }

    void savesWorkspaceAppearance()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        KConfigGroup profilesGroup = KSharedConfig::openConfig()->group(QLatin1String(ThemeProfiles::Group));
        const auto clearProfiles = qScopeGuard([] {
            KSharedConfig::openConfig()->deleteGroup(QLatin1String(ThemeProfiles::Group));
        });
        QList<ThemeProfiles::Profile> profiles{{QString(), QStringLiteral("Night"), ThemeProfiles::capture(Settings::self())}};
        QString error;
        QVERIFY2(ThemeProfiles::save(Settings::self(), profilesGroup, profiles, &error), qPrintable(error));
        const QString profileId = profiles.first().id;
        QVERIFY(!profileId.isEmpty());

        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &captured, QString &) {
                captured = {QByteArray("window"), {200, 300}, {500, 100}, true, false, std::nullopt, {}};
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        auto *name = dialog.findChild<QLineEdit *>(QStringLiteral("workspaceName"));
        auto *label = dialog.findChild<QLineEdit *>(QStringLiteral("workspaceShortLabel"));
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *create = dialog.findChild<QPushButton *>(QStringLiteral("workspaceCreate"));
        auto *profile = dialog.findChild<QComboBox *>(QStringLiteral("workspaceThemeProfile"));
        auto *overrideViewMode = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceOverrideViewMode"));
        auto *viewMode = dialog.findChild<QComboBox *>(QStringLiteral("workspaceViewMode"));
        auto *overrideDividers = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceOverrideDividers"));
        auto *dividers = dialog.findChild<QComboBox *>(QStringLiteral("workspaceDividers"));
        auto *reveal = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceRevealDividers"));
        auto *save = dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveAppearance"));
        auto *appearanceBehavior = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceAppearanceBehavior"));
        QVERIFY(name && label && list && create && profile && overrideViewMode && viewMode && overrideDividers && dividers && reveal && save
                && appearanceBehavior);
        name->setText(QStringLiteral("Work"));
        label->setText(QStringLiteral("W"));
        create->click();
        list->setCurrentRow(0);
        QVERIFY(appearanceBehavior->isChecked());
        QVERIFY(!viewMode->isEnabled());
        using Mode = WorkspaceAppearanceEditor::OverrideMode;
        auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("workspaceOverrideMode"));
        QVERIFY(mode);
        QCOMPARE(mode->currentIndex(), int(Mode::None));
        QVERIFY(!overrideViewMode->isEnabled());
        // Without a window to preview in, there is nothing to preview.
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("workspacePreviewAppearance"))->isVisibleTo(&dialog));
        mode->setCurrentIndex(int(Mode::Selected));
        QVERIFY(overrideViewMode->isEnabled());
        QVERIFY(!viewMode->isEnabled());

        profile->setCurrentIndex(profile->findData(profileId));
        overrideViewMode->setChecked(true);
        QVERIFY(viewMode->isEnabled());
        viewMode->setCurrentIndex(Settings::EnumDefaultViewMode::Cards);
        overrideDividers->setChecked(true);
        dividers->setCurrentIndex(Settings::EnumDividers::Hidden);
        QVERIFY(reveal->isEnabled());
        reveal->setChecked(true);
        save->click();

        WorkspaceCatalog::Catalog catalog;
        QVERIFY2(WorkspaceCatalog::load(group, catalog, error), qPrintable(error));
        const WorkspaceCatalog::Appearance saved = catalog.workspaces.first().appearance;
        QCOMPARE(saved.themeProfileId, profileId);
        QCOMPARE(saved.overrides,
                 (QJsonObject{{QStringLiteral("DefaultViewMode"), QStringLiteral("Cards")},
                              {QStringLiteral("Dividers"), QStringLiteral("Hidden")},
                              {QStringLiteral("RevealDividersOnHover"), true}}));

        // A removed profile stays attached for a retry, and unchecking a row removes its override.
        profilesGroup.deleteGroup();
        list->setCurrentRow(0);
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveBehaviors"))->click();
        QCOMPARE(profile->currentData().toString(), profileId);
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("workspaceAppearanceProblems"))->isVisibleTo(&dialog));
        overrideDividers->setChecked(false);
        save->click();
        QVERIFY2(WorkspaceCatalog::load(group, catalog, error), qPrintable(error));
        QCOMPARE(catalog.workspaces.first().appearance.themeProfileId, profileId);
        QCOMPARE(catalog.workspaces.first().appearance.overrides, (QJsonObject{{QStringLiteral("DefaultViewMode"), QStringLiteral("Cards")}}));
        QCOMPARE(mode->currentIndex(), int(Mode::Selected));

        // All saves every appearance option, keeping the edited ones.
        mode->setCurrentIndex(int(Mode::All));
        QVERIFY(!overrideDividers->isEnabled());
        QVERIFY(dividers->isEnabled());
        QVERIFY(viewMode->isEnabled());
        save->click();
        QVERIFY2(WorkspaceCatalog::load(group, catalog, error), qPrintable(error));
        const QJsonObject all = catalog.workspaces.first().appearance.overrides;
        QCOMPARE(all.size(), WorkspaceAppearance::overrideEntries(Settings::self()).size());
        QCOMPARE(all.value(QStringLiteral("DefaultViewMode")).toString(), QStringLiteral("Cards"));
        QCOMPARE(all.value(QStringLiteral("WindowOpacity")).toInt(), Settings::windowOpacity());
        QCOMPARE(mode->currentIndex(), int(Mode::All));

        // None removes every saved option but keeps the theme profile.
        mode->setCurrentIndex(int(Mode::None));
        QVERIFY(!viewMode->isEnabled());
        save->click();
        QVERIFY2(WorkspaceCatalog::load(group, catalog, error), qPrintable(error));
        QVERIFY(catalog.workspaces.first().appearance.overrides.isEmpty());
        QCOMPARE(catalog.workspaces.first().appearance.themeProfileId, profileId);
        QCOMPARE(mode->currentIndex(), int(Mode::None));
    }

    void previewsAppearanceUntilRevertedOrClosed()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        QList<std::optional<WorkspaceCatalog::Appearance>> previews;
        {
            WorkspaceDialog dialog(
                group,
                [](WorkspaceCatalog::Layout &, QString &) {
                    return true;
                },
                [](const WorkspaceCatalog::Layout &, QString &) {
                    return true;
                },
                QString());
            dialog.setAppearancePreview([&previews](const WorkspaceCatalog::Appearance *appearance) {
                previews.append(appearance ? std::optional(*appearance) : std::nullopt);
            });
            auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
            auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("workspaceOverrideMode"));
            auto *overrideBlur = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceOverrideBlur"));
            auto *blur = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceBlur"));
            auto *preview = dialog.findChild<QPushButton *>(QStringLiteral("workspacePreviewAppearance"));
            auto *revert = dialog.findChild<QPushButton *>(QStringLiteral("workspaceRevertAppearance"));
            QVERIFY(list && mode && overrideBlur && blur && preview && revert);
            QVERIFY(preview->isVisibleTo(&dialog));
            QVERIFY(!revert->isEnabled());
            list->setCurrentRow(0);
            mode->setCurrentIndex(int(WorkspaceAppearanceEditor::OverrideMode::Selected));
            overrideBlur->setChecked(true);
            blur->setChecked(true);
            preview->click();
            QCOMPARE(previews.size(), 1);
            QVERIFY(previews.last());
            QCOMPARE(previews.last()->overrides, (QJsonObject{{QStringLiteral("BackgroundBlur"), true}}));
            QVERIFY(revert->isEnabled());
            revert->click();
            QCOMPARE(previews.size(), 2);
            QVERIFY(!previews.last());
            QVERIFY(!revert->isEnabled());

            // Showing another workspace reverts the preview, and nothing was saved.
            preview->click();
            list->setCurrentRow(1);
            QCOMPARE(previews.size(), 4);
            QVERIFY(!previews.last());
            QVERIFY2(WorkspaceCatalog::load(group, catalog, error), qPrintable(error));
            QVERIFY(catalog.workspaces.first().appearance.isEmpty());

            // Closing the dialog reverts it too.
            list->setCurrentRow(0);
            preview->click();
            QCOMPARE(previews.size(), 5);
            dialog.reject();
            QCOMPARE(previews.size(), 6);
            QVERIFY(!previews.last());
        }
        QCOMPARE(previews.size(), 6);
    }

    void reportsUnavailableTranslucency()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        Settings::setWindowOpacity(80);
        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *mode = dialog.findChild<QComboBox *>(QStringLiteral("workspaceOverrideMode"));
        auto *problems = dialog.findChild<QLabel *>(QStringLiteral("workspaceAppearanceProblems"));
        QVERIFY(list && mode && problems);
        list->setCurrentRow(0);
        // The global translucency is reported by the settings, not for each workspace.
        QVERIFY(!problems->isVisibleTo(&dialog));
        // An offscreen dialog is not translucent, so the opacity saved with the workspace cannot be shown.
        mode->setCurrentIndex(int(WorkspaceAppearanceEditor::OverrideMode::All));
        QVERIFY(problems->isVisibleTo(&dialog));
        QVERIFY2(problems->text().contains(QLatin1String("compositing")), qPrintable(problems->text()));
    }

    void switchesAndDeletesWithoutChangingFiles()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("note.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("unchanged"), qint64(9));
        file.close();
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        const QString workId = catalog.workspaces.first().id;
        const QString homeId = catalog.workspaces.last().id;
        const WorkspaceCatalog::Layout work{QByteArray("work"), {100, 200}, {300, 400}, true, false, std::nullopt, {}};
        const WorkspaceCatalog::Layout home{QByteArray("home"), {200, 300}, {400, 500}, false, true, std::nullopt, {}};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, workId, work, error));
        QVERIFY(WorkspaceCatalog::setLayout(catalog, homeId, home, error));
        QVERIFY(WorkspaceCatalog::activate(catalog, workId, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        WorkspaceCatalog::Layout unsaved = work;
        unsaved.paneSizes = {150, 150};
        WorkspaceCatalog::Layout current = unsaved;
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&current](const WorkspaceCatalog::Layout &layout, QString &) {
                current = layout;
                return true;
            },
            workId);
        QSignalSpy changed(&dialog, &WorkspaceDialog::currentWorkspaceChanged);
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *activate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"));
        auto *remove = dialog.findChild<QPushButton *>(QStringLiteral("workspaceDelete"));
        QVERIFY(list && activate && remove);
        list->setCurrentRow(1);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::No)->click();
        });
        activate->click();
        QCOMPARE(current, unsaved);
        QCOMPARE(changed.size(), 0);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        activate->click();
        QCOMPARE(current, home);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().first().toString(), homeId);
        auto *save = dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveLayout"));
        QVERIFY(save);
        current.dual = true;
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::No)->click();
        });
        save->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.last().layout, std::optional<WorkspaceCatalog::Layout>(home));
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        save->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.last().layout, std::optional<WorkspaceCatalog::Layout>(current));
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::No)->click();
        });
        remove->click();
        QCOMPARE(list->count(), 2);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        remove->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.size(), 1);
        QVERIFY(catalog.activeId.isEmpty());
        QCOMPARE(changed.size(), 2);
        QVERIFY(changed.last().first().toString().isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("unchanged"));
    }

    void selectedWorkspaceBehaviorsCanSkipLayoutChanges()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        auto &workspace = catalog.workspaces.first();
        workspace.layout = WorkspaceCatalog::Layout{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, {}};
        workspace.behaviorSelection = {WorkspaceCatalog::BehaviorMode::Selected, false, true, false, false};
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));

        bool captured = false;
        bool applied = false;
        WorkspaceDialog dialog(
            group,
            [&captured](WorkspaceCatalog::Layout &, QString &) {
                captured = true;
                return true;
            },
            [&applied](const WorkspaceCatalog::Layout &, QString &) {
                applied = true;
                return true;
            },
            QString());
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *activate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"));
        QVERIFY(list && activate);
        list->setCurrentRow(0);
        activate->click();

        QVERIFY(!captured);
        QVERIFY(!applied);
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("workspaceStatus"))->text().isEmpty());
        WorkspaceCatalog::Catalog stored;
        QVERIFY(WorkspaceCatalog::load(group, stored, error));
        QCOMPARE(stored.activeId, workspace.id);
    }

    void selectedWorkspaceLayoutKeepsUncheckedPaneSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString workspaceId = catalog.workspaces.first().id;
        WindowSession savedSession;
        savedSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved"))};
        savedSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved-right"))};
        WorkspaceCatalog::Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, savedSession};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, workspaceId, saved, error));
        catalog.workspaces.first().behaviorSelection = {WorkspaceCatalog::BehaviorMode::Selected, false, false, true, false};
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));

        WindowSession currentSession;
        currentSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/current"))};
        currentSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/current-right"))};
        WorkspaceCatalog::Layout current{QByteArray("current"), {8, 9}, {10, 11}, false, true, std::nullopt, currentSession};
        WorkspaceCatalog::Layout applied;
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&applied](const WorkspaceCatalog::Layout &layout, QString &) {
                applied = layout;
                return true;
            },
            QString());
        dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();

        QCOMPARE(applied.windowState, saved.windowState);
        QVERIFY(applied.session);
        QCOMPARE(applied.session->left.tabs, currentSession.left.tabs);
        QCOMPARE(applied.session->right.tabs, currentSession.right.tabs);
        QCOMPARE(applied.session->activePane, currentSession.activePane);
        QCOMPARE(applied.paneSizes, saved.paneSizes);
        QCOMPARE(applied.workspaceSizes, saved.workspaceSizes);
    }

    void layoutRestoreOptionsKeepCurrentParts()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        WindowSession savedSession;
        savedSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved"))};
        savedSession.left.viewMode = 2;
        savedSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved-right"))};
        WorkspaceCatalog::Presentation savedPresentation;
        savedPresentation.toolbarActions = QStringList{QStringLiteral("saved_action")};
        savedPresentation.previewVisible = true;
        const WorkspaceCatalog::Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, savedPresentation, savedSession};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, catalog.workspaces.first().id, saved, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));

        WindowSession currentSession;
        currentSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/current"))};
        currentSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/current-right"))};
        WorkspaceCatalog::Presentation currentPresentation;
        currentPresentation.toolbarActions = QStringList{QStringLiteral("current_action")};
        const WorkspaceCatalog::Layout current{QByteArray("current"), {8, 9}, {10, 11}, false, true, currentPresentation, currentSession};
        WorkspaceCatalog::Layout applied;
        Settings::setWorkspaceRestoreViews(false);
        Settings::setWorkspaceRestoreToolbars(false);
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&applied](const WorkspaceCatalog::Layout &layout, QString &) {
                applied = layout;
                return true;
            },
            QString());
        dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();
        Settings::self()->setDefaults();

        QCOMPARE(applied.windowState, saved.windowState);
        QCOMPARE(applied.paneSizes, saved.paneSizes);
        QVERIFY(applied.presentation && applied.session);
        QCOMPARE(applied.presentation->toolbarActions, currentPresentation.toolbarActions);
        QVERIFY(applied.presentation->previewVisible);
        QCOMPARE(applied.session->left.tabs, savedSession.left.tabs);
        QCOMPARE(applied.session->left.viewMode, 0);
    }

    void yaziModeBehaviorIsSavedAndKeepsLegacyWorkspaces()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Current"), QStringLiteral("C"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Legacy"), QStringLiteral("L"), error));
        catalog.workspaces[1].behaviorSelection.yaziMode = std::nullopt;
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        const QString currentId = catalog.workspaces.at(0).id;
        const QString legacyId = catalog.workspaces.at(1).id;

        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *yaziMode = dialog.findChild<QCheckBox *>(QStringLiteral("workspaceYaziModeBehavior"));
        auto *saveBehaviors = dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveBehaviors"));
        QVERIFY(list && yaziMode && saveBehaviors);
        QCOMPARE(yaziMode->accessibleName(), QStringLiteral("Yazi mode"));
        QVERIFY(!yaziMode->accessibleDescription().isEmpty());
        QVERIFY(!yaziMode->toolTip().isEmpty());
        const auto selectWorkspace = [list](const QString &name) {
            for (int row = 0; row < list->count(); ++row) {
                if (list->item(row)->text().contains(name)) {
                    list->setCurrentRow(row);
                    return true;
                }
            }
            return false;
        };
        const auto stored = [&group](const QString &id) {
            WorkspaceCatalog::Catalog loaded;
            QString loadError;
            [&] {
                QVERIFY2(WorkspaceCatalog::load(group, loaded, loadError), qPrintable(loadError));
            }();
            const auto it = std::find_if(loaded.workspaces.cbegin(), loaded.workspaces.cend(), [&id](const auto &workspace) {
                return workspace.id == id;
            });
            return it == loaded.workspaces.cend() ? std::optional<bool>() : it->behaviorSelection.yaziMode;
        };

        QVERIFY(selectWorkspace(QStringLiteral("Current")));
        QVERIFY(yaziMode->isChecked());
        yaziMode->setChecked(false);
        saveBehaviors->click();
        QCOMPARE(stored(currentId), std::optional<bool>(false));
        QVERIFY(selectWorkspace(QStringLiteral("Current")));
        QVERIFY(!yaziMode->isChecked());

        // Saving other behaviors keeps a legacy workspace restoring Yazi mode with the views.
        QVERIFY(selectWorkspace(QStringLiteral("Legacy")));
        QVERIFY(yaziMode->isChecked());
        saveBehaviors->click();
        QCOMPARE(stored(legacyId), std::optional<bool>());
        QVERIFY(selectWorkspace(QStringLiteral("Legacy")));
        yaziMode->setChecked(false);
        saveBehaviors->click();
        QCOMPARE(stored(legacyId), std::optional<bool>(false));
    }

    void yaziModeBehaviorRestoresOrKeepsEachPanesYaziMode()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        WindowSession savedSession;
        savedSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved"))};
        savedSession.left.viewMode = 2;
        savedSession.left.yaziMode = true;
        savedSession.left.yaziPreview = 0;
        savedSession.left.yaziPreviewContent = 1;
        savedSession.left.yaziParentColumn = 0;
        savedSession.left.yaziColumnRatios = {2, 5, 3};
        savedSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved-right"))};
        const WorkspaceCatalog::Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, savedSession};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, catalog.workspaces.first().id, saved, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));

        WindowSession currentSession;
        currentSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/current"))};
        currentSession.left.yaziColumnRatios = {1, 1, 1};
        currentSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/current-right"))};
        currentSession.right.yaziMode = true;
        const WorkspaceCatalog::Layout current{QByteArray("current"), {8, 9}, {10, 11}, false, true, std::nullopt, currentSession};

        for (const bool restoreYazi : {true, false}) {
            WorkspaceCatalog::Catalog stored;
            QVERIFY(WorkspaceCatalog::load(group, stored, error));
            stored.workspaces.first().behaviorSelection.mode = WorkspaceCatalog::BehaviorMode::Selected;
            stored.workspaces.first().behaviorSelection.yaziMode = restoreYazi;
            QVERIFY(WorkspaceCatalog::save(group, stored, error));
            // The Yazi mode behavior applies even when the view restore option is off.
            Settings::setWorkspaceRestoreViews(false);
            WorkspaceCatalog::Layout applied;
            WorkspaceDialog dialog(
                group,
                [&current](WorkspaceCatalog::Layout &layout, QString &) {
                    layout = current;
                    return true;
                },
                [&applied](const WorkspaceCatalog::Layout &layout, QString &) {
                    applied = layout;
                    return true;
                },
                QString());
            dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
            QTimer::singleShot(0, [] {
                qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
            });
            dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();
            Settings::self()->setDefaults();

            QVERIFY(applied.session);
            QCOMPARE(applied.session->left.tabs, savedSession.left.tabs);
            QCOMPARE(applied.session->left.viewMode, 0);
            const PaneSession &expectedLeft = restoreYazi ? savedSession.left : currentSession.left;
            const PaneSession &expectedRight = restoreYazi ? savedSession.right : currentSession.right;
            QCOMPARE(applied.session->left.yaziMode, expectedLeft.yaziMode);
            QCOMPARE(applied.session->left.yaziPreview, expectedLeft.yaziPreview);
            QCOMPARE(applied.session->left.yaziPreviewContent, expectedLeft.yaziPreviewContent);
            QCOMPARE(applied.session->left.yaziParentColumn, expectedLeft.yaziParentColumn);
            QCOMPARE(applied.session->left.yaziColumnRatios, expectedLeft.yaziColumnRatios);
            QCOMPARE(applied.session->right.yaziMode, expectedRight.yaziMode);
        }
    }

    void yaziRestoreOptionsKeepCurrentYaziStateWithoutChangingTheWorkspace()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString workspaceId = catalog.workspaces.first().id;
        WindowSession savedSession;
        savedSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved"))};
        savedSession.left.viewMode = 2;
        savedSession.left.yaziMode = true;
        savedSession.left.yaziPreview = 0;
        savedSession.left.yaziPreviewContent = 1;
        savedSession.left.yaziParentColumn = 0;
        savedSession.left.yaziColumnRatios = {2, 5, 3};
        savedSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/saved-right"))};
        // A stored session's arrangement is the layout's.
        savedSession.dual = true;
        savedSession.stacked = false;
        const WorkspaceCatalog::Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, savedSession};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, workspaceId, saved, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));

        WindowSession currentSession;
        currentSession.left.tabs = {QUrl::fromLocalFile(QStringLiteral("/current"))};
        currentSession.left.yaziColumnRatios = {1, 1, 1};
        currentSession.right.tabs = {QUrl::fromLocalFile(QStringLiteral("/current-right"))};
        currentSession.right.yaziMode = true;
        currentSession.right.yaziPreview = 1;
        currentSession.dual = false;
        currentSession.stacked = true;
        const WorkspaceCatalog::Layout current{QByteArray("current"), {8, 9}, {10, 11}, false, true, std::nullopt, currentSession};

        const auto load = [&group] {
            WorkspaceCatalog::Catalog loaded;
            QString loadError;
            [&] {
                QVERIFY2(WorkspaceCatalog::load(group, loaded, loadError), qPrintable(loadError));
                QCOMPARE(loaded.workspaces.size(), 1);
            }();
            return loaded;
        };
        // Activates the workspace with the given Yazi options and returns the applied layout, if any.
        const auto activate = [&](bool restoreMode, bool restoreColumns) {
            Settings::setYaziWorkspaceRestoreMode(restoreMode);
            Settings::setYaziWorkspaceRestoreColumns(restoreColumns);
            Settings::setConfirmWorkspaceSwitch(Settings::EnumConfirmWorkspaceSwitch::Never);
            std::optional<WorkspaceCatalog::Layout> applied;
            WorkspaceDialog dialog(
                group,
                [&current](WorkspaceCatalog::Layout &layout, QString &) {
                    layout = current;
                    return true;
                },
                [&applied](const WorkspaceCatalog::Layout &layout, QString &) {
                    applied = layout;
                    return true;
                },
                QString());
            dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
            dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();
            return applied;
        };
        const auto verifyStoredWorkspace = [&](const WorkspaceCatalog::BehaviorSelection &selection) {
            const WorkspaceCatalog::Catalog stored = load();
            QCOMPARE(stored.workspaces.first().layout, std::optional<WorkspaceCatalog::Layout>(saved));
            QCOMPARE(stored.workspaces.first().behaviorSelection, selection);
            QCOMPARE(stored.activeId, workspaceId);
        };
        const WorkspaceCatalog::BehaviorSelection allBehaviors = load().workspaces.first().behaviorSelection;

        // Without the mode, each pane keeps its mode and takes the saved columns, views and tabs.
        auto applied = activate(false, true);
        QVERIFY(applied && applied->session);
        QCOMPARE(applied->session->left.tabs, savedSession.left.tabs);
        QCOMPARE(applied->session->left.viewMode, 2);
        QVERIFY(!applied->session->left.yaziMode);
        QVERIFY(applied->session->right.yaziMode);
        QCOMPARE(applied->session->left.yaziPreview, 0);
        QCOMPARE(applied->session->left.yaziPreviewContent, 1);
        QCOMPARE(applied->session->left.yaziParentColumn, 0);
        QCOMPARE(applied->session->left.yaziColumnRatios, QList<int>({2, 5, 3}));
        QCOMPARE(applied->session->right.yaziPreview, -1);
        QCOMPARE(applied->windowState, saved.windowState);
        verifyStoredWorkspace(allBehaviors);

        // Without either, all of each pane's Yazi state is kept and everything else restored.
        applied = activate(false, false);
        QVERIFY(applied && applied->session);
        QCOMPARE(applied->session->left.tabs, savedSession.left.tabs);
        QCOMPARE(applied->session->right.tabs, savedSession.right.tabs);
        QCOMPARE(applied->session->left.viewMode, 2);
        for (const auto &[result, expected] :
             {qMakePair(applied->session->left, currentSession.left), qMakePair(applied->session->right, currentSession.right)}) {
            QCOMPARE(result.yaziMode, expected.yaziMode);
            QCOMPARE(result.yaziPreview, expected.yaziPreview);
            QCOMPARE(result.yaziPreviewContent, expected.yaziPreviewContent);
            QCOMPARE(result.yaziParentColumn, expected.yaziParentColumn);
            QCOMPARE(result.yaziColumnRatios, expected.yaziColumnRatios);
        }
        verifyStoredWorkspace(allBehaviors);

        // A workspace that restores only Yazi mode then has nothing to restore, but still becomes active.
        WorkspaceCatalog::Catalog stored = load();
        const WorkspaceCatalog::BehaviorSelection yaziOnly{WorkspaceCatalog::BehaviorMode::Selected, false, false, false, false, false, true};
        stored.workspaces.first().behaviorSelection = yaziOnly;
        stored.activeId.clear();
        QVERIFY(WorkspaceCatalog::save(group, stored, error));
        applied = activate(false, false);
        QVERIFY(!applied);
        verifyStoredWorkspace(yaziOnly);
        applied = activate(true, false);
        QVERIFY(applied && applied->session);
        QVERIFY(applied->session->left.yaziMode);
        QCOMPARE(applied->session->left.yaziColumnRatios, currentSession.left.yaziColumnRatios);
        QCOMPARE(applied->session->left.tabs, currentSession.left.tabs);
        verifyStoredWorkspace(yaziOnly);

        // Saving a layout records each pane's Yazi state whatever the options say.
        Settings::setYaziWorkspaceRestoreMode(false);
        Settings::setYaziWorkspaceRestoreColumns(false);
        Settings::setConfirmWorkspaceLayoutOverwrite(false);
        {
            WorkspaceDialog dialog(
                group,
                [&current](WorkspaceCatalog::Layout &layout, QString &) {
                    layout = current;
                    return true;
                },
                [](const WorkspaceCatalog::Layout &, QString &) {
                    return true;
                },
                workspaceId);
            dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
            dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveLayout"))->click();
        }
        stored = load();
        QCOMPARE(stored.workspaces.first().layout, std::optional<WorkspaceCatalog::Layout>(current));
        QVERIFY(stored.workspaces.first().layout->session->right.yaziMode);
        QCOMPARE(stored.workspaces.first().layout->session->right.yaziPreview, 1);
        QCOMPARE(stored.workspaces.first().behaviorSelection, yaziOnly);
    }

    void layoutOverwriteConfirmationCanBeSkipped()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        const WorkspaceCatalog::Layout current{QByteArray("current"), {8, 9}, {10, 11}, false, true, std::nullopt, std::nullopt};
        Settings::setConfirmWorkspaceLayoutOverwrite(false);
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"))->setCurrentRow(0);
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveLayout"))->click();
        Settings::self()->setDefaults();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.first().layout, std::optional<WorkspaceCatalog::Layout>(current));
    }

    void switchConfirmationFollowsSettingAndUnsavedState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        const QString workId = catalog.workspaces.first().id;
        const WorkspaceCatalog::Layout work{QByteArray("work"), {100, 200}, {300, 400}, true, false, std::nullopt, {}};
        const WorkspaceCatalog::Layout home{QByteArray("home"), {200, 300}, {400, 500}, false, true, std::nullopt, {}};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, workId, work, error));
        QVERIFY(WorkspaceCatalog::setLayout(catalog, catalog.workspaces.last().id, home, error));
        QVERIFY(WorkspaceCatalog::activate(catalog, workId, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        WorkspaceCatalog::Layout current = work;
        WorkspaceDialog dialog(
            group,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&current](const WorkspaceCatalog::Layout &layout, QString &) {
                current = layout;
                return true;
            },
            workId);
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *activate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"));
        const auto activateRow = [&](int row, QMessageBox::StandardButton answer) {
            auto prompted = std::make_shared<bool>(false);
            QTimer::singleShot(0, [prompted, answer] {
                if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                    *prompted = true;
                    box->button(answer)->click();
                }
            });
            list->setCurrentRow(row);
            activate->click();
            QCoreApplication::processEvents();
            return *prompted;
        };

        // The default asks only when the current state is not saved in the active workspace.
        QVERIFY(!activateRow(1, QMessageBox::No));
        QCOMPARE(current, home);
        current.paneSizes = {1, 1};
        QVERIFY(activateRow(0, QMessageBox::No));
        QCOMPARE(current.paneSizes, (QList<int>{1, 1}));

        Settings::setConfirmWorkspaceSwitch(Settings::EnumConfirmWorkspaceSwitch::Never);
        QVERIFY(!activateRow(0, QMessageBox::No));
        QCOMPARE(current, work);

        Settings::setConfirmWorkspaceSwitch(Settings::EnumConfirmWorkspaceSwitch::Always);
        QVERIFY(activateRow(1, QMessageBox::No));
        QCOMPARE(current, work);
        Settings::self()->setDefaults();
    }

    void recoveryPromptRetriesAndKeepsSavedReferences()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const WorkspaceCatalog::Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, std::nullopt};
        QVERIFY(WorkspaceCatalog::setLayout(catalog, catalog.workspaces.first().id, saved, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        Settings::setConfirmWorkspaceSwitch(Settings::EnumConfirmWorkspaceSwitch::Never);
        int applied = 0;
        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &layout, QString &) {
                layout = {};
                return true;
            },
            [&applied](const WorkspaceCatalog::Layout &, QString &) {
                ++applied;
                return true;
            },
            QString());
        int checks = 0;
        QStringList missing{QStringLiteral("/missing")};
        dialog.setAvailabilityCheck([&](const WorkspaceCatalog::Layout &) {
            ++checks;
            return missing;
        });
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *activate = dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"));
        list->setCurrentRow(0);
        const auto answer = [](const std::function<void(QMessageBox *)> &respond) {
            auto prompted = std::make_shared<bool>(false);
            QTimer::singleShot(0, [prompted, respond] {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (box && box->objectName() == QLatin1String("workspaceUnavailableItems")) {
                    *prompted = true;
                    QVERIFY(box->informativeText().contains(QStringLiteral("/missing")));
                    respond(box);
                }
            });
            return prompted;
        };

        auto prompted = answer([](QMessageBox *box) {
            box->button(QMessageBox::Cancel)->click();
        });
        activate->click();
        QVERIFY(*prompted);
        QCOMPARE(applied, 0);
        QCOMPARE(checks, 1);

        prompted = answer([&missing](QMessageBox *box) {
            missing.clear();
            box->button(QMessageBox::Retry)->click();
        });
        activate->click();
        QVERIFY(*prompted);
        QCOMPARE(checks, 3);
        QCOMPARE(applied, 1);

        missing = {QStringLiteral("/missing")};
        prompted = answer([](QMessageBox *box) {
            const auto buttons = box->buttons();
            for (QAbstractButton *button : buttons) {
                if (box->buttonRole(button) == QMessageBox::AcceptRole) {
                    button->click();
                }
            }
        });
        activate->click();
        QVERIFY(*prompted);
        QCOMPARE(applied, 2);

        Settings::setWorkspaceRecoveryPrompt(false);
        prompted = answer([](QMessageBox *box) {
            box->button(QMessageBox::Cancel)->click();
        });
        activate->click();
        QCoreApplication::processEvents();
        QVERIFY(!*prompted);
        QCOMPARE(applied, 3);

        WorkspaceCatalog::Catalog stored;
        QVERIFY(WorkspaceCatalog::load(group, stored, error));
        QCOMPARE(stored.workspaces.first().layout, std::optional<WorkspaceCatalog::Layout>(saved));
    }

    void restoresPreviousLayoutWhenCatalogChangesDuringSwitch()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings"));
        KConfig first(path, KConfig::SimpleConfig);
        KConfig second(path, KConfig::SimpleConfig);
        const auto firstGroup = first.group(QStringLiteral("Workspaces"));
        const auto secondGroup = second.group(QStringLiteral("Workspaces"));
        WorkspaceCatalog::Catalog saved;
        QString error;
        QVERIFY(WorkspaceCatalog::create(saved, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString id = saved.workspaces.first().id;
        WorkspaceCatalog::Layout previous;
        WorkspaceCatalog::Layout target;
        target.stacked = false;
        QVERIFY(WorkspaceCatalog::setLayout(saved, id, target, error));
        QVERIFY(WorkspaceCatalog::save(firstGroup, saved, error));
        WorkspaceCatalog::Layout current = previous;
        int applications = 0;
        WorkspaceDialog dialog(
            firstGroup,
            [&current](WorkspaceCatalog::Layout &layout, QString &) {
                layout = current;
                return true;
            },
            [&](const WorkspaceCatalog::Layout &layout, QString &failure) {
                current = layout;
                if (++applications == 1) {
                    second.reparseConfiguration();
                    WorkspaceCatalog::Catalog external;
                    if (!WorkspaceCatalog::load(secondGroup, external, failure)
                        || !WorkspaceCatalog::rename(external, id, QStringLiteral("Changed elsewhere"), QStringLiteral("C"), failure)
                        || !WorkspaceCatalog::save(secondGroup, external, failure)) {
                        return false;
                    }
                }
                return true;
            },
            QString());
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        list->setCurrentRow(0);
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceActivate"))->click();
        QCOMPARE(applications, 2);
        QCOMPARE(current, previous);
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("workspaceStatus"))->text().isEmpty());
        WorkspaceCatalog::Catalog stored;
        QVERIFY(WorkspaceCatalog::load(firstGroup, stored, error));
        QCOMPARE(stored.workspaces.first().name, QStringLiteral("Changed elsewhere"));
        QVERIFY(stored.activeId.isEmpty());
    }

    void reportsConflictsAndRetainsOtherChanges()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("settings"));
        KConfig first(path, KConfig::SimpleConfig);
        KConfig second(path, KConfig::SimpleConfig);
        const auto firstGroup = first.group(QStringLiteral("Workspaces"));
        const auto secondGroup = second.group(QStringLiteral("Workspaces"));
        WorkspaceDialog dialog(
            firstGroup,
            [](WorkspaceCatalog::Layout &layout, QString &) {
                layout = {};
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            QString());
        WorkspaceCatalog::Catalog external;
        QString error;
        QVERIFY(WorkspaceCatalog::create(external, QStringLiteral("Other"), QStringLiteral("O"), error));
        QVERIFY(WorkspaceCatalog::save(secondGroup, external, error));
        dialog.findChild<QLineEdit *>(QStringLiteral("workspaceName"))->setText(QStringLiteral("Work"));
        dialog.findChild<QLineEdit *>(QStringLiteral("workspaceShortLabel"))->setText(QStringLiteral("W"));
        dialog.findChild<QPushButton *>(QStringLiteral("workspaceCreate"))->click();
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("workspaceStatus"))->text().isEmpty());
        QVERIFY(WorkspaceCatalog::load(firstGroup, external, error));
        QCOMPARE(external.workspaces.size(), 1);
        QCOMPARE(external.workspaces.first().name, QStringLiteral("Other"));
    }

    void linksProfilesAndRetainsMissingReferences()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        QString error;
        SavedSearches::Search search{QStringLiteral("PDFs"), QStringLiteral("*.pdf"), {}, LiveSearch::Field::FileName, true, {}, {}};
        QVERIFY(SavedSearches::save(config.group(QStringLiteral("SavedSearches")), {search}, &error));
        search = SavedSearches::load(config.group(QStringLiteral("SavedSearches")), &error).first();
        FolderSyncProfiles::Profile profile;
        profile.name = QStringLiteral("Photos");
        QVERIFY(FolderSyncProfiles::save(config.group(QStringLiteral("FolderSyncProfiles")), {profile}, &error));
        profile = FolderSyncProfiles::load(config.group(QStringLiteral("FolderSyncProfiles")), &error).first();

        WorkspaceCatalog::Catalog catalog;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::activate(catalog, catalog.workspaces.first().id, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            catalog.activeId);
        auto *workspaces = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *searches = dialog.findChild<QListWidget *>(QStringLiteral("workspaceSavedSearches"));
        auto *profiles = dialog.findChild<QListWidget *>(QStringLiteral("workspaceFolderSyncProfiles"));
        auto *save = dialog.findChild<QPushButton *>(QStringLiteral("workspaceSaveProfileLinks"));
        QVERIFY(workspaces && searches && profiles && save);
        workspaces->setCurrentRow(0);
        QCOMPARE(searches->count(), 1);
        QCOMPARE(profiles->count(), 1);
        searches->item(0)->setCheckState(Qt::Checked);
        profiles->item(0)->setCheckState(Qt::Checked);
        save->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.first().savedSearchIds, QStringList{search.id});
        QCOMPARE(catalog.workspaces.first().folderSyncProfileIds, QStringList{profile.id});

        QVERIFY(SavedSearches::save(config.group(QStringLiteral("SavedSearches")), {}, &error));
        QVERIFY(FolderSyncProfiles::save(config.group(QStringLiteral("FolderSyncProfiles")), {}, &error));
        dialog.refreshCatalog();
        QCOMPARE(searches->count(), 1);
        QCOMPARE(profiles->count(), 1);
        QVERIFY(searches->item(0)->text().contains(QStringLiteral("Missing saved search")));
        QVERIFY(profiles->item(0)->text().contains(QStringLiteral("Missing profile")));
        QCOMPARE(searches->item(0)->data(Qt::UserRole).toString(), search.id);
        QCOMPARE(profiles->item(0)->data(Qt::UserRole).toString(), profile.id);
        save->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.workspaces.first().savedSearchIds, QStringList{search.id});
        QCOMPARE(catalog.workspaces.first().folderSyncProfileIds, QStringList{profile.id});
    }

    void assignsReassignsAndUnassignsSavedLocations()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        FavoritesModel favorites(config.group(QStringLiteral("Favorites")));
        KFilePlacesModel places;
        const QUrl url = QUrl::fromLocalFile(directory.filePath(QStringLiteral("folder")));
        QVERIFY(favorites.add(url));
        WorkspaceLocations locations(&favorites, &places);
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Home"), QStringLiteral("H"), error));
        const QString workId = catalog.workspaces.first().id;
        const QString homeId = catalog.workspaces.last().id;
        const QString locationId = WorkspaceCatalog::locationId(url);
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            workId,
            nullptr,
            &locations);
        QSignalSpy changed(&dialog, &WorkspaceDialog::catalogChanged);
        auto *workspaces = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *savedLocations = dialog.findChild<QListWidget *>(QStringLiteral("workspaceLocationList"));
        auto *assign = dialog.findChild<QPushButton *>(QStringLiteral("workspaceAssignLocation"));
        auto *unassign = dialog.findChild<QPushButton *>(QStringLiteral("workspaceUnassignLocation"));
        QVERIFY(workspaces && savedLocations && assign && unassign);
        auto selectLocation = [&] {
            for (int row = 0; row < savedLocations->count(); ++row) {
                if (savedLocations->item(row)->data(Qt::UserRole).toString() == locationId) {
                    savedLocations->setCurrentRow(row);
                    return true;
                }
            }
            return false;
        };
        workspaces->setCurrentRow(0);
        QVERIFY(selectLocation());
        assign->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.locationAssignments.value(locationId), workId);
        QCOMPARE(changed.size(), 1);
        workspaces->setCurrentRow(1);
        QVERIFY(selectLocation());
        QTimer::singleShot(0, [] {
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())->button(QMessageBox::Yes)->click();
        });
        assign->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QCOMPARE(catalog.locationAssignments.value(locationId), homeId);
        QVERIFY(selectLocation());
        unassign->click();
        QVERIFY(WorkspaceCatalog::load(group, catalog, error));
        QVERIFY(!catalog.locationAssignments.contains(locationId));
        QCOMPARE(favorites.urls(), QList<QUrl>({url}));
    }

    void usesAnAssignedLocalLocationAsTheEditorVault()
    {
        QTemporaryDir directory;
        KConfig config(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        const auto group = config.group(QStringLiteral("Workspaces"));
        FavoritesModel favorites(config.group(QStringLiteral("Favorites")));
        KFilePlacesModel places;
        const QUrl local = QUrl::fromLocalFile(directory.filePath(QStringLiteral("notes")));
        const QUrl remote(QStringLiteral("sftp://example.invalid/notes"));
        QVERIFY(favorites.add(local));
        QVERIFY(favorites.add(remote));
        WorkspaceLocations locations(&favorites, &places);
        WorkspaceCatalog::Catalog catalog;
        QString error;
        QVERIFY(WorkspaceCatalog::create(catalog, QStringLiteral("Work"), QStringLiteral("W"), error));
        const QString workId = catalog.workspaces.first().id;
        const QString localId = WorkspaceCatalog::locationId(local);
        const QString remoteId = WorkspaceCatalog::locationId(remote);
        QVERIFY(WorkspaceCatalog::assignLocation(catalog, remoteId, workId, error));
        QVERIFY(WorkspaceCatalog::save(group, catalog, error));
        WorkspaceDialog dialog(
            group,
            [](WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            [](const WorkspaceCatalog::Layout &, QString &) {
                return true;
            },
            workId,
            nullptr,
            &locations);
        auto *workspaces = dialog.findChild<QListWidget *>(QStringLiteral("workspaceList"));
        auto *savedLocations = dialog.findChild<QListWidget *>(QStringLiteral("workspaceLocationList"));
        auto *assign = dialog.findChild<QPushButton *>(QStringLiteral("workspaceAssignLocation"));
        auto *unassign = dialog.findChild<QPushButton *>(QStringLiteral("workspaceUnassignLocation"));
        auto *vault = dialog.findChild<QPushButton *>(QStringLiteral("workspaceUseAsVault"));
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("workspaceStatus"));
        QVERIFY(workspaces && savedLocations && assign && unassign && vault && status);
        const auto select = [&](const QString &id) -> QListWidgetItem * {
            for (int row = 0; row < savedLocations->count(); ++row) {
                if (savedLocations->item(row)->data(Qt::UserRole).toString() == id) {
                    savedLocations->setCurrentRow(row);
                    return savedLocations->item(row);
                }
            }
            return nullptr;
        };
        const auto stored = [&] {
            WorkspaceCatalog::Catalog loaded;
            QString problem;
            WorkspaceCatalog::load(group, loaded, problem);
            return loaded.workspaces.value(0).vaultLocationId;
        };
        workspaces->setCurrentRow(0);

        QVERIFY(select(localId));
        vault->click();
        QVERIFY(stored().isEmpty());
        QVERIFY(!status->text().isEmpty());

        QVERIFY(select(remoteId));
        vault->click();
        QVERIFY(stored().isEmpty());
        QVERIFY(!status->text().isEmpty());

        QVERIFY(select(localId));
        assign->click();
        QVERIFY(select(localId));
        vault->click();
        QCOMPARE(stored(), localId);
        QVERIFY2(select(localId)->text().contains(QStringLiteral("editor vault")), qPrintable(select(localId)->text()));

        vault->click();
        QVERIFY(stored().isEmpty());
        QVERIFY(!select(localId)->text().contains(QStringLiteral("editor vault")));

        vault->click();
        QCOMPARE(stored(), localId);

        // A vault turned off in Preferences stays listed, and using it turns it on again.
        {
            WorkspaceCatalog::Catalog edited;
            QString problem;
            QVERIFY(WorkspaceCatalog::load(group, edited, problem));
            QVERIFY(WorkspaceCatalog::setVaultOptions(edited, workId, false, std::nullopt, problem));
            QVERIFY(WorkspaceCatalog::save(group, edited, problem));
        }
        dialog.refreshCatalog();
        QVERIFY2(select(localId)->text().contains(QStringLiteral("editor vault turned off")), qPrintable(select(localId)->text()));
        vault->click();
        QCOMPARE(stored(), localId);
        QVERIFY(!select(localId)->text().contains(QStringLiteral("turned off")));
        unassign->click();
        QVERIFY(stored().isEmpty());
        QCOMPARE(favorites.urls(), QList<QUrl>({local, remote}));
    }
};

QTEST_MAIN(WorkspaceDialogTest)

#include "workspacedialogtest.moc"
