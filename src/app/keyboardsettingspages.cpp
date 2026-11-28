// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "desktoptray.h"
#include "doubleshift.h"
#include "gallerywindow.h"
#include "keybindings.h"
#include "settings.h"

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KStandardAction>
#include <KXMLGUIFactory>
#include <KXmlGuiWindow>

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
/// The actions whose shortcuts KDE's global shortcut service can reserve.
const QStringList GlobalActionNames{QStringLiteral("toggle_window_visibility"), QStringLiteral("toggle_drop_down")};

/**
 * The default shortcuts of @p action as the window applies them. Switch Pane
 * leaves out Tab while Tab doesn't switch panes, so turning that setting off
 * isn't a customized shortcut.
 */
QList<QKeySequence> defaultShortcuts(QAction *action)
{
    QList<QKeySequence> defaults = KActionCollection::defaultShortcuts(action);
    if (action->objectName() == QLatin1String("switch_pane") && !Settings::tabSwitchesPanes()) {
        defaults.removeAll(QKeySequence(Qt::Key_Tab));
    }
    return defaults;
}

QString actionText(const QAction *action)
{
    const QString text = KLocalizedString::removeAcceleratorMarker(action->text());
    return text.isEmpty() ? action->objectName() : text;
}

/// The window's actions whose shortcuts can be configured and their bindings, the gallery's fixed keys, then the global shortcuts KDE has registered.
struct WindowBindings {
    /// The action of each binding, or null for the gallery's keys.
    QList<QAction *> actions;
    QList<KeyBindings::Binding> bindings;
};

WindowBindings windowBindings(KXmlGuiWindow *window)
{
    WindowBindings found;
    if (!window) {
        return found;
    }
    const QList<QAction *> actions = window->actionCollection()->actions();
    for (QAction *action : actions) {
        if (KActionCollection::isShortcutsConfigurable(action)) {
            found.actions.append(action);
            found.bindings.append({action->objectName(), actionText(action), action->shortcuts(), defaultShortcuts(action)});
        }
    }
    for (const KeyBindings::Binding &binding : GalleryWindow::keyBindings()) {
        found.actions.append(nullptr);
        found.bindings.append(binding);
    }
    for (QAction *action : actions) {
        if (GlobalActionNames.contains(action->objectName()) && KGlobalAccel::self()->hasShortcut(action)) {
            found.actions.append(action);
            found.bindings.append(
                {action->objectName(), actionText(action), KGlobalAccel::self()->shortcut(action), KGlobalAccel::self()->defaultShortcut(action), true});
        }
    }
    return found;
}

QString shortcutsText(const QList<QKeySequence> &shortcuts)
{
    QStringList texts;
    for (const QKeySequence &shortcut : KeyBindings::keys(shortcuts)) {
        texts.append(shortcut.toString(QKeySequence::NativeText));
    }
    return texts.isEmpty() ? i18nc("@item no keyboard shortcut", "None") : texts.join(i18nc("@item separator of keyboard shortcuts", "; "));
}

QLabel *notice(const QString &name, const QString &text = {})
{
    auto *label = new QLabel(text);
    label->setObjectName(name);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QPushButton *iconButton(const QString &name, const QString &text, const QString &iconName)
{
    auto *button = new QPushButton(QIcon::fromTheme(iconName), text);
    button->setObjectName(name);
    return button;
}

/// A row of @p status and a button that shows where to change it.
QHBoxLayout *statusRow(QLabel *status, QPushButton *show)
{
    auto *row = new QHBoxLayout;
    row->addWidget(status, 1);
    row->addWidget(show, 0, Qt::AlignTop);
    return row;
}
}

QWidget *SettingsDialog::createKeyboardShortcutsPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("keyboardShortcutsPage"));
    auto *layout = new QVBoxLayout(page);
    auto *window = qobject_cast<KXmlGuiWindow *>(parentWidget());

    auto *customizedGroup = new QGroupBox(i18nc("@title:group", "Customized Shortcuts"), page);
    auto *customizedLayout = new QVBoxLayout(customizedGroup);
    QLabel *customizedSummary = notice(QStringLiteral("keyboardCustomizedSummary"));
    customizedLayout->addWidget(customizedSummary);
    auto *customized = new QTreeWidget(customizedGroup);
    customized->setObjectName(QStringLiteral("keyboardCustomizedShortcuts"));
    customized->setAccessibleName(i18nc("@label accessible name", "Actions whose shortcuts differ from their defaults"));
    customized->setHeaderLabels(
        {i18nc("@title:column", "Action"), i18nc("@title:column", "Shortcuts"), i18nc("@title:column", "Default"), i18nc("@title:column", "Applies in")});
    customized->setRootIsDecorated(false);
    customized->setUniformRowHeights(true);
    customized->setMinimumHeight(customized->fontMetrics().height() * 8);
    customizedLayout->addWidget(customized);
    layout->addWidget(customizedGroup);

    auto *conflictsGroup = new QGroupBox(i18nc("@title:group", "Conflicts"), page);
    auto *conflictsLayout = new QVBoxLayout(conflictsGroup);
    QLabel *conflictSummary = notice(QStringLiteral("keyboardConflictSummary"));
    conflictsLayout->addWidget(conflictSummary);
    auto *conflicts = new QListWidget(conflictsGroup);
    conflicts->setObjectName(QStringLiteral("keyboardConflicts"));
    conflicts->setAccessibleName(i18nc("@label accessible name", "Conflicting keyboard shortcuts"));
    conflicts->setWordWrap(true);
    conflictsLayout->addWidget(conflicts);
    layout->addWidget(conflictsGroup);

    QPushButton *configure = iconButton(QStringLiteral("keyboardConfigureShortcuts"),
                                        i18nc("@action:button", "Configure Keyboard Shortcuts…"),
                                        QStringLiteral("configure-shortcuts"));
    configure->setToolTip(i18nc("@info:tooltip", "Change, add or remove the shortcuts of every action, including key sequences such as Ctrl+K, Ctrl+C"));
    QPushButton *reset = iconButton(QStringLiteral("keyboardResetShortcuts"), i18nc("@action:button", "Reset All Shortcuts…"), QStringLiteral("edit-reset"));
    reset->setToolTip(i18nc("@info:tooltip", "Give every action its default shortcuts; global shortcuts are kept"));
    const QPointer<QAction> shortcutsAction = window ? window->actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)) : nullptr;
    configure->setEnabled(!shortcutsAction.isNull());
    // Preferences stays open, so the summary updates once the shortcuts are saved.
    connect(configure, &QPushButton::clicked, this, [shortcutsAction] {
        if (shortcutsAction) {
            shortcutsAction->trigger();
        }
    });
    connect(reset, &QPushButton::clicked, this, &SettingsDialog::resetShortcutsToDefaults);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(configure);
    buttons->addWidget(reset);
    buttons->addStretch();
    layout->addLayout(buttons);

    auto *otherGroup = new QGroupBox(i18nc("@title:group", "Search, Yazi Mode and Global Shortcuts"), page);
    otherGroup->setObjectName(QStringLiteral("keyboardOtherKeys"));
    auto *otherLayout = new QVBoxLayout(otherGroup);
    QLabel *search = notice(QStringLiteral("keyboardSearchKeys"));
    QPushButton *showSearch = iconButton(QStringLiteral("keyboardShowSearchSettings"), i18nc("@action:button", "Search Settings"), QStringLiteral("edit-find"));
    otherLayout->addLayout(statusRow(search, showSearch));
    QLabel *yazi = notice(QStringLiteral("keyboardYaziKeys"));
    QPushButton *showYazi = iconButton(QStringLiteral("keyboardShowYaziKeys"), i18nc("@action:button", "Yazi Mode Keys"), QStringLiteral("view-file-columns"));
    otherLayout->addLayout(statusRow(yazi, showYazi));
    QLabel *global = notice(QStringLiteral("keyboardGlobalKeys"));
    QPushButton *showGlobal = iconButton(QStringLiteral("keyboardShowGlobalShortcuts"),
                                         i18nc("@action:button", "Global Shortcut Settings"),
                                         QStringLiteral("preferences-desktop-keyboard-shortcut"));
    otherLayout->addLayout(statusRow(global, showGlobal));
    layout->addWidget(otherGroup);
    connect(showSearch, &QPushButton::clicked, this, [this] {
        showSettingsTab(QStringLiteral("searchSettingsPage"));
    });
    connect(showYazi, &QPushButton::clicked, this, [this] {
        showSettingsTab(QStringLiteral("yaziKeyboardPage"));
    });
    connect(showGlobal, &QPushButton::clicked, this, [this] {
        showSettingsTab(QStringLiteral("shortcutsIntegrationPage"));
    });

    layout->addWidget(notice(QStringLiteral("keyboardShortcutsNotice"),
                             i18nc("@info",
                                   "Configure Keyboard Shortcuts gives each action a shortcut and an alternate one, each a key "
                                   "combination such as Ctrl+Alt+F or a sequence of up to four, such as Ctrl+K, Ctrl+C. It warns "
                                   "before taking a shortcut from another action and restores the defaults of one action or all "
                                   "of them. Shortcuts take effect as soon as they are saved there.")));
    layout->addWidget(notice(QStringLiteral("keyboardContextsNotice"),
                             i18nc("@info",
                                   "Window shortcuts apply anywhere in the window, except that a text field keeps the keys used "
                                   "for typing and editing, including Tab and Shift+Tab to move the focus, and lists and the "
                                   "folder tree keep the arrow keys, Home, End, Page Up and Page Down. The embedded terminal "
                                   "decides which keys it keeps. The image gallery has its own keys, which apply only in its "
                                   "window, and global shortcuts apply everywhere.")));
    layout->addStretch();

    m_refreshKeyboardShortcuts = [this, window, customizedSummary, customized, conflictSummary, conflicts, reset, search, yazi, global] {
        const WindowBindings found = windowBindings(window);
        const QList<int> changed = KeyBindings::customized(found.bindings);
        customized->clear();
        for (int index : changed) {
            const KeyBindings::Binding &binding = found.bindings.at(index);
            if (binding.global) {
                continue;
            }
            auto *item =
                new QTreeWidgetItem(customized,
                                    {binding.text, shortcutsText(binding.shortcuts), shortcutsText(binding.defaults), KeyBindings::contextText(binding)});
            item->setData(0, Qt::UserRole, binding.name);
            if (const QAction *action = found.actions.at(index)) {
                item->setIcon(0, action->icon());
            }
        }
        customized->resizeColumnToContents(0);
        customized->setVisible(customized->topLevelItemCount() > 0);
        reset->setEnabled(window && customized->topLevelItemCount() > 0);
        if (!window) {
            customizedSummary->setText(i18nc("@info", "Open Preferences from a Plasma Opus window to see and change its keyboard shortcuts."));
        } else if (customized->topLevelItemCount() == 0) {
            customizedSummary->setText(i18nc("@info", "Every action has its default shortcuts."));
        } else {
            customizedSummary->setText(i18ncp("@info",
                                              "1 action has shortcuts other than its defaults:",
                                              "%1 actions have shortcuts other than their defaults:",
                                              customized->topLevelItemCount()));
        }

        const QList<KeyBindings::Conflict> conflictList = KeyBindings::conflicts(found.bindings);
        conflicts->clear();
        for (const KeyBindings::Conflict &conflict : conflictList) {
            conflicts->addItem(KeyBindings::conflictText(conflict, found.bindings));
        }
        conflicts->setVisible(conflicts->count() > 0);
        conflictSummary->setText(conflicts->count() == 0
                                     ? i18nc("@info", "No conflicts: every shortcut runs its action.")
                                     : i18ncp("@info",
                                              "1 conflict. Give one of the actions another shortcut in Configure Keyboard Shortcuts:",
                                              "%1 conflicts. Give one of the actions in each another shortcut in Configure Keyboard Shortcuts:",
                                              conflicts->count()));

        const QAction *searchAction = window ? window->actionCollection()->action(QStringLiteral("search_folder")) : nullptr;
        const QString searchKeys = searchAction ? shortcutsText(searchAction->shortcuts()) : i18nc("@item no keyboard shortcut", "None");
        const QString unavailable = DoubleShiftDetector::unavailableReason();
        if (!Settings::doubleShiftSearch()) {
            search->setText(i18nc("@info %1 shortcuts", "Search Current Folder: %1. Pressing Shift twice to search is off.", searchKeys));
        } else if (!unavailable.isEmpty()) {
            search->setText(i18nc("@info %1 shortcuts, %2 reason",
                                  "Search Current Folder: %1. Pressing Shift twice to search is unavailable: %2",
                                  searchKeys,
                                  unavailable));
        } else {
            search->setText(i18nc("@info %1 shortcuts", "Search Current Folder: %1. Pressing Shift twice also searches.", searchKeys));
        }

        const int shadowed = yaziShadowedShortcutCount(std::max(0, m_yaziKeyPreset->currentIndex()));
        yazi->setText(shadowed == 0 ? i18nc("@info", "The chosen Yazi mode key preset leaves every window shortcut working.")
                                    : i18ncp("@info",
                                             "The chosen Yazi mode key preset takes 1 window shortcut while a Yazi mode column has the keyboard focus.",
                                             "The chosen Yazi mode key preset takes %1 window shortcuts while a Yazi mode column has the keyboard focus.",
                                             shadowed));

        QStringList globalLines;
        if (!window) {
            globalLines.append(i18nc("@info", "Open Preferences from a Plasma Opus window to see its global shortcuts."));
        } else if (!DesktopTray::globalShortcutServiceAvailable()) {
            globalLines.append(i18nc("@info", "KDE's global shortcut service is not running, so global shortcuts don't work in this session."));
        } else {
            for (const QString &name : GlobalActionNames) {
                QAction *action = window->actionCollection()->action(name);
                if (!action) {
                    continue;
                }
                const QList<QKeySequence> keys = KGlobalAccel::self()->hasShortcut(action) ? KGlobalAccel::self()->shortcut(action) : QList<QKeySequence>();
                globalLines.append(i18nc("@info %1 action, %2 shortcuts", "%1: %2", actionText(action), shortcutsText(keys)));
            }
        }
        global->setText(globalLines.join(QLatin1Char('\n')));
    };

    if (window && window->guiFactory()) {
        connect(window->guiFactory(), &KXMLGUIFactory::shortcutsSaved, page, [this] {
            m_refreshKeyboardShortcuts();
        });
    }
    connect(m_yaziKeyPreset, &QComboBox::currentIndexChanged, page, [this] {
        m_refreshKeyboardShortcuts();
    });
    // Saved settings, such as Tab switching panes or pressing Shift twice, change what is listed once the window applies them.
    connect(Settings::self(), &Settings::configChanged, page, [this, page] {
        QTimer::singleShot(0, page, [this] {
            m_refreshKeyboardShortcuts();
        });
    });
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, page, [this] {
        m_refreshKeyboardShortcuts();
    });
    m_refreshKeyboardShortcuts();
    return page;
}

void SettingsDialog::setShortcutResetConfirmation(std::function<bool(const QString &)> confirm)
{
    m_confirmShortcutReset = std::move(confirm);
}

void SettingsDialog::resetShortcutsToDefaults()
{
    auto *window = qobject_cast<KXmlGuiWindow *>(parentWidget());
    if (!window) {
        return;
    }
    const WindowBindings found = windowBindings(window);
    int changed = 0;
    for (const KeyBindings::Binding &binding : found.bindings) {
        changed += !binding.global && KeyBindings::isCustomized(binding);
    }
    if (changed == 0
        || !m_confirmShortcutReset(i18ncp("@info",
                                          "Give the action with customized shortcuts its default shortcuts again? This takes effect "
                                          "right away. Global shortcuts are kept.",
                                          "Give the %1 actions with customized shortcuts their default shortcuts again? This takes "
                                          "effect right away. Global shortcuts are kept.",
                                          changed))) {
        return;
    }
    for (int i = 0; i < found.bindings.size(); ++i) {
        if (!found.bindings.at(i).global && found.actions.at(i)) {
            found.actions.at(i)->setShortcuts(found.bindings.at(i).defaults);
        }
    }
    window->actionCollection()->writeSettings();
    if (window->guiFactory()) {
        // Everything that follows Configure Keyboard Shortcuts, such as the Yazi Mode conflicts, updates too.
        Q_EMIT window->guiFactory()->shortcutsSaved();
    } else {
        m_refreshKeyboardShortcuts();
    }
}
