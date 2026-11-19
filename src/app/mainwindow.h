// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KFileItem>
#include <KXmlGuiWindow>

#include <QList>
#include <QUrl>

class KFileItemActions;
class KFilePlacesModel;
class KFilePlacesView;
class KNewFileMenu;
class KToggleAction;
class Pane;
class QLabel;
class QSplitter;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Opens each URL in a new tab of the active pane; restores the
    /// session (or the home folder) when the list is empty.
    void openUrls(const QList<QUrl> &urls);

    Pane *activePane() const;
    Pane *inactivePane() const;

    /**
     * Splits the panes top/bottom instead of side by side. When stacked,
     * the top pane's tabs run along its bottom edge and the bottom pane's
     * tabs along its top edge, so the two tab bars meet in the middle.
     * @param stacked true for top/bottom, false for side by side
     */
    void setStackedLayout(bool stacked);
    bool isStackedLayout() const;

protected:
    bool queryClose() override;
    void readProperties(const KConfigGroup &group) override;
    void saveProperties(KConfigGroup &group) override;

private:
    void setupActions();
    void setupPanes();
    void setupPlaces();
    void setupStatusBar();

    void setActivePane(Pane *pane);
    void connectPane(Pane *pane);
    void updateActions();
    void updateStatusBar();
    void updateFreeSpace();
    void updateWindowTitle();
    void updateTabBars();
    void showStatusMessage(const QString &message);

    void restoreSession();
    void saveSession();

    void showContextMenu(Pane *pane, const QPoint &globalPos, const KFileItemList &items);
    void activateItem(const KFileItem &item);

    void copyToOtherPane();
    void moveToOtherPane();
    void cut();
    void copy();
    void paste();
    void rename();
    void trash();
    void deletePermanently();
    void showProperties();
    void openTerminal();
    void newFolder();
    void showNewMenu();
    void toggleLayout(bool dual);
    void toggleViewMode();
    void setShowHiddenFiles(bool show);
    void setPreviewsEnabled(bool enabled);

    KFilePlacesModel *m_placesModel = nullptr;
    KFilePlacesView *m_placesView = nullptr;
    KFileItemActions *m_fileItemActions = nullptr;
    KNewFileMenu *m_newFileMenu = nullptr;
    QSplitter *m_splitter = nullptr;
    Pane *m_left = nullptr;
    Pane *m_right = nullptr;
    Pane *m_active = nullptr;
    QLabel *m_itemsLabel = nullptr;
    QLabel *m_messageLabel = nullptr;
    QLabel *m_freeSpaceLabel = nullptr;
    KToggleAction *m_dualLayoutAction = nullptr;
    KToggleAction *m_stackedLayoutAction = nullptr;
    KToggleAction *m_hiddenFilesAction = nullptr;
    KToggleAction *m_previewsAction = nullptr;
    QAction *m_viewModeAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_pasteAction = nullptr;
    bool m_sessionRestored = false;
};
