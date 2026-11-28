// SPDX-License-Identifier: GPL-3.0-or-later

#include "windowshortcuts.h"

#include "gallerywindow.h"
#include "navigationshortcuts.h"
#include "settings.h"
#include "shortcutstorage.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KXmlGuiWindow>

#include <QAction>
#include <QDebug>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace WindowShortcuts
{
namespace
{
KConfigGroup group()
{
    return KSharedConfig::openConfig()->group(QLatin1String(ShortcutStorage::Group));
}

/// Keeps an action's defaults without the extra navigation keys, which change its KActionCollection defaults.
constexpr const char *BaseDefaultsProperty = "_opus_baseDefaultShortcuts";

/// The actions of @p window whose shortcuts KDE's editor lists.
QList<QAction *> configurableActions(KXmlGuiWindow *window)
{
    QList<QAction *> found;
    if (!window) {
        return found;
    }
    const QList<QAction *> actions = window->actionCollection()->actions();
    for (QAction *action : actions) {
        // KXmlGui doesn't save actions it names itself, as their names change.
        if (KActionCollection::isShortcutsConfigurable(action) && !action->objectName().isEmpty()
            && !action->objectName().startsWith(QLatin1String("unnamed-"))) {
            found.append(action);
        }
    }
    return found;
}

/// The current shortcut scheme, which KXmlGui saves shortcuts for.
QString shortcutScheme()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("Shortcut Schemes")).readEntry("Current Scheme", QStringLiteral("Default"));
}

/// Takes the shortcuts out of the ui.rc file at @p path, which KDE's editor writes.
ShortcutStorage::XmlGuiShortcuts takeFromUiRc(const QString &path)
{
    QFile file(path);
    if (path.isEmpty() || !file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    ShortcutStorage::XmlGuiShortcuts found = ShortcutStorage::takeXmlGuiShortcuts(QString::fromUtf8(file.readAll()), shortcutScheme());
    file.close();
    if (found.stripped.isEmpty()) {
        return found;
    }
    QSaveFile stripped(path);
    if (!stripped.open(QIODevice::WriteOnly) || stripped.write(found.stripped.toUtf8()) < 0 || !stripped.commit()) {
        // The shortcuts are stored anyway; the file's are the same until KDE's editor saves again.
        qWarning() << "Could not remove the shortcuts from" << path << stripped.errorString();
    }
    return found;
}
}

QList<QKeySequence> defaults(QAction *action)
{
    QList<QKeySequence> defaults = KActionCollection::defaultShortcuts(action);
    if (action->objectName() == QLatin1String("switch_pane") && !Settings::tabSwitchesPanes()) {
        defaults.removeAll(QKeySequence(Qt::Key_Tab));
    }
    return defaults;
}

QStringList globalActionNames()
{
    return {QStringLiteral("toggle_window_visibility"), QStringLiteral("toggle_drop_down")};
}

Bindings bindings(KXmlGuiWindow *window)
{
    const auto actionText = [](const QAction *action) {
        const QString text = KLocalizedString::removeAcceleratorMarker(action->text());
        return text.isEmpty() ? action->objectName() : text;
    };
    Bindings found;
    if (!window) {
        return found;
    }
    const QList<QAction *> actions = window->actionCollection()->actions();
    for (QAction *action : actions) {
        if (KActionCollection::isShortcutsConfigurable(action)) {
            found.actions.append(action);
            found.bindings.append({action->objectName(), actionText(action), action->shortcuts(), defaults(action)});
        }
    }
    for (const KeyBindings::Binding &binding : GalleryWindow::keyBindings()) {
        found.actions.append(nullptr);
        found.bindings.append(binding);
    }
    const QStringList globalNames = globalActionNames();
    for (QAction *action : actions) {
        if (globalNames.contains(action->objectName()) && KGlobalAccel::self()->hasShortcut(action)) {
            found.actions.append(action);
            found.bindings.append(
                {action->objectName(), actionText(action), KGlobalAccel::self()->shortcut(action), KGlobalAccel::self()->defaultShortcut(action), true});
        }
    }
    return found;
}

QString uiRcPath(const QString &componentName, const QString &xmlFile)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kxmlgui5/%1/%2").arg(componentName, xmlFile);
}

void migrate(const QString &uiRcPath)
{
    KConfigGroup shortcuts = group();
    const ShortcutStorage::XmlGuiShortcuts found = takeFromUiRc(uiRcPath);
    if (!found.shortcuts.isEmpty() || !ShortcutStorage::isCurrent(shortcuts)) {
        ShortcutStorage::migrate(shortcuts, found);
        shortcuts.sync();
    }
}

void apply(KXmlGuiWindow *window)
{
    const KConfigGroup shortcuts = group();
    for (QAction *action : configurableActions(window)) {
        const auto stored = ShortcutStorage::read(shortcuts, action->objectName());
        const QList<QKeySequence> keys = stored ? *stored : defaults(action);
        if (action->shortcuts() != keys) {
            action->setShortcuts(keys);
        }
    }
    applyNavigationKeys(window);
}

void apply(QAction *action)
{
    if (!action || !KActionCollection::isShortcutsConfigurable(action)) {
        return;
    }
    if (const auto stored = ShortcutStorage::read(group(), action->objectName())) {
        action->setShortcuts(*stored);
    }
}

void save(KXmlGuiWindow *window)
{
    if (!window) {
        return;
    }
    KConfigGroup shortcuts = group();
    for (QAction *action : configurableActions(window)) {
        ShortcutStorage::write(shortcuts, action->objectName(), action->shortcuts(), defaults(action));
    }
    ShortcutStorage::markCurrent(shortcuts);
    shortcuts.sync();
    // The actions already have what KDE's editor saved there, which is now stored.
    takeFromUiRc(window->localXMLFile());
}

void forget(const QString &name)
{
    KConfigGroup shortcuts = group();
    if (!name.isEmpty() && name != QLatin1String(ShortcutStorage::VersionKey) && shortcuts.hasKey(name)) {
        shortcuts.deleteEntry(name);
        shortcuts.sync();
    }
}

void prepareRebuild(KXmlGuiWindow *window)
{
    const KConfigGroup shortcuts = group();
    for (QAction *action : configurableActions(window)) {
        if (shortcuts.hasKey(action->objectName())) {
            action->setShortcuts(KActionCollection::defaultShortcuts(action));
        }
    }
}

QList<QKeySequence> baseDefaults(QAction *action)
{
    if (!action) {
        return {};
    }
    const QVariant stored = action->property(BaseDefaultsProperty);
    if (stored.isValid()) {
        return stored.value<QList<QKeySequence>>();
    }
    const QList<QKeySequence> base = KActionCollection::defaultShortcuts(action);
    action->setProperty(BaseDefaultsProperty, QVariant::fromValue(base));
    return base;
}

void applyNavigationKeys(KXmlGuiWindow *window)
{
    if (!window) {
        return;
    }
    const bool enabled = Settings::extraNavigationShortcuts();
    Bindings found;
    bool listed = false;
    for (const NavigationShortcuts::Action &navigation : NavigationShortcuts::actions()) {
        QAction *action = window->actionCollection()->action(navigation.name);
        if (navigation.extra.isEmpty() || !action) {
            continue;
        }
        const QList<QKeySequence> base = baseDefaults(action);
        if (!enabled && KActionCollection::defaultShortcuts(action) == base) {
            continue;
        }
        if (!listed) {
            found = bindings(window);
            listed = true;
        }
        const int self = found.actions.indexOf(action);
        const bool taken = enabled && NavigationShortcuts::user(found.bindings, self, navigation.extra) >= 0;
        const QList<QKeySequence> wanted = NavigationShortcuts::defaults(base, navigation.extra, enabled, taken);
        if (KActionCollection::defaultShortcuts(action) == wanted) {
            continue;
        }
        // This also gives the action its new defaults as its shortcuts; apply() gives it its stored ones again.
        KActionCollection::setDefaultShortcuts(action, wanted);
        apply(action);
        if (self >= 0) {
            found.bindings[self].shortcuts = action->shortcuts();
            found.bindings[self].defaults = wanted;
        }
    }
}
}
