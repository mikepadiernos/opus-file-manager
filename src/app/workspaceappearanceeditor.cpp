// SPDX-License-Identifier: GPL-3.0-or-later

#include "workspaceappearanceeditor.h"

#include "appearancedefaults.h"
#include "settings.h"
#include "themeprofiles.h"
#include "windoweffects.h"
#include "workspaceappearance.h"

#include <KLocalizedString>
#include <KSharedConfig>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
const QString ViewModeEntry = QStringLiteral("DefaultViewMode");
const QString FolderRulesEntry = QStringLiteral("FolderColorRules");
const QString BlurEntry = QStringLiteral("BackgroundBlur");
const QString CornersEntry = QStringLiteral("CornerRadius");
const QString CustomCornersEntry = QStringLiteral("CustomCornerRadius");
const QString DividersEntry = QStringLiteral("Dividers");
const QString RevealEntry = QStringLiteral("RevealDividersOnHover");
}

WorkspaceAppearanceEditor::WorkspaceAppearanceEditor(QWidget *parent)
    : QGroupBox(i18nc("@title:group", "Workspace Appearance"), parent)
{
    setObjectName(QStringLiteral("workspaceAppearance"));
    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(i18nc("@info",
                                         "Applies while this workspace is active, without changing the global settings. Plasma's appearance, the global "
                                         "settings, the theme profile and these options apply in that order, then the custom stylesheet."),
                                   this);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *grid = new QGridLayout;
    m_profile = new QComboBox(this);
    m_profile->setObjectName(QStringLiteral("workspaceThemeProfile"));
    auto *profileLabel = new QLabel(i18nc("@label:listbox", "Theme profile:"), this);
    profileLabel->setBuddy(m_profile);
    grid->addWidget(profileLabel, 0, 0);
    grid->addWidget(m_profile, 0, 1);

    const auto addRow = [this, grid](const QString &name, const QString &text, const QStringList &entries, const QList<QWidget *> &editors) {
        auto *check = new QCheckBox(text, this);
        check->setObjectName(name);
        auto *row = new QHBoxLayout;
        for (QWidget *editor : editors) {
            row->addWidget(editor);
        }
        row->addStretch();
        const int index = grid->rowCount();
        grid->addWidget(check, index, 0);
        grid->addLayout(row, index, 1);
        connect(check, &QCheckBox::toggled, this, &WorkspaceAppearanceEditor::updateEnabled);
        m_rows.append({check, entries, editors});
    };

    m_viewMode = new QComboBox(this);
    m_viewMode->setObjectName(QStringLiteral("workspaceViewMode"));
    m_viewMode->addItems({i18nc("@item:inlistbox view mode", "Details"),
                          i18nc("@item:inlistbox view mode", "Icons"),
                          i18nc("@item:inlistbox view mode", "Compact"),
                          i18nc("@item:inlistbox view mode", "Cards")});
    addRow(QStringLiteral("workspaceOverrideViewMode"), i18nc("@option:check", "View mode:"), {ViewModeEntry}, {m_viewMode});

    m_folderRules = new QComboBox(this);
    m_folderRules->setObjectName(QStringLiteral("workspaceFolderColorRules"));
    addRow(QStringLiteral("workspaceOverrideFolderColorRules"), i18nc("@option:check", "Folder colour rules:"), {FolderRulesEntry}, {m_folderRules});

    m_blur = new QCheckBox(i18nc("@option:check", "Blur behind translucent backgrounds"), this);
    m_blur->setObjectName(QStringLiteral("workspaceBlur"));
    connect(m_blur, &QCheckBox::toggled, this, &WorkspaceAppearanceEditor::updateProblems);
    addRow(QStringLiteral("workspaceOverrideBlur"), i18nc("@option:check", "Blur:"), {BlurEntry}, {m_blur});

    m_corners = new QComboBox(this);
    m_corners->setObjectName(QStringLiteral("workspaceCornerRadius"));
    m_corners->addItems({i18nc("@item:inlistbox corner radius", "Follow widget style"),
                         i18nc("@item:inlistbox corner radius", "Square"),
                         i18nc("@item:inlistbox corner radius", "Small"),
                         i18nc("@item:inlistbox corner radius", "Medium"),
                         i18nc("@item:inlistbox corner radius", "Large"),
                         i18nc("@item:inlistbox corner radius", "Custom")});
    m_customCorners = new QSpinBox(this);
    m_customCorners->setObjectName(QStringLiteral("workspaceCustomCornerRadius"));
    m_customCorners->setRange(0, 24);
    m_customCorners->setSuffix(i18nc("@item:valuesuffix logical pixels", " px"));
    m_customCorners->setAccessibleName(i18nc("@label:spinbox", "Custom corner radius"));
    connect(m_corners, &QComboBox::currentIndexChanged, this, &WorkspaceAppearanceEditor::updateEnabled);
    addRow(QStringLiteral("workspaceOverrideCornerRadius"),
           i18nc("@option:check", "Corner radius:"),
           {CornersEntry, CustomCornersEntry},
           {m_corners, m_customCorners});

    m_dividers = new QComboBox(this);
    m_dividers->setObjectName(QStringLiteral("workspaceDividers"));
    m_dividers->addItems(
        {i18nc("@item:inlistbox dividers", "Follow widget style"), i18nc("@item:inlistbox dividers", "Thin"), i18nc("@item:inlistbox dividers", "Hidden")});
    m_revealDividers = new QCheckBox(i18nc("@option:check", "Show hidden dividers under the pointer"), this);
    m_revealDividers->setObjectName(QStringLiteral("workspaceRevealDividers"));
    connect(m_dividers, &QComboBox::currentIndexChanged, this, &WorkspaceAppearanceEditor::updateEnabled);
    addRow(QStringLiteral("workspaceOverrideDividers"), i18nc("@option:check", "Dividers:"), {DividersEntry, RevealEntry}, {m_dividers, m_revealDividers});
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);

    m_problems = new QLabel(this);
    m_problems->setObjectName(QStringLiteral("workspaceAppearanceProblems"));
    m_problems->setWordWrap(true);
    m_problems->hide();
    layout->addWidget(m_problems);
    auto *save = new QPushButton(i18nc("@action:button", "Save Workspace Appearance"), this);
    save->setObjectName(QStringLiteral("workspaceSaveAppearance"));
    connect(save, &QPushButton::clicked, this, &WorkspaceAppearanceEditor::saveRequested);
    layout->addWidget(save);
    connect(m_profile, &QComboBox::currentIndexChanged, this, &WorkspaceAppearanceEditor::updateProblems);
    setAppearance({});
}

void WorkspaceAppearanceEditor::reloadProfiles(const QString &selectedId)
{
    const QSignalBlocker blocker(m_profile);
    m_profile->clear();
    m_profile->addItem(i18nc("@item:inlistbox no theme profile", "None, use the global appearance"), QString());
    const QList<ThemeProfiles::Profile> profiles =
        ThemeProfiles::load(Settings::self(), KSharedConfig::openConfig()->group(QLatin1String(ThemeProfiles::Group)));
    for (const ThemeProfiles::Profile &profile : profiles) {
        m_profile->addItem(profile.name, profile.id);
    }
    if (!selectedId.isEmpty() && m_profile->findData(selectedId) < 0) {
        m_profile->addItem(i18nc("@item:inlistbox", "Unavailable theme profile, kept for retry"), selectedId);
    }
    m_profile->setCurrentIndex(qMax(0, m_profile->findData(selectedId)));
}

void WorkspaceAppearanceEditor::showGlobalValues()
{
    m_viewMode->setCurrentIndex(qBound(0, int(Settings::defaultViewMode()), m_viewMode->count() - 1));
    m_blur->setChecked(Settings::backgroundBlur());
    m_corners->setCurrentIndex(int(Settings::cornerRadius()));
    m_customCorners->setValue(Settings::customCornerRadius());
    m_dividers->setCurrentIndex(int(Settings::dividers()));
    m_revealDividers->setChecked(Settings::revealDividersOnHover());
}

void WorkspaceAppearanceEditor::setAppearance(const WorkspaceCatalog::Appearance &appearance)
{
    m_stored = appearance;
    reloadProfiles(appearance.themeProfileId);
    showGlobalValues();
    const QVariantMap values = WorkspaceAppearance::decodeOverrides(Settings::self(), appearance.overrides);
    m_folderRules->clear();
    if (values.contains(FolderRulesEntry)) {
        const QStringList saved = values.value(FolderRulesEntry).toStringList();
        m_folderRules->addItem(i18ncp("@item:inlistbox", "Saved with the workspace, %1 rule", "Saved with the workspace, %1 rules", saved.size()), saved);
    }
    const QStringList global = Settings::folderColorRules();
    m_folderRules->addItem(i18ncp("@item:inlistbox", "Copy of the global rules, %1 rule", "Copy of the global rules, %1 rules", global.size()), global);
    m_folderRules->addItem(i18nc("@item:inlistbox", "No folder colour rules"), QStringList());
    if (values.contains(ViewModeEntry)) {
        m_viewMode->setCurrentIndex(values.value(ViewModeEntry).toInt());
    }
    if (values.contains(BlurEntry)) {
        m_blur->setChecked(values.value(BlurEntry).toBool());
    }
    if (values.contains(CornersEntry)) {
        m_corners->setCurrentIndex(values.value(CornersEntry).toInt());
    }
    if (values.contains(CustomCornersEntry)) {
        m_customCorners->setValue(values.value(CustomCornersEntry).toInt());
    }
    if (values.contains(DividersEntry)) {
        m_dividers->setCurrentIndex(values.value(DividersEntry).toInt());
    }
    if (values.contains(RevealEntry)) {
        m_revealDividers->setChecked(values.value(RevealEntry).toBool());
    }
    for (const Row &row : std::as_const(m_rows)) {
        const QSignalBlocker blocker(row.override);
        row.override->setChecked(std::any_of(row.entries.cbegin(), row.entries.cend(), [&values](const QString &entry) {
            return values.contains(entry);
        }));
    }
    updateEnabled();
}

WorkspaceCatalog::Appearance WorkspaceAppearanceEditor::appearance() const
{
    QVariantMap values;
    const auto checked = [this](const QString &entry) {
        return std::any_of(m_rows.cbegin(), m_rows.cend(), [&entry](const Row &row) {
            return row.entries.contains(entry) && row.override->isChecked();
        });
    };
    if (checked(ViewModeEntry)) {
        values.insert(ViewModeEntry, m_viewMode->currentIndex());
    }
    if (checked(FolderRulesEntry)) {
        values.insert(FolderRulesEntry, m_folderRules->currentData().toStringList());
    }
    if (checked(BlurEntry)) {
        values.insert(BlurEntry, m_blur->isChecked());
    }
    if (checked(CornersEntry)) {
        values.insert(CornersEntry, m_corners->currentIndex());
        values.insert(CustomCornersEntry, m_customCorners->value());
    }
    if (checked(DividersEntry)) {
        values.insert(DividersEntry, m_dividers->currentIndex());
        values.insert(RevealEntry, m_revealDividers->isChecked());
    }
    WorkspaceCatalog::Appearance result;
    result.themeProfileId = m_profile->currentData().toString();
    result.overrides = WorkspaceAppearance::encodeOverrides(Settings::self(), values);
    // Keep saved values this editor cannot show, such as values that are invalid now, unless they were replaced.
    const QVariantMap valid = WorkspaceAppearance::decodeOverrides(Settings::self(), m_stored.overrides);
    for (auto it = m_stored.overrides.constBegin(); it != m_stored.overrides.constEnd(); ++it) {
        if (!valid.contains(it.key()) && !checked(it.key())) {
            result.overrides.insert(it.key(), it.value());
        }
    }
    return result;
}

void WorkspaceAppearanceEditor::updateEnabled()
{
    for (const Row &row : std::as_const(m_rows)) {
        for (QWidget *editor : row.editors) {
            editor->setEnabled(row.override->isChecked());
        }
    }
    m_customCorners->setEnabled(m_customCorners->isEnabled() && m_corners->currentIndex() == Settings::EnumCornerRadius::Custom);
    m_revealDividers->setEnabled(m_revealDividers->isEnabled() && m_dividers->currentIndex() == Settings::EnumDividers::Hidden);
    updateProblems();
}

void WorkspaceAppearanceEditor::updateProblems()
{
    QStringList messages;
    if (AppearanceDefaults::workspaceOverridesDisabled()) {
        messages.append(i18nc("@info", "Workspace appearance is turned off for this session, so the global appearance applies."));
    }
    const QList<ThemeProfiles::Profile> profiles =
        ThemeProfiles::load(Settings::self(), KSharedConfig::openConfig()->group(QLatin1String(ThemeProfiles::Group)));
    const WorkspaceAppearance::Resolved resolved = WorkspaceAppearance::resolve(Settings::self(), appearance(), profiles);
    if (!resolved.problems.isEmpty()) {
        messages.append(i18nc("@info %1 is a list of themes, fonts or options",
                              "Unavailable now and kept for retry: %1.",
                              resolved.problems.join(i18nc("@item list separator", ", "))));
    }
    if (resolved.values.value(BlurEntry).toBool()) {
        if (const QString reason = WindowEffects::unavailableReason(window(), true, false); !reason.isEmpty()) {
            messages.append(reason);
        }
    }
    m_problems->setText(messages.join(QLatin1Char('\n')));
    m_problems->setVisible(!messages.isEmpty());
}
