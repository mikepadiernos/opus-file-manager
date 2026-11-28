// SPDX-License-Identifier: GPL-3.0-or-later

#include "settingsdialog.h"

#include "commandpalette.h"
#include "desktoptray.h"
#include "doubleshift.h"
#include "keybindings.h"
#include "keymapfile.h"
#include "keymapimportdialog.h"
#include "navigationshortcuts.h"
#include "settings.h"
#include "settingswidgets.h"
#include "vimnavigation.h"
#include "windowshortcuts.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStandardAction>
#include <KXMLGUIFactory>
#include <KXmlGuiWindow>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace
{
QString actionText(const QAction *action)
{
    const QString text = KLocalizedString::removeAcceleratorMarker(action->text());
    return text.isEmpty() ? action->objectName() : text;
}

using WindowBindings = WindowShortcuts::Bindings;

WindowBindings windowBindings(KXmlGuiWindow *window)
{
    return WindowShortcuts::bindings(window);
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

/// Calls a function each time the watched widget is shown, such as when its tab is chosen.
class ShowWatcher : public QObject
{
public:
    ShowWatcher(QWidget *widget, std::function<void()> shown)
        : QObject(widget)
        , m_shown(std::move(shown))
    {
        widget->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parent() && event->type() == QEvent::Show) {
            m_shown();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_shown;
};
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
    QPushButton *map = iconButton(QStringLiteral("keyboardShowMap"), i18nc("@action:button", "Keyboard Map…"), QStringLiteral("help-keybord-shortcuts"));
    map->setToolTip(i18nc("@info:tooltip", "Search every action and its shortcuts by name or by pressing keys, and export a printable reference"));
    const QPointer<QAction> mapAction = window ? window->actionCollection()->action(QStringLiteral("keyboard_map")) : nullptr;
    map->setEnabled(!mapAction.isNull());
    connect(map, &QPushButton::clicked, this, [mapAction] {
        if (mapAction) {
            mapAction->trigger();
        }
    });
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(configure);
    buttons->addWidget(reset);
    buttons->addWidget(map);
    buttons->addStretch();
    layout->addLayout(buttons);

    auto *keymapGroup = new QGroupBox(i18nc("@title:group", "Import and Export"), page);
    keymapGroup->setObjectName(QStringLiteral("keyboardKeymap"));
    auto *keymapLayout = new QVBoxLayout(keymapGroup);
    keymapLayout->addWidget(notice(QStringLiteral("keyboardKeymapNotice"),
                                   i18nc("@info",
                                         "Export saves the customized shortcuts and the keyboard settings to a keymap file. "
                                         "Import shows what a keymap file changes and which conflicts it adds before applying "
                                         "it; nothing in the file runs.")));
    QPushButton *importKeymap = iconButton(QStringLiteral("keyboardImportShortcuts"), i18nc("@action:button", "Import…"), QStringLiteral("document-import"));
    importKeymap->setToolTip(i18nc("@info:tooltip", "Preview and apply the shortcuts and keyboard settings of a keymap file"));
    QPushButton *exportKeymap = iconButton(QStringLiteral("keyboardExportShortcuts"), i18nc("@action:button", "Export…"), QStringLiteral("document-export"));
    exportKeymap->setToolTip(i18nc("@info:tooltip", "Save the customized shortcuts and the keyboard settings to a keymap file"));
    importKeymap->setEnabled(window);
    exportKeymap->setEnabled(window);
    connect(importKeymap, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this,
                                                          i18nc("@title:window", "Import Keyboard Shortcuts"),
                                                          QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                          i18nc("@item:inlistbox", "Plasma Opus keymaps (*.json)"));
        if (!path.isEmpty()) {
            importShortcuts(path);
        }
    });
    connect(exportKeymap, &QPushButton::clicked, this, [this] {
        QString path = QFileDialog::getSaveFileName(
            this,
            i18nc("@title:window", "Export Keyboard Shortcuts"),
            QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("plasma-opus-keymap.json")),
            i18nc("@item:inlistbox", "Plasma Opus keymaps (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        if (QFileInfo(path).suffix().isEmpty()) {
            path += QLatin1String(".json");
        }
        exportShortcuts(path);
    });
    auto *keymapButtons = new QHBoxLayout;
    keymapButtons->addWidget(importKeymap);
    keymapButtons->addWidget(exportKeymap);
    keymapButtons->addStretch();
    keymapLayout->addLayout(keymapButtons);
    m_keymapStatus = notice(QStringLiteral("keyboardKeymapStatus"));
    m_keymapStatus->setVisible(false);
    keymapLayout->addWidget(m_keymapStatus);
    layout->addWidget(keymapGroup);
    if (!m_reviewKeymapImport) {
        m_reviewKeymapImport = [](KeymapImportDialog &dialog) {
            return dialog.exec() == QDialog::Accepted;
        };
    }

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
                                   "of them. Shortcuts take effect as soon as they are saved there, in every Plasma Opus window, "
                                   "and are stored in the Shortcuts group of opus-file-managerrc.")));
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
        const KeyBindings::GlobalAvailability availability = DesktopTray::globalShortcutAvailability();
        if (!window) {
            globalLines.append(i18nc("@info", "Open Preferences from a Plasma Opus window to see its global shortcuts."));
        } else if (availability == KeyBindings::GlobalAvailability::NoService) {
            globalLines.append(KeyBindings::globalAvailabilityText(availability));
        } else {
            for (const QString &name : WindowShortcuts::globalActionNames()) {
                QAction *action = window->actionCollection()->action(name);
                if (!action) {
                    continue;
                }
                const QList<QKeySequence> keys = KGlobalAccel::self()->hasShortcut(action) ? KGlobalAccel::self()->shortcut(action) : QList<QKeySequence>();
                if (keys.isEmpty() && name == WindowShortcuts::globalActionNames().constFirst() && Settings::globalShortcut()) {
                    globalLines.append(i18nc("@info %1 action",
                                             "%1: none. KDE didn't assign a key combination, usually because another application "
                                             "already uses it.",
                                             actionText(action)));
                } else {
                    globalLines.append(i18nc("@info %1 action, %2 shortcuts", "%1: %2", actionText(action), shortcutsText(keys)));
                }
            }
            const QString limitation = KeyBindings::globalAvailabilityText(availability);
            if (!limitation.isEmpty()) {
                globalLines.append(limitation);
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

QWidget *SettingsDialog::createKeyboardNavigationPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("keyboardNavigationPage"));
    auto *layout = new QVBoxLayout(page);
    auto *window = qobject_cast<KXmlGuiWindow *>(parentWidget());

    layout->addWidget(notice(QStringLiteral("keyboardNavigationIntro"),
                             i18nc("@info",
                                   "These actions move between the panes and tabs, change the selection and filter, and move the "
                                   "keyboard focus to the preview panel or the embedded terminal. Those without a shortcut have "
                                   "none by default, so they don't take a key you already use; give them one in Configure "
                                   "Keyboard Shortcuts, or turn on the extra keys below.")));
    layout->addWidget(SettingsWidgets::checkBox(QStringLiteral("ExtraNavigationShortcuts"),
                                                i18nc("@option:check", "Add the extra navigation shortcuts that no other action uses")));
    layout->addWidget(notice(QStringLiteral("keyboardNavigationExtraNotice"),
                             i18nc("@info",
                                   "Each extra key is added as one more default shortcut of its action. A key that another "
                                   "action uses, including one of your own shortcuts, is left out, and your own shortcuts "
                                   "for these actions are kept. Ctrl+Page Down and Ctrl+Page Up are KDE's standard keys for "
                                   "the next and previous tab, Ctrl+Shift+A its standard Deselect and Ctrl+Shift+F4 the key "
                                   "Dolphin uses to focus its terminal.")));

    auto *tree = new QTreeWidget(page);
    tree->setObjectName(QStringLiteral("keyboardNavigationShortcuts"));
    tree->setAccessibleName(i18nc("@label accessible name", "Navigation actions, their shortcuts and extra keys"));
    tree->setHeaderLabels(
        {i18nc("@title:column", "Action"), i18nc("@title:column", "Shortcuts"), i18nc("@title:column", "Extra Key"), i18nc("@title:column", "Status")});
    tree->setUniformRowHeights(true);
    tree->setMinimumHeight(tree->fontMetrics().height() * 12);
    layout->addWidget(tree, 1);

    layout->addWidget(notice(QStringLiteral("keyboardNavigationTerminalNotice"),
                             i18nc("@info",
                                   "While you type in the embedded terminal, it keeps keys with a single modifier, such as "
                                   "Alt+1 and Ctrl+Page Down, for itself; Ctrl+Shift+F4 leaves it again.")));

    QPushButton *configure = iconButton(QStringLiteral("keyboardNavigationConfigure"),
                                        i18nc("@action:button", "Configure Keyboard Shortcuts…"),
                                        QStringLiteral("configure-shortcuts"));
    const QPointer<QAction> shortcutsAction = window ? window->actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)) : nullptr;
    configure->setEnabled(!shortcutsAction.isNull());
    connect(configure, &QPushButton::clicked, this, [shortcutsAction] {
        if (shortcutsAction) {
            shortcutsAction->trigger();
        }
    });
    QPushButton *map = iconButton(QStringLiteral("keyboardNavigationMap"), i18nc("@action:button", "Keyboard Map…"), QStringLiteral("help-keybord-shortcuts"));
    const QPointer<QAction> mapAction = window ? window->actionCollection()->action(QStringLiteral("keyboard_map")) : nullptr;
    map->setEnabled(!mapAction.isNull());
    connect(map, &QPushButton::clicked, this, [mapAction] {
        if (mapAction) {
            mapAction->trigger();
        }
    });
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(configure);
    buttons->addWidget(map);
    buttons->addStretch();
    layout->addLayout(buttons);

    m_refreshKeyboardNavigation = [window, tree] {
        tree->clear();
        if (!window) {
            auto *item = new QTreeWidgetItem(tree, {i18nc("@info", "Open Preferences from a Plasma Opus window to see its navigation shortcuts.")});
            item->setFirstColumnSpanned(true);
            return;
        }
        const WindowBindings found = windowBindings(window);
        const bool enabled = Settings::extraNavigationShortcuts();
        QTreeWidgetItem *areaItem = nullptr;
        std::optional<NavigationShortcuts::Area> area;
        for (const NavigationShortcuts::Action &navigation : NavigationShortcuts::actions()) {
            QAction *action = window->actionCollection()->action(navigation.name);
            const int index = action ? found.actions.indexOf(action) : -1;
            if (index < 0) {
                continue;
            }
            if (area != navigation.area) {
                area = navigation.area;
                areaItem = new QTreeWidgetItem(tree, {NavigationShortcuts::areaText(navigation.area)});
                areaItem->setFirstColumnSpanned(true);
                areaItem->setFlags(Qt::ItemIsEnabled);
            }
            const NavigationShortcuts::State state = NavigationShortcuts::state(found.bindings, index, navigation.extra, enabled);
            const QString extra = navigation.extra.isEmpty() ? QString() : navigation.extra.toString(QKeySequence::NativeText);
            auto *item = new QTreeWidgetItem(areaItem,
                                             {found.bindings.at(index).text,
                                              shortcutsText(found.bindings.at(index).shortcuts),
                                              extra,
                                              NavigationShortcuts::stateText(state, found.bindings)});
            item->setData(0, Qt::UserRole, navigation.name);
            item->setData(3, Qt::UserRole, static_cast<int>(state.status));
            item->setIcon(0, action->icon());
        }
        tree->expandAll();
        for (int column = 0; column < tree->columnCount() - 1; ++column) {
            tree->resizeColumnToContents(column);
        }
    };

    if (window && window->guiFactory()) {
        connect(window->guiFactory(), &KXMLGUIFactory::shortcutsSaved, page, [this] {
            m_refreshKeyboardNavigation();
        });
    }
    // The window gives the actions their extra keys once the setting is saved.
    connect(Settings::self(), &Settings::configChanged, page, [this, page] {
        QTimer::singleShot(0, page, [this] {
            m_refreshKeyboardNavigation();
        });
    });
    m_refreshKeyboardNavigation();
    return page;
}

QWidget *SettingsDialog::createKeyboardVimPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("keyboardVimPage"));
    auto *layout = new QVBoxLayout(page);
    auto *window = qobject_cast<KXmlGuiWindow *>(parentWidget());

    layout->addWidget(notice(QStringLiteral("keyboardVimIntro"),
                             i18nc("@info",
                                   "Move through the file views of panes that aren't in Yazi mode with Vim-style keys: j and k "
                                   "move down and up, h opens the parent folder and l the focused item. The keys apply only while "
                                   "a file view has the keyboard focus. View › Vim-Style Navigation turns them on or off too.")));
    layout->addWidget(SettingsWidgets::checkBox(QStringLiteral("VimNavigation"), i18nc("@option:check", "Use Vim-style keys in file views")));

    auto *keysGroup = new QGroupBox(i18nc("@title:group", "Keys"), page);
    auto *keysLayout = new QVBoxLayout(keysGroup);
    auto *keys = new QTreeWidget(keysGroup);
    keys->setObjectName(QStringLiteral("keyboardVimKeys"));
    keys->setAccessibleName(i18nc("@label accessible name", "Vim-style keys and their commands"));
    keys->setHeaderLabels({i18nc("@title:column", "Keys"), i18nc("@title:column", "Command")});
    keys->setRootIsDecorated(false);
    keys->setUniformRowHeights(true);
    keys->setMinimumHeight(keys->fontMetrics().height() * 12);
    // One row per command, listing its keys, in the order of the keymap.
    QList<std::pair<YaziMode::Command, QStringList>> rows;
    for (const YaziMode::Binding &binding : VimNavigation::keymap().bindings) {
        auto row = std::find_if(rows.begin(), rows.end(), [&binding](const auto &candidate) {
            return candidate.first == binding.command;
        });
        if (row == rows.end()) {
            rows.append({binding.command, {}});
            row = rows.end() - 1;
        }
        row->second.append(YaziMode::sequenceText(binding.keys));
    }
    for (const auto &[command, texts] : std::as_const(rows)) {
        auto *item = new QTreeWidgetItem(keys, {texts.join(i18nc("@item separator of keys", ", ")), YaziMode::commandText(command)});
        item->setData(0, Qt::UserRole, YaziMode::commandId(command));
    }
    keys->resizeColumnToContents(0);
    keysLayout->addWidget(keys);
    keysLayout->addWidget(notice(QStringLiteral("keyboardVimCountsNotice"),
                                 i18nc("@info",
                                       "Type a number before a movement key to repeat it, as in 5j. While the keys are on, digits "
                                       "from 1 to 9 begin a count and the letters listed here don't start filtering as you type; "
                                       "press / or i to filter. Other letters still filter as you type. The arrow keys, Enter, Tab and "
                                       "Escape work as before.")));
    keysLayout->addWidget(notice(QStringLiteral("keyboardVimModesNotice"),
                                 i18nc("@info",
                                       "Each pane shows its mode at the bottom: NORMAL while the keys move through the items, with "
                                       "a count or the first key of a sequence while it waits for the rest, FILTER in the filter "
                                       "bar and INSERT in another text field, such as the location bar. In the filter bar, Escape "
                                       "returns to the items and keeps the filter; press \\ to clear it.")));
    layout->addWidget(keysGroup, 1);

    auto *conflictsGroup = new QGroupBox(i18nc("@title:group", "Conflicts"), page);
    auto *conflictsLayout = new QVBoxLayout(conflictsGroup);
    QLabel *conflictSummary = notice(QStringLiteral("keyboardVimConflictSummary"));
    conflictsLayout->addWidget(conflictSummary);
    auto *conflicts = new QListWidget(conflictsGroup);
    conflicts->setObjectName(QStringLiteral("keyboardVimConflicts"));
    conflicts->setAccessibleName(i18nc("@label accessible name", "Conflicts of the Vim-style keys"));
    conflicts->setWordWrap(true);
    conflictsLayout->addWidget(conflicts);
    conflictsLayout->addWidget(notice(QStringLiteral("keyboardVimConflictsNotice"),
                                      i18nc("@info",
                                            "A window shortcut that uses a plain key listed here doesn't run while a file view has the "
                                            "keyboard focus; it still runs from the menus and toolbars, and from the keyboard when the "
                                            "focus is elsewhere. Keys with Ctrl, Alt or Meta, such as Ctrl+D, never replace a window "
                                            "shortcut: the shortcut keeps running, and the Vim-style key works only while no shortcut "
                                            "uses it.")));
    layout->addWidget(conflictsGroup);

    QPushButton *configure =
        iconButton(QStringLiteral("keyboardVimConfigure"), i18nc("@action:button", "Configure Keyboard Shortcuts…"), QStringLiteral("configure-shortcuts"));
    const QPointer<QAction> shortcutsAction = window ? window->actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)) : nullptr;
    configure->setEnabled(!shortcutsAction.isNull());
    connect(configure, &QPushButton::clicked, this, [shortcutsAction] {
        if (shortcutsAction) {
            shortcutsAction->trigger();
        }
    });
    QPushButton *map = iconButton(QStringLiteral("keyboardVimMap"), i18nc("@action:button", "Keyboard Map…"), QStringLiteral("help-keybord-shortcuts"));
    const QPointer<QAction> mapAction = window ? window->actionCollection()->action(QStringLiteral("keyboard_map")) : nullptr;
    map->setEnabled(!mapAction.isNull());
    connect(map, &QPushButton::clicked, this, [mapAction] {
        if (mapAction) {
            mapAction->trigger();
        }
    });
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(configure);
    buttons->addWidget(map);
    buttons->addStretch();
    layout->addLayout(buttons);

    layout->addWidget(notice(QStringLiteral("keyboardVimAccessibilityNotice"),
                             i18nc("@info",
                                   "Keys are never taken from typing: not in the filter bar, the location bar, search fields, "
                                   "the terminal, input methods or while renaming an item in place. A pane in Yazi mode uses "
                                   "Yazi mode's key preset instead, chosen under Yazi Mode › Keyboard.")));

    m_refreshKeyboardVim = [this, conflictSummary, conflicts] {
        const YaziMode::Keymap keymap = VimNavigation::keymap();
        const QList<YaziMode::ActionShortcuts> actions = windowActionShortcuts();
        QStringList found = keymapConflicts(keymap, VimNavigation::shadowedShortcuts(keymap, actions));
        const QList<YaziMode::ShadowedShortcut> yielded = VimNavigation::yieldedShortcuts(keymap, actions);
        for (const YaziMode::ShadowedShortcut &shortcut : yielded) {
            if (shortcut.bindings.isEmpty()) {
                continue;
            }
            found.append(i18nc("@item %1 shortcut, %2 window action, %3 Vim-style command",
                               "%1 runs “%2”, so it doesn't run “%3”",
                               shortcut.shortcut.toString(QKeySequence::NativeText),
                               shortcut.text,
                               YaziMode::commandText(keymap.bindings.at(shortcut.bindings.first()).command)));
        }
        conflicts->clear();
        conflicts->addItems(found);
        conflicts->setVisible(!found.isEmpty());
        conflictSummary->setText(found.isEmpty() ? i18nc("@info", "No conflicts: every Vim-style key works, and so do the window's shortcuts.")
                                                 : i18ncp("@info",
                                                          "The Vim-style keys have 1 conflict while a file view has the keyboard focus:",
                                                          "The Vim-style keys have %1 conflicts while a file view has the keyboard focus:",
                                                          found.size()));
    };
    if (window && window->guiFactory()) {
        connect(window->guiFactory(), &KXMLGUIFactory::shortcutsSaved, page, [this] {
            m_refreshKeyboardVim();
        });
    }
    // Saved settings, such as the extra navigation shortcuts, change the window's shortcuts once the window applies them.
    connect(Settings::self(), &Settings::configChanged, page, [this, page] {
        QTimer::singleShot(0, page, [this] {
            m_refreshKeyboardVim();
        });
    });
    m_refreshKeyboardVim();
    return page;
}

QWidget *SettingsDialog::createKeyboardCommandPalettePage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("keyboardCommandPalettePage"));
    auto *layout = new QVBoxLayout(page);
    auto *window = qobject_cast<KXmlGuiWindow *>(parentWidget());

    layout->addWidget(notice(QStringLiteral("keyboardCommandPaletteIntro"),
                             i18nc("@info",
                                   "The command palette, under Help › Command Palette, lists the actions of the menus and "
                                   "toolbars by name, with their shortcuts. Type part of a name and press Enter to run the "
                                   "action in the pane that had the keyboard focus; unavailable actions aren't listed, and "
                                   "the recently used ones come first.")));
    QLabel *shortcut = notice(QStringLiteral("keyboardCommandPaletteShortcut"));
    layout->addWidget(shortcut);
    layout->addWidget(SettingsWidgets::checkBox(QStringLiteral("CommandPaletteSavedCommands"), i18nc("@option:check", "List saved commands")));
    layout->addWidget(notice(QStringLiteral("keyboardCommandPaletteSavedNotice"),
                             i18nc("@info",
                                   "Choosing a saved command opens it in the Scripts and Custom Commands window to preview, "
                                   "as its own shortcut does; it runs only once you start it there.")));

    auto *historyGroup = new QGroupBox(i18nc("@title:group", "Recently Used"), page);
    auto *historyLayout = new QVBoxLayout(historyGroup);
    QLabel *historySummary = notice(QStringLiteral("keyboardCommandPaletteHistorySummary"));
    historyLayout->addWidget(historySummary);
    auto *history = new QListWidget(historyGroup);
    history->setObjectName(QStringLiteral("keyboardCommandPaletteHistory"));
    history->setAccessibleName(i18nc("@label accessible name", "Actions recently run from the command palette, most recent first"));
    history->setMinimumHeight(history->fontMetrics().height() * (CommandPalette::HistorySize + 1));
    historyLayout->addWidget(history);
    QPushButton *clear =
        iconButton(QStringLiteral("keyboardCommandPaletteClearHistory"), i18nc("@action:button", "Clear History"), QStringLiteral("edit-clear-history"));
    clear->setToolTip(i18nc("@info:tooltip", "Forget which actions were recently run from the command palette"));
    auto *historyButtons = new QHBoxLayout;
    historyButtons->addWidget(clear);
    historyButtons->addStretch();
    historyLayout->addLayout(historyButtons);
    layout->addWidget(historyGroup);

    QPushButton *configure = iconButton(QStringLiteral("keyboardCommandPaletteConfigure"),
                                        i18nc("@action:button", "Configure Keyboard Shortcuts…"),
                                        QStringLiteral("configure-shortcuts"));
    const QPointer<QAction> shortcutsAction = window ? window->actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)) : nullptr;
    configure->setEnabled(!shortcutsAction.isNull());
    connect(configure, &QPushButton::clicked, this, [shortcutsAction] {
        if (shortcutsAction) {
            shortcutsAction->trigger();
        }
    });
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(configure);
    buttons->addStretch();
    layout->addLayout(buttons);
    layout->addStretch();

    m_refreshCommandPalette = [window, shortcut, historySummary, history, clear] {
        const QAction *action = window ? window->actionCollection()->action(QString::fromLatin1(CommandPalette::ActionName)) : nullptr;
        if (!action) {
            shortcut->setText(i18nc("@info", "Open Preferences from a Plasma Opus window to see the command palette's shortcut."));
        } else {
            shortcut->setText(i18nc("@info %1 shortcuts", "Shortcut: %1", shortcutsText(action->shortcuts())));
        }
        const QStringList recent =
            CommandPalette::historyText(CommandPalette::history(KSharedConfig::openStateConfig()->group(QString::fromLatin1(CommandPalette::HistoryGroup))));
        history->clear();
        history->addItems(recent);
        history->setVisible(!recent.isEmpty());
        clear->setEnabled(!recent.isEmpty());
        historySummary->setText(recent.isEmpty() ? i18nc("@info", "No actions have been run from the command palette yet.")
                                                 : i18nc("@info", "These come first in the palette until you type:"));
    };
    connect(clear, &QPushButton::clicked, this, [this] {
        KConfigGroup state = KSharedConfig::openStateConfig()->group(QString::fromLatin1(CommandPalette::HistoryGroup));
        CommandPalette::clearHistory(state);
        m_refreshCommandPalette();
    });

    if (window && window->guiFactory()) {
        connect(window->guiFactory(), &KXMLGUIFactory::shortcutsSaved, page, [this] {
            m_refreshCommandPalette();
        });
    }
    // The palette may be used while Preferences is open.
    new ShowWatcher(page, [this] {
        m_refreshCommandPalette();
    });
    m_refreshCommandPalette();
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
    WindowShortcuts::save(window);
    if (window->guiFactory()) {
        // Everything that follows Configure Keyboard Shortcuts, such as the Yazi Mode conflicts, updates too.
        Q_EMIT window->guiFactory()->shortcutsSaved();
    } else {
        m_refreshKeyboardShortcuts();
    }
}

void SettingsDialog::setKeymapImportReview(std::function<bool(KeymapImportDialog &)> review)
{
    m_reviewKeymapImport = std::move(review);
}

void SettingsDialog::exportShortcuts(const QString &path)
{
    auto *window = qobject_cast<KXmlGuiWindow *>(parentWidget());
    if (!window || path.isEmpty()) {
        return;
    }
    KeymapFile::Keymap keymap = KeymapFile::fromBindings(windowBindings(window).bindings);
    keymap.settings = KeymapFile::settingValues(Settings::self());
    const QString error = KeymapFile::exportFile(keymap, Settings::self(), path, QDateTime::currentDateTimeUtc());
    if (!error.isEmpty()) {
        m_keymapStatus->setText(error);
    } else if (keymap.actions.isEmpty()) {
        m_keymapStatus->setText(
            i18nc("@info %1 file", "Exported the keyboard settings to %1. Every action has its default shortcuts, so the file lists none.", path));
    } else {
        m_keymapStatus->setText(i18ncp("@info %2 file",
                                       "Exported the keyboard settings and the shortcuts of 1 customized action to %2.",
                                       "Exported the keyboard settings and the shortcuts of %1 customized actions to %2.",
                                       keymap.actions.size(),
                                       path));
    }
    m_keymapStatus->setVisible(true);
}

void SettingsDialog::importShortcuts(const QString &path)
{
    auto *window = qobject_cast<KXmlGuiWindow *>(parentWidget());
    if (!window || path.isEmpty()) {
        return;
    }
    m_keymapStatus->setVisible(true);
    QString error;
    QStringList warnings;
    const std::optional<KeymapFile::Keymap> keymap = KeymapFile::importFile(path, Settings::self(), &error, &warnings);
    if (!keymap) {
        m_keymapStatus->setText(i18nc("@info %1 file, %2 reason", "Could not import %1: %2 Nothing was changed.", path, error));
        return;
    }

    // The settings go into Preferences' own widgets, compared with what they show.
    QStringList settingChanges;
    QVariantMap editorValues;
    const auto onOff = [](bool on) {
        return on ? i18nc("@item setting value", "On") : i18nc("@item setting value", "Off");
    };
    for (auto it = keymap->settings.constBegin(); it != keymap->settings.constEnd(); ++it) {
        auto *editor = findChild<QWidget *>(QStringLiteral("kcfg_") + it.key());
        QString from;
        QString to;
        if (auto *check = qobject_cast<QCheckBox *>(editor)) {
            if (check->isChecked() == it.value().toBool()) {
                continue;
            }
            from = onOff(check->isChecked());
            to = onOff(it.value().toBool());
        } else if (auto *combo = qobject_cast<QComboBox *>(editor)) {
            const int index = it.value().toInt();
            if (index < 0 || index >= combo->count() || index == combo->currentIndex()) {
                continue;
            }
            from = combo->currentText();
            to = combo->itemText(index);
        } else {
            continue;
        }
        settingChanges.append(i18nc("@item %1 setting, %2 current value, %3 imported value", "%1: %2 → %3", KeymapFile::settingText(it.key()), from, to));
        editorValues.insert(it.key(), it.value());
    }

    const WindowBindings found = windowBindings(window);
    KeymapImportDialog dialog(QFileInfo(path).fileName(), found.bindings, *keymap, warnings, settingChanges, this);
    if (!m_reviewKeymapImport(dialog)) {
        m_keymapStatus->setText(i18nc("@info %1 file", "Didn't import %1; nothing was changed.", path));
        return;
    }
    const KeymapFile::Preview preview = dialog.preview();
    int changed = 0;
    for (const KeymapFile::Change &change : preview.changes) {
        // Looked up again, as the window's actions may have changed while the preview was shown.
        if (QAction *action = window->actionCollection()->action(found.bindings.at(change.index).name)) {
            action->setShortcuts(change.shortcuts);
            ++changed;
        }
    }
    if (changed > 0) {
        WindowShortcuts::save(window);
        if (window->guiFactory()) {
            // As after Configure Keyboard Shortcuts, every window and everything that lists shortcuts updates.
            Q_EMIT window->guiFactory()->shortcutsSaved();
        } else {
            m_refreshKeyboardShortcuts();
        }
    }
    if (!editorValues.isEmpty()) {
        setEditorValues(editorValues);
    }
    QString text = i18ncp("@info %2 file", "Imported the shortcuts of 1 action from %2.", "Imported the shortcuts of %1 actions from %2.", changed, path);
    if (!editorValues.isEmpty()) {
        text += QLatin1Char(' ')
            + i18ncp("@info",
                     "Apply or OK saves the keyboard setting it changed.",
                     "Apply or OK saves the %1 keyboard settings it changed.",
                     editorValues.size());
    }
    m_keymapStatus->setText(text);
}
