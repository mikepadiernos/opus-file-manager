// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspacerestore.h"
#include "settings.h"

#include <QStandardPaths>
#include <QTest>

using namespace WorkspaceCatalog;

namespace
{
PaneSession pane(const QString &path, const QString &filter, int view, int presentation)
{
    PaneSession session;
    session.tabs = {QUrl::fromLocalFile(path), QUrl::fromLocalFile(path + QStringLiteral("/sub"))};
    session.currentTab = 1;
    session.pinnedCount = 1;
    session.lockedTabs = {1};
    session.names = {path};
    session.filterText = filter;
    session.filterScope = 1;
    session.viewMode = view;
    session.locationPresentation = presentation;
    return session;
}

Layout savedLayout()
{
    Presentation presentation;
    presentation.panePlacement = PanePlacement::Window;
    presentation.previewVisible = true;
    presentation.terminalVisible = true;
    presentation.windowObjects = {QStringLiteral("savedDock")};
    presentation.toolbarActions = QStringList{QStringLiteral("saved_action")};
    presentation.detachedPaneGeometry = QRect(1, 2, 3, 4);
    WindowSession session;
    session.left = pane(QStringLiteral("/saved"), QStringLiteral("*.txt"), 1, 1);
    session.right = pane(QStringLiteral("/saved-right"), QStringLiteral("*.md"), 2, 2);
    session.left.yaziMode = true;
    session.left.yaziPreview = 0;
    session.left.yaziPreviewContent = 1;
    session.left.yaziParentColumn = 0;
    session.left.yaziColumnRatios = {2, 5, 3};
    session.activePane = 1;
    return Layout{QByteArray("saved"), {1, 2}, {3, 4}, true, false, presentation, session};
}

Layout currentLayout()
{
    Presentation presentation;
    presentation.toolbarActions = QStringList{QStringLiteral("current_action")};
    WindowSession session;
    session.left = pane(QStringLiteral("/current"), QString(), 0, 0);
    session.left.tabs.prepend(QUrl::fromLocalFile(QStringLiteral("/saved/sub")));
    session.left.currentTab = 0;
    session.right = pane(QStringLiteral("/current-right"), QString(), 0, 0);
    session.right.yaziMode = true;
    session.right.yaziPreview = 1;
    session.right.yaziPreviewContent = 0;
    session.right.yaziParentColumn = 1;
    session.right.yaziColumnRatios = {1, 1, 1};
    return Layout{QByteArray("current"), {8, 9}, {10, 11}, false, true, presentation, session};
}

void verifyCurrentYazi(const WindowSession &session)
{
    QVERIFY(!session.left.yaziMode);
    QVERIFY(session.right.yaziMode);
    QCOMPARE(session.left.yaziPreview, -1);
    QCOMPARE(session.right.yaziPreview, 1);
    QCOMPARE(session.left.yaziPreviewContent, -1);
    QCOMPARE(session.right.yaziPreviewContent, 0);
    QCOMPARE(session.left.yaziParentColumn, -1);
    QCOMPARE(session.right.yaziParentColumn, 1);
    QVERIFY(session.left.yaziColumnRatios.isEmpty());
    QCOMPARE(session.right.yaziColumnRatios, QList<int>({1, 1, 1}));
}

void verifySavedYazi(const WindowSession &session)
{
    const WindowSession saved = *savedLayout().session;
    for (const auto &[result, expected] : {qMakePair(session.left, saved.left), qMakePair(session.right, saved.right)}) {
        QCOMPARE(result.yaziMode, expected.yaziMode);
        QCOMPARE(result.yaziPreview, expected.yaziPreview);
        QCOMPARE(result.yaziPreviewContent, expected.yaziPreviewContent);
        QCOMPARE(result.yaziParentColumn, expected.yaziParentColumn);
        QCOMPARE(result.yaziColumnRatios, expected.yaziColumnRatios);
    }
}

/// A layout with each pane's Yazi state cleared, to compare everything else.
Layout withoutYazi(Layout layout)
{
    if (layout.session) {
        for (PaneSession *pane : {&layout.session->left, &layout.session->right}) {
            const PaneSession cleared;
            pane->yaziMode = cleared.yaziMode;
            pane->yaziPreview = cleared.yaziPreview;
            pane->yaziPreviewContent = cleared.yaziPreviewContent;
            pane->yaziParentColumn = cleared.yaziParentColumn;
            pane->yaziColumnRatios = cleared.yaziColumnRatios;
        }
    }
    return layout;
}
}

class WorkspaceRestoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void restoresEverythingUnchanged()
    {
        Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, std::nullopt};
        Layout result;
        QString error;
        QVERIFY2(WorkspaceRestore::merge(saved, currentLayout(), {}, {}, result, error), qPrintable(error));
        QCOMPARE(result, saved);
        QVERIFY(WorkspaceRestore::merge(savedLayout(), currentLayout(), {}, {}, result, error));
        QCOMPARE(result, savedLayout());
    }

    void keepsCurrentPaneArrangement()
    {
        WorkspaceRestore::Options options;
        options.paneArrangement = false;
        Layout result;
        QString error;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), {}, options, result, error), qPrintable(error));
        QCOMPARE(result.paneSizes, currentLayout().paneSizes);
        QCOMPARE(result.dual, false);
        QCOMPARE(result.stacked, true);
        QCOMPARE(result.presentation->panePlacement, PanePlacement::Split);
        QCOMPARE(result.presentation->detachedPaneGeometry, QRect());
        QCOMPARE(result.presentation->previewVisible, true);
        QCOMPARE(result.windowState, QByteArray("saved"));
        QCOMPARE(result.session->left, savedLayout().session->left);
        QCOMPARE(result.session->dual, false);
        QCOMPARE(result.session->stacked, true);
    }

    void keepsCurrentViewsToolbarsAndPanels()
    {
        WorkspaceRestore::Options options;
        options.views = false;
        options.toolbars = false;
        options.panels = false;
        // Workspaces saved before the Yazi mode behavior restore Yazi mode with the views.
        BehaviorSelection legacy;
        legacy.yaziMode = std::nullopt;
        Layout result;
        QString error;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), legacy, options, result, error), qPrintable(error));
        QCOMPARE(result.windowState, QByteArray("current"));
        QCOMPARE(result.workspaceSizes, currentLayout().workspaceSizes);
        QCOMPARE(result.paneSizes, savedLayout().paneSizes);
        QCOMPARE(result.presentation->panePlacement, PanePlacement::Window);
        QCOMPARE(result.presentation->previewVisible, false);
        QCOMPARE(result.presentation->terminalVisible, false);
        QVERIFY(result.presentation->windowObjects.isEmpty());
        QCOMPARE(result.presentation->toolbarActions, currentLayout().presentation->toolbarActions);
        QCOMPARE(result.session->left.viewMode, 0);
        QCOMPARE(result.session->right.viewMode, 0);
        verifyCurrentYazi(*result.session);
        QCOMPARE(result.session->left.tabs, savedLayout().session->left.tabs);
        QCOMPARE(result.session->left.filterText, QStringLiteral("*.txt"));
        QCOMPARE(result.session->activePane, 1);

        options = {};
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), legacy, options, result, error), qPrintable(error));
        verifySavedYazi(*result.session);
    }

    void restoresYaziModeIndependentlyOfViews()
    {
        WorkspaceRestore::Options options;
        options.views = false;
        BehaviorSelection selection;
        QVERIFY(selection.yaziMode.value_or(false));
        Layout result;
        QString error;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), selection, options, result, error), qPrintable(error));
        QCOMPARE(result.session->left.viewMode, 0);
        QCOMPARE(result.session->right.viewMode, 0);
        verifySavedYazi(*result.session);

        options = {};
        selection.mode = BehaviorMode::Selected;
        selection.yaziMode = false;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), selection, options, result, error), qPrintable(error));
        QCOMPARE(result.session->left.viewMode, 1);
        QCOMPARE(result.session->right.viewMode, 2);
        verifyCurrentYazi(*result.session);
        QCOMPARE(result.session->left.tabs, savedLayout().session->left.tabs);

        // Only the Yazi mode behavior selected, as when the other behaviors are off.
        selection = {};
        selection.mode = BehaviorMode::Selected;
        selection.locationPresentation = false;
        selection.searchScope = false;
        selection.layout = false;
        selection.paneTabRestoration = false;
        selection.appearance = false;
        selection.yaziMode = true;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), selection, options, result, error), qPrintable(error));
        QCOMPARE(result.session->left.tabs, currentLayout().session->left.tabs);
        QCOMPARE(result.session->left.viewMode, 0);
        verifySavedYazi(*result.session);

        selection.yaziMode = std::nullopt;
        QVERIFY(!selection.appliesYaziMode(true));
        selection.layout = true;
        QVERIFY(selection.appliesYaziMode(true));
        QVERIFY(!selection.appliesYaziMode(false));
        selection.mode = BehaviorMode::None;
        selection.yaziMode = true;
        QVERIFY(!selection.appliesYaziMode(true));
    }

    void yaziOptionsChooseTheRestoredParts()
    {
        using WorkspaceRestore::YaziParts;
        WorkspaceRestore::Options options;
        BehaviorSelection selection;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts({true, true}));
        options.yaziMode = false;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts({false, true}));
        options.yaziColumns = false;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts({false, false}));
        QVERIFY(!WorkspaceRestore::yaziParts(selection, options).any());
        options = {};
        options.yaziColumns = false;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts({true, false}));
        QVERIFY(WorkspaceRestore::yaziParts(selection, options).any());

        // The options never restore Yazi state that the workspace's behaviors leave out.
        options = {};
        selection.mode = BehaviorMode::Selected;
        selection.yaziMode = false;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts());
        selection.mode = BehaviorMode::None;
        selection.yaziMode = true;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts());
        // Workspaces saved before the Yazi mode behavior follow the layout and views, then the options.
        selection = {};
        selection.yaziMode = std::nullopt;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts({true, true}));
        options.yaziMode = false;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts({false, true}));
        options.views = false;
        QCOMPARE(WorkspaceRestore::yaziParts(selection, options), YaziParts());
    }

    void yaziOptionsKeepEachPanesCurrentYaziState()
    {
        Layout full;
        QString error;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), {}, {}, full, error), qPrintable(error));
        // A partial pane restore matches the session's arrangement to the restored layout's, as stored layouts do.
        full.session->dual = full.dual;
        full.session->stacked = full.stacked;
        const Layout saved = savedLayout();
        const Layout current = currentLayout();

        // Without the mode, each pane keeps its own mode and takes the saved columns.
        WorkspaceRestore::Options options;
        options.yaziMode = false;
        Layout result;
        QVERIFY2(WorkspaceRestore::merge(saved, current, {}, options, result, error), qPrintable(error));
        QCOMPARE(result.session->left.yaziMode, current.session->left.yaziMode);
        QCOMPARE(result.session->right.yaziMode, current.session->right.yaziMode);
        QCOMPARE(result.session->left.yaziPreview, 0);
        QCOMPARE(result.session->left.yaziPreviewContent, 1);
        QCOMPARE(result.session->left.yaziParentColumn, 0);
        QCOMPARE(result.session->left.yaziColumnRatios, QList<int>({2, 5, 3}));
        QCOMPARE(result.session->right.yaziPreview, -1);
        QVERIFY(result.session->right.yaziColumnRatios.isEmpty());
        // The views, tabs, locations and everything else are restored as without the option.
        QCOMPARE(withoutYazi(result), withoutYazi(full));
        QCOMPARE(result.session->left.viewMode, 1);
        QCOMPARE(result.session->left.tabs, saved.session->left.tabs);

        // Without the columns, each pane takes the saved mode and keeps its own columns.
        options = {};
        options.yaziColumns = false;
        QVERIFY2(WorkspaceRestore::merge(saved, current, {}, options, result, error), qPrintable(error));
        QVERIFY(result.session->left.yaziMode);
        QVERIFY(!result.session->right.yaziMode);
        QCOMPARE(result.session->left.yaziPreview, -1);
        QCOMPARE(result.session->left.yaziPreviewContent, -1);
        QCOMPARE(result.session->left.yaziParentColumn, -1);
        QVERIFY(result.session->left.yaziColumnRatios.isEmpty());
        QCOMPARE(result.session->right.yaziPreview, 1);
        QCOMPARE(result.session->right.yaziColumnRatios, QList<int>({1, 1, 1}));
        QCOMPARE(withoutYazi(result), withoutYazi(full));

        // Without either, nothing of Yazi mode changes, whether or not the views are restored.
        options.yaziMode = false;
        for (const bool views : {false, true}) {
            options.views = views;
            QVERIFY2(WorkspaceRestore::merge(saved, current, {}, options, result, error), qPrintable(error));
            verifyCurrentYazi(*result.session);
            QCOMPARE(result.session->left.viewMode, views ? 1 : 0);
            QCOMPARE(result.session->left.tabs, saved.session->left.tabs);
        }
        QCOMPARE(withoutYazi(result), withoutYazi(full));

        // A workspace that restores only Yazi mode then restores nothing.
        BehaviorSelection yaziOnly{BehaviorMode::Selected, false, false, false, false, false, true};
        QVERIFY2(WorkspaceRestore::merge(saved, current, yaziOnly, options, result, error), qPrintable(error));
        QCOMPARE(result, current);
        options.yaziMode = true;
        QVERIFY2(WorkspaceRestore::merge(saved, current, yaziOnly, options, result, error), qPrintable(error));
        QVERIFY(result.session->left.yaziMode);
        QCOMPARE(result.session->left.tabs, current.session->left.tabs);
        QCOMPARE(result.session->left.yaziColumnRatios, current.session->left.yaziColumnRatios);
        QCOMPARE(result.windowState, current.windowState);

        // A workspace without the Yazi mode behavior is unaffected by the options.
        BehaviorSelection withoutYaziBehavior{BehaviorMode::Selected, true, true, true, true, true, false};
        Layout before;
        QVERIFY2(WorkspaceRestore::merge(saved, current, withoutYaziBehavior, {}, before, error), qPrintable(error));
        options = {};
        options.yaziMode = false;
        options.yaziColumns = false;
        QVERIFY2(WorkspaceRestore::merge(saved, current, withoutYaziBehavior, options, result, error), qPrintable(error));
        QCOMPARE(result, before);
        verifyCurrentYazi(*result.session);

        // A layout without a pane snapshot restores as before, with nothing of Yazi mode to keep.
        const Layout noSnapshot{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, std::nullopt};
        QVERIFY2(WorkspaceRestore::merge(noSnapshot, current, {}, options, result, error), qPrintable(error));
        QCOMPARE(result, noSnapshot);
    }

    void readsYaziOptionsFromSettings()
    {
        Settings::self()->setDefaults();
        QCOMPARE(WorkspaceRestore::fromSettings(), WorkspaceRestore::Options());
        Settings::setYaziWorkspaceRestoreMode(false);
        QVERIFY(!WorkspaceRestore::fromSettings().yaziMode);
        QVERIFY(WorkspaceRestore::fromSettings().yaziColumns);
        Settings::setYaziWorkspaceRestoreColumns(false);
        QVERIFY(!WorkspaceRestore::fromSettings().yaziColumns);
        // They are Yazi mode settings, separate from the workspace defaults.
        QCOMPARE(Settings::self()->findItem(QStringLiteral("YaziWorkspaceRestoreMode"))->group(), QStringLiteral("YaziMode"));
        QCOMPARE(Settings::self()->findItem(QStringLiteral("YaziWorkspaceRestoreColumns"))->group(), QStringLiteral("YaziMode"));
        QCOMPARE(WorkspaceRestore::defaultBehaviorSelection(), BehaviorSelection{});
        Settings::self()->setDefaults();
    }

    void keepsCurrentTabsActiveTabsAndFilters()
    {
        WorkspaceRestore::Options options;
        options.activeTabs = false;
        options.filters = false;
        Layout result;
        QString error;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), {}, options, result, error), qPrintable(error));
        const auto saved = savedLayout();
        QCOMPARE(result.session->left.tabs, saved.session->left.tabs);
        // The current folder /saved/sub is also a saved tab, so it stays current.
        QCOMPARE(result.session->left.currentTab, 1);
        QCOMPARE(result.session->right.currentTab, 0);
        QCOMPARE(result.session->activePane, 0);
        QVERIFY(result.session->left.filterText.isEmpty());
        QCOMPARE(result.session->left.viewMode, 1);

        options.tabs = false;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), {}, options, result, error), qPrintable(error));
        QCOMPARE(result.session->left.tabs, currentLayout().session->left.tabs);
        QCOMPARE(result.session->left.names, currentLayout().session->left.names);
        QCOMPARE(result.session->left.viewMode, 1);
        QCOMPARE(result.session->right.locationPresentation, 2);
    }

    void optionsOnlyApplyToSelectedBehaviors()
    {
        const BehaviorSelection selection{BehaviorMode::Selected, false, false, false, true};
        Layout result;
        QString error;
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), selection, {}, result, error), qPrintable(error));
        QCOMPARE(result.windowState, QByteArray("current"));
        QCOMPARE(result.presentation, currentLayout().presentation);
        QCOMPARE(result.session->left.tabs, savedLayout().session->left.tabs);
        QCOMPARE(result.session->left.viewMode, 0);
        QCOMPARE(result.session->left.locationPresentation, 0);

        const BehaviorSelection none{BehaviorMode::None, true, true, true, true};
        QVERIFY2(WorkspaceRestore::merge(savedLayout(), currentLayout(), none, {}, result, error), qPrintable(error));
        QCOMPARE(result, currentLayout());
    }

    void readsDefaultBehaviorSelection()
    {
        QCOMPARE(WorkspaceRestore::defaultBehaviorSelection(), BehaviorSelection{});
        Settings::setWorkspaceDefaultBehaviorMode(Settings::EnumWorkspaceDefaultBehaviorMode::Selected);
        Settings::setWorkspaceDefaultLocationPresentation(false);
        Settings::setWorkspaceDefaultLayout(false);
        const BehaviorSelection expected{BehaviorMode::Selected, false, true, false, true};
        QCOMPARE(WorkspaceRestore::defaultBehaviorSelection(), expected);
        Settings::self()->setDefaults();
    }

    void reportsMissingSnapshots()
    {
        Layout saved{QByteArray("saved"), {1, 2}, {3, 4}, true, false, std::nullopt, std::nullopt};
        WorkspaceRestore::Options options;
        options.toolbars = false;
        Layout result;
        QString error;
        QVERIFY(!WorkspaceRestore::merge(saved, currentLayout(), {}, options, result, error));
        QVERIFY(!error.isEmpty());

        options = {};
        options.filters = false;
        QVERIFY(!WorkspaceRestore::merge(saved, currentLayout(), {}, options, result, error));
        QVERIFY(!error.isEmpty());

        Layout current = currentLayout();
        current.session.reset();
        QVERIFY(!WorkspaceRestore::merge(savedLayout(), current, {}, options, result, error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(WorkspaceRestoreTest)
#include "workspacerestoretest.moc"
