// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardmapdialog.h"

#include "doubleshift.h"
#include "settings.h"
#include "windowshortcuts.h"
#include "yazimode.h"

#include <KActionCollection>
#include <KGlobalAccel>
#include <KKeySequenceWidget>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardAction>
#include <KXMLGUIFactory>
#include <KXmlGuiWindow>

#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

KeyboardMapDialog::KeyboardMapDialog(KXmlGuiWindow *window, QWidget *parent)
    : QDialog(parent)
    , m_window(window)
{
    setObjectName(QStringLiteral("keyboardMapDialog"));
    setWindowTitle(i18nc("@title:window", "Keyboard Map"));
    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("keyboardMapSearch"));
    m_search->setPlaceholderText(i18nc("@info:placeholder", "Action name, description or shortcut"));
    m_search->setClearButtonEnabled(true);
    form->addRow(i18nc("@label:textbox", "Search:"), m_search);
    m_keys = new KKeySequenceWidget(this);
    m_keys->setObjectName(QStringLiteral("keyboardMapKeys"));
    m_keys->setCheckForConflictsAgainst(KKeySequenceWidget::None);
    m_keys->setModifierlessAllowed(true);
    m_keys->setMultiKeyShortcutsAllowed(true);
    m_keys->setClearButtonShown(true);
    m_keys->setToolTip(i18nc("@info:tooltip", "Click, then press a key combination to list the actions it runs and the key sequences it begins"));
    form->addRow(i18nc("@label", "Find by key:"), m_keys);
    m_withShortcuts = new QCheckBox(i18nc("@option:check", "Only actions with shortcuts"), this);
    m_withShortcuts->setObjectName(QStringLiteral("keyboardMapWithShortcuts"));
    m_withShortcuts->setChecked(true);
    form->addRow(QString(), m_withShortcuts);
    layout->addLayout(form);

    m_list = new QTreeWidget(this);
    m_list->setObjectName(QStringLiteral("keyboardMapList"));
    m_list->setAccessibleName(i18nc("@label accessible name", "Actions and their keyboard shortcuts"));
    m_list->setHeaderLabels(
        {i18nc("@title:column", "Action"), i18nc("@title:column", "Shortcuts"), i18nc("@title:column", "Applies in"), i18nc("@title:column", "Description")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAlternatingRowColors(true);
    m_list->setMinimumHeight(m_list->fontMetrics().height() * 20);
    layout->addWidget(m_list, 1);
    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("keyboardMapCount"));
    m_count->setTextFormat(Qt::PlainText);
    layout->addWidget(m_count);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *exportButton = buttons->addButton(i18nc("@action:button", "Export…"), QDialogButtonBox::ActionRole);
    exportButton->setObjectName(QStringLiteral("keyboardMapExport"));
    exportButton->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    exportButton->setToolTip(i18nc("@info:tooltip", "Save the listed actions as an HTML or PDF reference to print"));
    m_configure = buttons->addButton(i18nc("@action:button", "Configure Keyboard Shortcuts…"), QDialogButtonBox::ActionRole);
    m_configure->setObjectName(QStringLiteral("keyboardMapConfigure"));
    m_configure->setIcon(QIcon::fromTheme(QStringLiteral("configure-shortcuts")));
    const QPointer<QAction> shortcutsAction = window ? window->actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)) : nullptr;
    m_configure->setEnabled(!shortcutsAction.isNull());
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(exportButton, &QPushButton::clicked, this, &KeyboardMapDialog::exportReference);
    connect(m_configure, &QPushButton::clicked, this, [shortcutsAction] {
        if (shortcutsAction) {
            shortcutsAction->trigger();
        }
    });
    connect(m_search, &QLineEdit::textChanged, this, &KeyboardMapDialog::updateList);
    connect(m_keys, &KKeySequenceWidget::keySequenceChanged, this, &KeyboardMapDialog::updateList);
    connect(m_withShortcuts, &QCheckBox::toggled, this, &KeyboardMapDialog::updateList);
    if (window && window->guiFactory()) {
        connect(window->guiFactory(), &KXMLGUIFactory::shortcutsSaved, this, &KeyboardMapDialog::refresh);
    }
    // A changed Yazi mode preset or Tab switching panes reaches the actions once the window applies the settings.
    connect(Settings::self(), &Settings::configChanged, this, [this] {
        QTimer::singleShot(0, this, &KeyboardMapDialog::refresh);
    });
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &KeyboardMapDialog::refresh);

    refresh();
    m_search->setFocus();
    resize(fontMetrics().averageCharWidth() * 140, sizeHint().height());
}

QList<KeyboardMap::Entry> KeyboardMapDialog::entries(KXmlGuiWindow *window)
{
    QList<KeyboardMap::Entry> windowEntries;
    QList<KeyboardMap::Entry> galleryEntries;
    QList<KeyboardMap::Entry> globalEntries;
    const WindowShortcuts::Bindings found = WindowShortcuts::bindings(window);
    for (int i = 0; i < found.bindings.size(); ++i) {
        const KeyBindings::Binding &binding = found.bindings.at(i);
        const QAction *action = found.actions.at(i);
        KeyboardMap::Entry entry{binding.name, binding.text, {}, binding.shortcuts, {}, KeyBindings::contextText(binding)};
        if (action) {
            entry.description = KeyboardMap::description(action->text(), action->toolTip(), action->statusTip(), action->whatsThis());
        }
        (binding.global ? globalEntries : action ? windowEntries : galleryEntries).append(entry);
    }
    std::stable_sort(windowEntries.begin(), windowEntries.end(), [](const KeyboardMap::Entry &a, const KeyboardMap::Entry &b) {
        return a.text.localeAwareCompare(b.text) < 0;
    });

    // Pressing Shift twice is a gesture rather than a shortcut, so no action lists it.
    if (window && Settings::doubleShiftSearch() && DoubleShiftDetector::unavailableReason().isEmpty()) {
        const QAction *search = window->actionCollection()->action(QStringLiteral("search_folder"));
        windowEntries.append({QStringLiteral("double_shift_search"),
                              search ? KLocalizedString::removeAcceleratorMarker(search->text()) : i18nc("@action", "Search Current Folder…"),
                              i18nc("@info", "Press Shift twice quickly to open search; Preferences › Tools › Search turns it off"),
                              {QKeySequence(Qt::Key_Shift, Qt::Key_Shift)},
                              i18nc("@item keyboard gesture", "Shift twice"),
                              KeyBindings::contextText(KeyBindings::Context::Application)});
    }

    // Yazi mode binds some commands to several keys, such as Up and K; each command is one entry.
    QList<KeyboardMap::Entry> yaziEntries;
    const YaziMode::Keymap keymap = YaziMode::presetKeymap(static_cast<YaziMode::Preset>(Settings::yaziKeyPreset()));
    for (const YaziMode::Binding &binding : keymap.bindings) {
        const QString id = YaziMode::commandId(binding.command);
        auto entry = std::find_if(yaziEntries.begin(), yaziEntries.end(), [&id](const KeyboardMap::Entry &e) {
            return e.name == id;
        });
        if (entry == yaziEntries.end()) {
            yaziEntries.append({id, YaziMode::commandText(binding.command), {}, {}, {}, i18nc("@item where a keyboard shortcut applies", "Yazi mode columns")});
            entry = std::prev(yaziEntries.end());
        }
        const YaziMode::KeySequence &keys = binding.keys;
        const QKeyCombination none = QKeyCombination::fromCombined(0);
        entry->shortcuts.append(QKeySequence(keys.value(0, none), keys.value(1, none), keys.value(2, none), keys.value(3, none)));
        if (!entry->keysText.isEmpty()) {
            entry->keysText += i18nc("@item separator of keyboard shortcuts", "; ");
        }
        entry->keysText += YaziMode::sequenceText(keys);
    }
    return windowEntries + galleryEntries + yaziEntries + globalEntries;
}

void KeyboardMapDialog::setSearchKeys(const QKeySequence &keys)
{
    m_keys->setKeySequence(keys);
    updateList();
}

QList<KeyboardMap::Entry> KeyboardMapDialog::shownEntries() const
{
    return m_shown;
}

QString KeyboardMapDialog::exportTo(const QString &path) const
{
    return KeyboardMap::exportReference(m_shown, i18nc("@title", "Plasma Opus Keyboard Shortcuts"), path);
}

void KeyboardMapDialog::refresh()
{
    m_entries = entries(m_window);
    m_configure->setEnabled(m_window && m_window->actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)));
    updateList();
}

void KeyboardMapDialog::updateList()
{
    m_shown = KeyboardMap::filter(m_entries, m_search->text(), m_keys->keySequence(), m_withShortcuts->isChecked());
    m_list->clear();
    const QString none = i18nc("@item no keyboard shortcut", "None");
    for (const KeyboardMap::Entry &entry : std::as_const(m_shown)) {
        const QString keys = KeyboardMap::shortcutsText(entry);
        auto *item = new QTreeWidgetItem(m_list, {entry.text, keys.isEmpty() ? none : keys, entry.context, entry.description});
        item->setData(0, Qt::UserRole, entry.name);
        item->setToolTip(3, entry.description);
        if (m_window && !entry.name.isEmpty()) {
            if (const QAction *action = m_window->actionCollection()->action(entry.name)) {
                item->setIcon(0, action->icon());
            }
        }
    }
    m_list->resizeColumnToContents(0);
    m_list->resizeColumnToContents(1);
    m_list->resizeColumnToContents(2);
    if (!m_window) {
        m_count->setText(i18nc("@info", "Open the keyboard map from a Plasma Opus window to see its shortcuts."));
    } else {
        QString count = i18ncp("@info", "%1 action listed.", "%1 actions listed.", m_shown.size());
        // Say when a search finds actions that are hidden only because they have no shortcut.
        const qsizetype hidden =
            m_withShortcuts->isChecked() ? KeyboardMap::filter(m_entries, m_search->text(), m_keys->keySequence(), false).size() - m_shown.size() : 0;
        if (hidden > 0) {
            count += QLatin1Char(' ')
                + i18ncp("@info",
                         "1 more without a shortcut is hidden; clear Only actions with shortcuts to show it.",
                         "%1 more without shortcuts are hidden; clear Only actions with shortcuts to show them.",
                         hidden);
        }
        m_count->setText(count);
    }
}

void KeyboardMapDialog::exportReference()
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString htmlFilter = i18nc("@item file type", "HTML page (*.html)");
    const QString pdfFilter = i18nc("@item file type", "PDF document (*.pdf)");
    QString filter = htmlFilter;
    QString path = QFileDialog::getSaveFileName(this,
                                                i18nc("@title:window", "Export Keyboard Map"),
                                                QDir(folder.isEmpty() ? QDir::homePath() : folder).filePath(QStringLiteral("plasma-opus-shortcuts.html")),
                                                htmlFilter + QStringLiteral(";;") + pdfFilter,
                                                &filter);
    if (path.isEmpty()) {
        return;
    }
    const QString suffix = filter == pdfFilter ? QStringLiteral(".pdf") : QStringLiteral(".html");
    if (!path.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive) && !path.endsWith(QLatin1String(".html"), Qt::CaseInsensitive)
        && !path.endsWith(QLatin1String(".htm"), Qt::CaseInsensitive)) {
        path += suffix;
    }
    const QString error = exportTo(path);
    if (!error.isEmpty()) {
        KMessageBox::error(this, i18nc("@info %1 file, %2 error", "Could not export the keyboard map to %1: %2", path, error));
    }
}
