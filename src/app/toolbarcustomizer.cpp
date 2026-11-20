// SPDX-License-Identifier: GPL-3.0-or-later

#include "toolbarcustomizer.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBar>

#include <QActionGroup>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDialog>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>

namespace
{
constexpr auto ToolbarMimeType = "application/x-opus-toolbar-item";
constexpr auto ToolbarGroup = "ToolbarCustomization";
constexpr auto EntriesGroup = "Entries";

QByteArray mimePayload(const QString &source, const QString &kind, const QString &name)
{
    return (source + QLatin1Char('\n') + kind + QLatin1Char('\n') + name).toUtf8();
}

bool parsePayload(const QMimeData *mimeData, QStringList *parts)
{
    if (!mimeData->hasFormat(QLatin1String(ToolbarMimeType))) {
        return false;
    }
    *parts = QString::fromUtf8(mimeData->data(QLatin1String(ToolbarMimeType))).split(QLatin1Char('\n'));
    return parts->size() == 3;
}
}

ToolbarCustomizer::ToolbarCustomizer(KToolBar *toolbar, KActionCollection *actions, QMainWindow *window)
    : QObject(window)
    , m_toolbar(toolbar)
    , m_actions(actions)
    , m_window(window)
    , m_dock(new QDockWidget(i18nc("@title:window", "Toolbar Actions"), window))
    , m_filter(new QLineEdit(m_dock))
    , m_actionList(new QListWidget(m_dock))
{
    m_dock->setObjectName(QStringLiteral("toolbarActionsDock"));
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    auto *contents = new QWidget(m_dock);
    auto *layout = new QVBoxLayout(contents);
    layout->setContentsMargins(8, 8, 8, 8);
    auto *instructions = new QLabel(
        i18nc("@info", "Drag actions onto the toolbar. Drag toolbar entries away to remove them. Right-click a toolbar entry to change its appearance."),
        contents);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);
    m_filter->setPlaceholderText(i18nc("@placeholder", "Filter actions"));
    m_filter->setObjectName(QStringLiteral("toolbarActionFilter"));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);
    m_actionList->setObjectName(QStringLiteral("toolbarActionList"));
    m_actionList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_actionList->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_actionList->viewport()->setAcceptDrops(false);
    m_actionList->viewport()->installEventFilter(this);
    layout->addWidget(m_actionList);
    m_dock->setWidget(contents);
    window->addDockWidget(Qt::RightDockWidgetArea, m_dock);
    m_dock->hide();

    m_toolbar->setAcceptDrops(false);
    m_toolbar->installEventFilter(this);
    connect(m_filter, &QLineEdit::textChanged, this, &ToolbarCustomizer::filterActions);
    connect(m_dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible && m_editing) {
            setEditing(false);
        }
    });

    restoreToolbar();
    populateActions();
}

void ToolbarCustomizer::setEditing(bool editing)
{
    if (m_editing == editing) {
        return;
    }
    m_editing = editing;
    m_actionList->viewport()->setAcceptDrops(editing);
    m_toolbar->setAcceptDrops(editing);
    if (editing) {
        installButtonFilter(nullptr);
        m_dock->show();
        m_dock->raise();
    } else {
        m_dock->hide();
    }
    Q_EMIT editingChanged(editing);
}

bool ToolbarCustomizer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_toolbar) {
        if (!m_restoring && !m_saveScheduled && (event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved)) {
            m_saveScheduled = true;
            QTimer::singleShot(0, this, [this] {
                m_saveScheduled = false;
                if (!m_restoring) {
                    installButtonFilter(nullptr);
                    saveToolbar();
                    populateActions();
                }
            });
        }
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *drop = static_cast<QDropEvent *>(event);
            QStringList parts;
            if (m_editing && parsePayload(drop->mimeData(), &parts)) {
                drop->setDropAction(Qt::MoveAction);
                drop->accept();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            QStringList parts;
            if (m_editing && parsePayload(drop->mimeData(), &parts)) {
                handleToolbarDrop(drop->mimeData(), drop->position().toPoint());
                drop->setDropAction(Qt::MoveAction);
                drop->accept();
                return true;
            }
        }
    } else if (watched == m_actionList->viewport()) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *drop = static_cast<QDropEvent *>(event);
            QStringList parts;
            if (m_editing && parsePayload(drop->mimeData(), &parts) && parts.at(0) == QLatin1String("toolbar")) {
                drop->setDropAction(Qt::MoveAction);
                drop->accept();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            QStringList parts;
            if (m_editing && parsePayload(drop->mimeData(), &parts) && parts.at(0) == QLatin1String("toolbar")) {
                removeToolbarItem(parts.at(1), parts.at(2));
                drop->setDropAction(Qt::MoveAction);
                drop->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            m_palettePressPosition = static_cast<QMouseEvent *>(event)->position().toPoint();
        } else if (event->type() == QEvent::MouseMove && m_editing) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if ((mouse->buttons() & Qt::LeftButton)
                && (mouse->position().toPoint() - m_palettePressPosition).manhattanLength() >= QApplication::startDragDistance()) {
                startPaletteDrag(m_palettePressPosition);
                return true;
            }
        }
    } else if (qobject_cast<QWidget *>(watched)) {
        QAction *action = actionForButton(watched);
        if (event->type() == QEvent::MouseButtonPress && m_editing) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_pressedAction = action;
                m_pressPosition = mouse->position().toPoint();
            }
        } else if (event->type() == QEvent::MouseMove && m_editing && m_pressedAction) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if ((mouse->buttons() & Qt::LeftButton) && (mouse->position().toPoint() - m_pressPosition).manhattanLength() >= QApplication::startDragDistance()) {
                startActionDrag(m_pressedAction);
                m_pressedAction = nullptr;
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_pressedAction = nullptr;
        } else if (event->type() == QEvent::ContextMenu && m_editing && action) {
            showActionMenu(action, static_cast<QContextMenuEvent *>(event)->globalPos());
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void ToolbarCustomizer::populateActions()
{
    m_actionList->clear();
    for (QAction *action : m_actions->actions()) {
        const QString name = action->objectName();
        if (name.isEmpty() || action->text().isEmpty() || action->isSeparator() || name == QLatin1String("customize_main_toolbar")
            || m_toolbar->actions().contains(action)) {
            continue;
        }
        auto *item = new QListWidgetItem(action->icon(), action->text(), m_actionList);
        item->setData(Qt::UserRole, QStringLiteral("action"));
        item->setData(Qt::UserRole + 1, name);
    }
    for (const auto &[kind, label] : {qMakePair(QStringLiteral("separator"), i18nc("@item:inlistbox", "Separator")),
                                      qMakePair(QStringLiteral("spacer"), i18nc("@item:inlistbox", "Expanding Spacer"))}) {
        auto *item = new QListWidgetItem(label, m_actionList);
        item->setData(Qt::UserRole, kind);
    }
    filterActions(m_filter->text());
}

void ToolbarCustomizer::filterActions(const QString &text)
{
    for (int row = 0; row < m_actionList->count(); ++row) {
        QListWidgetItem *item = m_actionList->item(row);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void ToolbarCustomizer::installButtonFilter(QAction *action)
{
    const QList<QAction *> actions = action ? QList<QAction *>{action} : m_toolbar->actions();
    for (QAction *candidate : actions) {
        if ((candidate->isSeparator() || kindForAction(candidate) == QLatin1String("spacer")) && candidate->objectName().isEmpty()) {
            const QString prefix = candidate->isSeparator() ? QStringLiteral("opus_toolbar_separator_") : QStringLiteral("opus_toolbar_spacer_");
            candidate->setObjectName(prefix + QUuid::createUuid().toString(QUuid::WithoutBraces));
        }
        QWidget *widget = m_toolbar->widgetForAction(candidate);
        if (widget && !widget->property("toolbarCustomizerInstalled").toBool()) {
            widget->setProperty("toolbarCustomizerInstalled", true);
            widget->installEventFilter(this);
        }
        if (widget && !candidate->property("toolbarCustomizerAppearanceConnected").toBool()) {
            candidate->setProperty("toolbarCustomizerAppearanceConnected", true);
            connect(candidate, &QAction::changed, this, [this, candidate] {
                applyAppearance(candidate);
            });
        }
        applyAppearance(candidate);
    }
}

QAction *ToolbarCustomizer::actionForButton(QObject *button) const
{
    for (QAction *action : m_toolbar->actions()) {
        if (m_toolbar->widgetForAction(action) == button) {
            return action;
        }
    }
    return nullptr;
}

QString ToolbarCustomizer::kindForAction(QAction *action) const
{
    if (action->isSeparator()) {
        return QStringLiteral("separator");
    }
    if (action->property("toolbarSpacer").toBool() || action->objectName() == QLatin1String("spacer")
        || action->objectName().startsWith(QLatin1String("spacer_")) || (qobject_cast<QWidgetAction *>(action) && !m_actions->actions().contains(action))) {
        return QStringLiteral("spacer");
    }
    return QStringLiteral("action");
}

void ToolbarCustomizer::startActionDrag(QAction *action)
{
    if (!action) {
        return;
    }
    const QString kind = kindForAction(action);
    const QString name = action->objectName();
    auto *mimeData = new QMimeData;
    mimeData->setData(QLatin1String(ToolbarMimeType), mimePayload(QStringLiteral("toolbar"), kind, name));
    QDrag drag(m_toolbar);
    drag.setMimeData(mimeData);
    if (!action->icon().isNull()) {
        drag.setPixmap(action->icon().pixmap(24, 24));
    }
    const Qt::DropAction result = drag.exec(Qt::MoveAction);
    if (result != Qt::MoveAction && m_toolbar->actions().contains(action)) {
        removeToolbarItem(kind, name);
    }
}

void ToolbarCustomizer::startPaletteDrag(const QPoint &position)
{
    QListWidgetItem *item = m_actionList->itemAt(position);
    if (!item) {
        return;
    }
    const QString kind = item->data(Qt::UserRole).toString();
    const QString name = item->data(Qt::UserRole + 1).toString();
    auto *mimeData = new QMimeData;
    mimeData->setData(QLatin1String(ToolbarMimeType), mimePayload(QStringLiteral("palette"), kind, name));
    QDrag drag(m_actionList);
    drag.setMimeData(mimeData);
    if (!item->icon().isNull()) {
        drag.setPixmap(item->icon().pixmap(24, 24));
    }
    drag.exec(Qt::MoveAction);
}

void ToolbarCustomizer::showActionMenu(QAction *action, const QPoint &globalPosition)
{
    if (action->isSeparator() || kindForAction(action) == QLatin1String("spacer")) {
        return;
    }
    QMenu menu(m_window);
    QAction *remove = menu.addAction(i18nc("@action:inmenu", "Remove from Toolbar"));
    menu.addSeparator();
    QAction *chooseIcon = menu.addAction(i18nc("@action:inmenu", "Choose Icon…"));
    QAction *resetIcon = menu.addAction(i18nc("@action:inmenu", "Use Default Icon"));
    QMenu *styleMenu = menu.addMenu(i18nc("@menu", "Button Style"));
    auto *styleGroup = new QActionGroup(styleMenu);
    styleGroup->setExclusive(true);
    const QList<QPair<Qt::ToolButtonStyle, QString>> styles{
        {Qt::ToolButtonIconOnly, i18nc("@item:inmenu", "Icon Only")},
        {Qt::ToolButtonTextOnly, i18nc("@item:inmenu", "Text Only")},
        {Qt::ToolButtonTextBesideIcon, i18nc("@item:inmenu", "Text Beside Icon")},
        {Qt::ToolButtonTextUnderIcon, i18nc("@item:inmenu", "Text Under Icon")},
    };
    const KConfigGroup entry = KSharedConfig::openConfig()->group(QLatin1String(ToolbarGroup)).group(QLatin1String(EntriesGroup)).group(action->objectName());
    const int currentStyle = entry.readEntry("ButtonStyle", -1);
    for (const auto &[style, label] : styles) {
        QAction *styleAction = styleMenu->addAction(label);
        styleAction->setCheckable(true);
        styleAction->setChecked(currentStyle == style);
        styleGroup->addAction(styleAction);
        connect(styleAction, &QAction::triggered, this, [this, action, style] {
            KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(ToolbarGroup)).group(QLatin1String(EntriesGroup)).group(action->objectName());
            group.writeEntry("ButtonStyle", static_cast<int>(style));
            group.sync();
            applyAppearance(action);
        });
    }
    QAction *chosen = menu.exec(globalPosition);
    if (chosen == remove) {
        removeToolbarItem(kindForAction(action), kindForAction(action) == QLatin1String("action") ? action->objectName() : QString());
    } else if (chosen == chooseIcon) {
        QStringList iconNames;
        for (QAction *candidate : m_actions->actions()) {
            const QString iconName = candidate->icon().name();
            if (!iconName.isEmpty() && !iconNames.contains(iconName)) {
                iconNames.append(iconName);
            }
        }
        std::sort(iconNames.begin(), iconNames.end());
        QInputDialog dialog(m_window);
        dialog.setWindowTitle(i18nc("@title:window", "Choose Toolbar Icon"));
        dialog.setLabelText(i18nc("@label", "Icon theme name:"));
        dialog.setComboBoxItems(iconNames);
        dialog.setComboBoxEditable(true);
        dialog.setTextValue(entry.readEntry("IconName", action->icon().name()));
        if (dialog.exec() == QDialog::Accepted && !dialog.textValue().trimmed().isEmpty()) {
            const QString iconName = dialog.textValue().trimmed();
            if (!QIcon::hasThemeIcon(iconName)) {
                QMessageBox::warning(m_window,
                                     i18nc("@title:window", "Icon Not Found"),
                                     i18nc("@info", "The current icon theme does not provide “%1”.", iconName));
            } else {
                KConfigGroup group =
                    KSharedConfig::openConfig()->group(QLatin1String(ToolbarGroup)).group(QLatin1String(EntriesGroup)).group(action->objectName());
                group.writeEntry("IconName", iconName);
                group.sync();
                applyAppearance(action);
            }
        }
    } else if (chosen == resetIcon) {
        KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(ToolbarGroup)).group(QLatin1String(EntriesGroup)).group(action->objectName());
        group.deleteEntry("IconName");
        group.sync();
        applyAppearance(action);
    }
}

void ToolbarCustomizer::applyAppearance(QAction *action)
{
    const KConfigGroup entry = KSharedConfig::openConfig()->group(QLatin1String(ToolbarGroup)).group(QLatin1String(EntriesGroup)).group(action->objectName());
    const QString iconName = entry.readEntry("IconName", QString());
    if (auto *button = qobject_cast<QToolButton *>(m_toolbar->widgetForAction(action))) {
        if (iconName.isEmpty()) {
            button->setIcon(action->icon());
        } else {
            const QIcon icon = QIcon::fromTheme(iconName);
            if (icon.isNull()) {
                qWarning() << "Configured toolbar icon is unavailable in the current theme:" << iconName;
            } else {
                button->setIcon(icon);
            }
        }
        const int style = entry.readEntry("ButtonStyle", -1);
        if (style >= Qt::ToolButtonIconOnly && style <= Qt::ToolButtonTextUnderIcon) {
            button->setToolButtonStyle(static_cast<Qt::ToolButtonStyle>(style));
        }
    }
}

void ToolbarCustomizer::saveToolbar()
{
    KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(ToolbarGroup));
    QStringList items;
    for (QAction *action : m_toolbar->actions()) {
        const QString kind = kindForAction(action);
        if (kind == QLatin1String("action")) {
            const QString name = action->objectName();
            if (!name.isEmpty()) {
                items.append(QStringLiteral("action:") + name);
            }
        } else {
            items.append(kind);
        }
    }
    group.writeEntry("Actions", items);
    group.sync();
}

void ToolbarCustomizer::restoreToolbar()
{
    m_restoring = true;
    const KConfigGroup group = KSharedConfig::openConfig()->group(QLatin1String(ToolbarGroup));
    if (!group.hasKey("Actions")) {
        installButtonFilter(nullptr);
        m_restoring = false;
        return;
    }
    const QStringList items = group.readEntry("Actions", QStringList());
    const QList<QAction *> defaultActions = m_toolbar->actions();
    m_toolbar->clear();
    bool restoredAny = false;
    for (const QString &item : items) {
        if (item == QLatin1String("separator")) {
            auto *separator = new QAction(this);
            separator->setSeparator(true);
            separator->setProperty("toolbarCustomizerOwned", true);
            m_toolbar->addAction(separator);
            restoredAny = true;
        } else if (item == QLatin1String("spacer")) {
            auto *spacer = new QWidgetAction(this);
            spacer->setProperty("toolbarSpacer", true);
            spacer->setProperty("toolbarCustomizerOwned", true);
            auto *widget = new QWidget;
            widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            widget->setMinimumWidth(16);
            spacer->setDefaultWidget(widget);
            m_toolbar->addAction(spacer);
            restoredAny = true;
        } else if (item.startsWith(QLatin1String("action:"))) {
            if (QAction *action = m_actions->action(item.mid(7))) {
                m_toolbar->addAction(action);
                restoredAny = true;
            }
        }
    }
    if (!items.isEmpty() && !restoredAny) {
        for (QAction *action : defaultActions) {
            m_toolbar->addAction(action);
        }
    }
    installButtonFilter(nullptr);
    m_restoring = false;
}

void ToolbarCustomizer::handleToolbarDrop(const QMimeData *mimeData, const QPoint &position)
{
    QStringList parts;
    if (!parsePayload(mimeData, &parts)) {
        return;
    }
    const QString source = parts.at(0);
    const QString kind = parts.at(1);
    const QString name = parts.at(2);
    if (source != QLatin1String("palette") && source != QLatin1String("toolbar")) {
        return;
    }
    QAction *action = nullptr;
    if (kind == QLatin1String("action")) {
        action = m_actions->action(name);
        if (!action || (source == QLatin1String("toolbar") && !m_toolbar->actions().contains(action))) {
            return;
        }
        if (source == QLatin1String("palette") && m_toolbar->actions().contains(action)) {
            return;
        }
    } else if (kind == QLatin1String("separator")) {
        if (source == QLatin1String("toolbar")) {
            const QList<QAction *> toolbarActions = m_toolbar->actions();
            const auto it = std::find_if(toolbarActions.cbegin(), toolbarActions.cend(), [&name](QAction *candidate) {
                return candidate->isSeparator() && candidate->objectName() == name;
            });
            if (it != toolbarActions.cend()) {
                action = *it;
            }
        }
        if (source == QLatin1String("toolbar") && !action) {
            return;
        }
        if (source == QLatin1String("palette")) {
            action = new QAction(this);
            action->setSeparator(true);
            action->setProperty("toolbarCustomizerOwned", true);
        }
    } else if (kind == QLatin1String("spacer")) {
        if (source == QLatin1String("toolbar")) {
            const QList<QAction *> toolbarActions = m_toolbar->actions();
            const auto it = std::find_if(toolbarActions.cbegin(), toolbarActions.cend(), [&name, this](QAction *candidate) {
                return kindForAction(candidate) == QLatin1String("spacer") && candidate->objectName() == name;
            });
            if (it != toolbarActions.cend()) {
                action = *it;
            }
        }
        if (source == QLatin1String("toolbar") && !action) {
            return;
        }
        if (source == QLatin1String("palette")) {
            auto *spacer = new QWidgetAction(this);
            spacer->setProperty("toolbarSpacer", true);
            spacer->setProperty("toolbarCustomizerOwned", true);
            auto *widget = new QWidget;
            widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            widget->setMinimumWidth(16);
            spacer->setDefaultWidget(widget);
            action = spacer;
        }
    } else {
        return;
    }

    QAction *before = nullptr;
    for (QAction *candidate : m_toolbar->actions()) {
        if (candidate == action) {
            continue;
        }
        const QRect rect = m_toolbar->actionGeometry(candidate);
        const int coordinate = m_toolbar->orientation() == Qt::Horizontal ? position.x() : position.y();
        const int center = m_toolbar->orientation() == Qt::Horizontal ? rect.center().x() : rect.center().y();
        if (rect.isValid() && coordinate < center) {
            before = candidate;
            break;
        }
    }
    if (m_toolbar->actions().contains(action)) {
        m_toolbar->removeAction(action);
    }
    m_toolbar->insertAction(before, action);
    installButtonFilter(action);
    saveToolbar();
    populateActions();
}

void ToolbarCustomizer::removeToolbarItem(const QString &kind, const QString &name)
{
    QAction *action = nullptr;
    if (kind == QLatin1String("action")) {
        action = m_actions->action(name);
    } else {
        for (QAction *candidate : m_toolbar->actions()) {
            if (kindForAction(candidate) == kind && (name.isEmpty() || candidate->objectName() == name)) {
                action = candidate;
                break;
            }
        }
    }
    if (action && m_toolbar->actions().contains(action)) {
        m_toolbar->removeAction(action);
        saveToolbar();
        populateActions();
        if (action->property("toolbarCustomizerOwned").toBool()) {
            action->deleteLater();
        }
    }
}
