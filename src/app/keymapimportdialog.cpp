// SPDX-License-Identifier: GPL-3.0-or-later

#include "keymapimportdialog.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{
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

QListWidget *list(const QString &name, const QString &accessibleName)
{
    auto *widget = new QListWidget;
    widget->setObjectName(name);
    widget->setAccessibleName(accessibleName);
    widget->setWordWrap(true);
    return widget;
}
}

KeymapImportDialog::KeymapImportDialog(const QString &fileName,
                                       const QList<KeyBindings::Binding> &bindings,
                                       const KeymapFile::Keymap &keymap,
                                       const QStringList &warnings,
                                       const QStringList &settingChanges,
                                       QWidget *parent)
    : QDialog(parent)
    , m_bindings(bindings)
    , m_keymap(keymap)
    , m_warnings(warnings)
    , m_settingChanges(settingChanges)
{
    setObjectName(QStringLiteral("keymapImportDialog"));
    setWindowTitle(i18nc("@title:window %1 file name", "Import Keyboard Shortcuts from %1", fileName));
    auto *layout = new QVBoxLayout(this);

    m_summary = notice(QStringLiteral("keymapImportSummary"));
    layout->addWidget(m_summary);

    auto *changesGroup = new QGroupBox(i18nc("@title:group", "Shortcuts That Change"), this);
    auto *changesLayout = new QVBoxLayout(changesGroup);
    m_changes = new QTreeWidget(changesGroup);
    m_changes->setObjectName(QStringLiteral("keymapImportChanges"));
    m_changes->setAccessibleName(i18nc("@label accessible name", "Shortcuts the import changes"));
    m_changes->setHeaderLabels(
        {i18nc("@title:column", "Action"), i18nc("@title:column", "Now"), i18nc("@title:column", "Imported"), i18nc("@title:column", "Applies in")});
    m_changes->setRootIsDecorated(false);
    m_changes->setUniformRowHeights(true);
    m_changes->setMinimumHeight(m_changes->fontMetrics().height() * 8);
    changesLayout->addWidget(m_changes);
    m_resetUnlisted = new QCheckBox(i18nc("@option:check", "Give actions that the file doesn't list their default shortcuts"), changesGroup);
    m_resetUnlisted->setObjectName(QStringLiteral("keymapImportResetUnlisted"));
    m_resetUnlisted->setToolTip(
        i18nc("@info:tooltip", "Makes the shortcuts the same as where the file was exported; otherwise only the listed actions change"));
    changesLayout->addWidget(m_resetUnlisted);
    layout->addWidget(changesGroup);

    auto *conflictsGroup = new QGroupBox(i18nc("@title:group", "Conflicts After Importing"), this);
    auto *conflictsLayout = new QVBoxLayout(conflictsGroup);
    m_conflictSummary = notice(QStringLiteral("keymapImportConflictSummary"));
    conflictsLayout->addWidget(m_conflictSummary);
    m_conflicts = list(QStringLiteral("keymapImportConflicts"), i18nc("@label accessible name", "Conflicts the import adds"));
    conflictsLayout->addWidget(m_conflicts);
    layout->addWidget(conflictsGroup);

    auto *settingsGroup = new QGroupBox(i18nc("@title:group", "Keyboard Settings"), this);
    settingsGroup->setObjectName(QStringLiteral("keymapImportSettingsGroup"));
    auto *settingsLayout = new QVBoxLayout(settingsGroup);
    QListWidget *settingsList = list(QStringLiteral("keymapImportSettings"), i18nc("@label accessible name", "Keyboard settings the import changes"));
    settingsList->addItems(m_settingChanges);
    settingsLayout->addWidget(settingsList);
    settingsLayout->addWidget(notice(QStringLiteral("keymapImportSettingsNotice"),
                                     i18nc("@info", "Preferences shows these settings; Apply or OK there saves them, as when you change them yourself.")));
    settingsGroup->setVisible(!m_settingChanges.isEmpty());
    layout->addWidget(settingsGroup);

    auto *skippedGroup = new QGroupBox(i18nc("@title:group", "Not Imported"), this);
    skippedGroup->setObjectName(QStringLiteral("keymapImportSkippedGroup"));
    auto *skippedLayout = new QVBoxLayout(skippedGroup);
    m_skipped = list(QStringLiteral("keymapImportSkipped"), i18nc("@label accessible name", "What the import leaves out"));
    skippedLayout->addWidget(m_skipped);
    layout->addWidget(skippedGroup);

    layout->addWidget(notice(QStringLiteral("keymapImportSafetyNotice"),
                             i18nc("@info",
                                   "Importing only gives actions shortcuts and changes the settings listed; nothing in the file "
                                   "runs. Shortcuts take effect right away in every Plasma Opus window, as when they are saved "
                                   "in Configure Keyboard Shortcuts.")));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    m_apply = buttons->button(QDialogButtonBox::Apply);
    m_apply->setObjectName(QStringLiteral("keymapImportApply"));
    connect(m_apply, &QPushButton::clicked, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_resetUnlisted, &QCheckBox::toggled, this, &KeymapImportDialog::refresh);
    refresh();
    m_changes->setFocus();
    resize(sizeHint().expandedTo(QSize(fontMetrics().averageCharWidth() * 100, 0)));
}

KeymapFile::Preview KeymapImportDialog::preview() const
{
    return m_preview;
}

bool KeymapImportDialog::resetsUnlisted() const
{
    return m_resetUnlisted->isChecked();
}

void KeymapImportDialog::refresh()
{
    m_preview = KeymapFile::preview(m_bindings, m_keymap, m_resetUnlisted->isChecked());

    m_changes->clear();
    for (const KeymapFile::Change &change : std::as_const(m_preview.changes)) {
        const KeyBindings::Binding &binding = m_bindings.at(change.index);
        auto *item = new QTreeWidgetItem(m_changes,
                                         {binding.text, shortcutsText(binding.shortcuts), shortcutsText(change.shortcuts), KeyBindings::contextText(binding)});
        item->setData(0, Qt::UserRole, binding.name);
    }
    m_changes->resizeColumnToContents(0);

    QStringList parts;
    if (m_preview.changes.isEmpty()) {
        parts.append(i18nc("@info", "The file changes no shortcuts."));
    } else {
        parts.append(
            i18ncp("@info", "Importing changes the shortcuts of 1 action:", "Importing changes the shortcuts of %1 actions:", m_preview.changes.size()));
    }
    if (m_preview.unchanged > 0) {
        parts.append(
            i18ncp("@info", "1 more action already has the file's shortcuts.", "%1 more actions already have the file's shortcuts.", m_preview.unchanged));
    }
    m_summary->setText(parts.join(QLatin1Char(' ')));

    m_conflicts->clear();
    for (const KeyBindings::Conflict &conflict : std::as_const(m_preview.added)) {
        m_conflicts->addItem(KeyBindings::conflictText(conflict, m_preview.bindings));
    }
    m_conflicts->setVisible(m_conflicts->count() > 0);
    QStringList conflictParts;
    conflictParts.append(m_preview.added.isEmpty() ? i18nc("@info", "The import adds no conflicts.")
                                                   : i18ncp("@info",
                                                            "The import adds 1 conflict; the shortcuts can still be changed afterwards:",
                                                            "The import adds %1 conflicts; the shortcuts can still be changed afterwards:",
                                                            m_preview.added.size()));
    if (m_preview.resolved > 0) {
        conflictParts.append(i18ncp("@info", "It resolves 1 conflict.", "It resolves %1 conflicts.", m_preview.resolved));
    }
    if (!m_preview.kept.isEmpty()) {
        conflictParts.append(i18ncp("@info",
                                    "1 conflict stays as it is; Preferences › Keyboard › Shortcuts lists it.",
                                    "%1 conflicts stay as they are; Preferences › Keyboard › Shortcuts lists them.",
                                    m_preview.kept.size()));
    }
    m_conflictSummary->setText(conflictParts.join(QLatin1Char(' ')));

    m_skipped->clear();
    for (const QString &name : std::as_const(m_preview.unknown)) {
        m_skipped->addItem(KeymapFile::unknownText(name));
    }
    m_skipped->addItems(m_warnings);
    m_skipped->parentWidget()->setVisible(m_skipped->count() > 0);

    m_apply->setEnabled(!m_preview.changes.isEmpty() || !m_settingChanges.isEmpty());
}
