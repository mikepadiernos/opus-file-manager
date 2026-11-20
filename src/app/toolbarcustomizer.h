// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPoint>
#include <QPointer>

class QAction;
class KActionCollection;
class KToolBar;
class QDockWidget;
class QLineEdit;
class QListWidget;
class QMainWindow;
class QMimeData;

class ToolbarCustomizer : public QObject
{
    Q_OBJECT

public:
    ToolbarCustomizer(KToolBar *toolbar, KActionCollection *actions, QMainWindow *window);

    void setEditing(bool editing);

Q_SIGNALS:
    void editingChanged(bool editing);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void populateActions();
    void filterActions(const QString &text);
    void installButtonFilter(QAction *action);
    QAction *actionForButton(QObject *button) const;
    QString kindForAction(QAction *action) const;
    void startActionDrag(QAction *action);
    void startPaletteDrag(const QPoint &position);
    void showActionMenu(QAction *action, const QPoint &globalPosition);
    void applyAppearance(QAction *action);
    void saveToolbar();
    void restoreToolbar();
    void handleToolbarDrop(const QMimeData *mimeData, const QPoint &position);
    void removeToolbarItem(const QString &kind, const QString &name);

    KToolBar *m_toolbar = nullptr;
    KActionCollection *m_actions = nullptr;
    QMainWindow *m_window = nullptr;
    QDockWidget *m_dock = nullptr;
    QLineEdit *m_filter = nullptr;
    QListWidget *m_actionList = nullptr;
    QPointer<QAction> m_pressedAction;
    QPoint m_pressPosition;
    QPoint m_palettePressPosition;
    bool m_editing = false;
    bool m_restoring = false;
    bool m_saveScheduled = false;
};
