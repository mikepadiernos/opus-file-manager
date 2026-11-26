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
