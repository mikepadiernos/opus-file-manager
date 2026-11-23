// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QStringList>

class QAction;
class KActionCollection;
class KToolBar;
class Pane;
class QDockWidget;
class QDomDocument;
class QLineEdit;
class QListWidget;
class QMainWindow;
class QMenu;
class QMenuBar;
class QMimeData;
class QToolBar;

class ToolbarCustomizer : public QObject
{
    Q_OBJECT

public:
    ToolbarCustomizer(KToolBar *toolbar, KActionCollection *actions, QMainWindow *window, const QList<Pane *> &panes = {});

    void setEditing(bool editing);
    void applyAppearanceSettings();
    void resetToDefaults();
    bool prepareKdeToolbarEditor(QDomDocument &document);
    void finishKdeToolbarEditor();
    void prepareGuiRebuild();
    void toolbarRebuilt(KToolBar *toolbar);
    QStringList toolbarConfiguration() const;
    void setToolbarConfiguration(const QStringList &actions);

Q_SIGNALS:
    void editingChanged(bool editing);
    void toolbarSettingsRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct MenuSnapshot {
        QPointer<QMenu> menu;
        QList<QAction *> actions;
    };

    void populateActions();
    void filterActions(const QString &text);
    QList<QMenu *> menus() const;
    void installButtonFilter(QAction *action);
    QList<QToolBar *> toolbars() const;
    QAction *actionForButton(QObject *button) const;
    QAction *actionForPersistenceName(const QString &name) const;
    QString persistenceName(QAction *action) const;
    QString kindForAction(QAction *action) const;
    void startActionDrag(QAction *action, QToolBar *toolbar);
    void startPaletteDrag(const QPoint &position);
    void showActionMenu(QAction *action, const QPoint &globalPosition);
    void showToolbarMenu(QToolBar *toolbar, const QPoint &globalPosition);
    void applyAppearance(QAction *action);
    void saveToolbar();
    void restoreToolbar();
    void handleToolbarDrop(const QMimeData *mimeData, const QPoint &position, QToolBar *target);
    void handleMenuDrop(QMenu *target, const QMimeData *mimeData, const QPoint &position);
    void saveMenuCustomizations(QMenu *menu);
    void restoreMenuCustomizations();
    void removeToolbarItem(const QString &kind, const QString &name);

    QPointer<KToolBar> m_toolbar;
    KActionCollection *m_actions = nullptr;
    QPointer<QMainWindow> m_window;
    QPointer<QMenuBar> m_menuBar;
    QDockWidget *m_dock = nullptr;
    QLineEdit *m_filter = nullptr;
    QListWidget *m_actionList = nullptr;
    QList<Pane *> m_panes;
    struct ToolbarSnapshot {
        QPointer<QAction> action;
        QString kind;
    };
    struct EditorAlias {
        QAction *action;
        QPointer<QAction> original;
        QString iconName;
        QString iconText;
    };
    QList<ToolbarSnapshot> m_defaultToolbarActions;
    QList<MenuSnapshot> m_defaultMenus;
    QList<EditorAlias> m_editorAliases;
    QPointer<QAction> m_pressedAction;
    QPointer<QMenu> m_pressedMenu;
    QPoint m_pressPosition;
    QPoint m_palettePressPosition;
    bool m_editing = false;
    bool m_forwardingNativeMenu = false;
    bool m_restoring = false;
    bool m_saveScheduled = false;
    quint64 m_saveGeneration = 0;
};
