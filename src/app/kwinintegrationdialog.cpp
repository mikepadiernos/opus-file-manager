// SPDX-License-Identifier: GPL-3.0-or-later

#include "kwinintegrationdialog.h"

#include "kwinintegration.h"

#include <KGlobalAccel>
#include <KKeySequenceWidget>
#include <KLocalizedString>
#include <KMessageBox>
#include <KWindowStateSaver>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

KWinIntegrationDialog::KWinIntegrationDialog(KWinIntegration *kwin, const QList<QPair<QString, QString>> &workspaces, QAction *dropDownAction, QWidget *parent)
    : QDialog(parent)
    , m_kwin(kwin)
    , m_workspaces(workspaces)
    , m_dropDownAction(dropDownAction)
{
    setObjectName(QStringLiteral("kwinIntegrationDialog"));
    setWindowTitle(i18nc("@title:window", "Configure KWin Integration"));
    auto *layout = new QVBoxLayout(this);

    auto *scriptBox = new QGroupBox(i18nc("@title:group", "KWin Script"), this);
    auto *scriptLayout = new QVBoxLayout(scriptBox);
    auto *intro = new QLabel(i18nc("@info",
                                   "An optional KWin script lets Plasma Opus restore, tile and place its windows, which Wayland applications cannot do "
                                   "themselves. Nothing is installed until you choose to, and the script acts only on Plasma Opus windows."),
                             scriptBox);
    intro->setWordWrap(true);
    scriptLayout->addWidget(intro);
    m_status = new QLabel(scriptBox);
    m_status->setObjectName(QStringLiteral("kwinStatus"));
    m_status->setWordWrap(true);
    scriptLayout->addWidget(m_status);
    auto *actions = new QHBoxLayout;
    m_install = new QPushButton(scriptBox);
    m_install->setObjectName(QStringLiteral("kwinInstall"));
    m_enabled = new QCheckBox(i18nc("@option:check", "Enabled"), scriptBox);
    m_enabled->setObjectName(QStringLiteral("kwinEnabled"));
    m_remove = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:button", "Remove"), scriptBox);
    m_remove->setObjectName(QStringLiteral("kwinRemove"));
    auto *refresh = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action:button", "Refresh"), scriptBox);
    actions->addWidget(m_install);
    actions->addWidget(m_enabled);
    actions->addWidget(m_remove);
    actions->addStretch();
    actions->addWidget(refresh);
    scriptLayout->addLayout(actions);
    auto *links = new QHBoxLayout;
    auto *scripts = new QPushButton(i18nc("@action:button", "KWin Scripts Settings…"), scriptBox);
    auto *rules = new QPushButton(i18nc("@action:button", "Window Rules…"), scriptBox);
    links->addWidget(scripts);
    links->addWidget(rules);
    links->addStretch();
    scriptLayout->addLayout(links);
    layout->addWidget(scriptBox);

    const KWinScript::Options options = kwin->options();
    auto *behaviourBox = new QGroupBox(i18nc("@title:group", "Window Management"), this);
    auto *behaviourLayout = new QVBoxLayout(behaviourBox);
    m_restoreWindows =
        new QCheckBox(i18nc("@option:check", "Restore the window's screen, virtual desktop, size, tile and keep-above state with each workspace"),
                      behaviourBox);
    m_restoreWindows->setObjectName(QStringLiteral("kwinRestoreWindows"));
    m_restoreWindows->setChecked(options.restoreWindows);
    m_tileCompanions = new QCheckBox(i18nc("@option:check", "Place detached previews, editors and panes beside the main window"), behaviourBox);
    m_tileCompanions->setObjectName(QStringLiteral("kwinTileCompanions"));
    m_tileCompanions->setChecked(options.tileCompanions);
    m_followDesktops = new QCheckBox(i18nc("@option:check", "Apply the workspace assigned to the virtual desktop that becomes current"), behaviourBox);
    m_followDesktops->setObjectName(QStringLiteral("kwinFollowDesktops"));
    m_followDesktops->setChecked(options.followDesktops);
    behaviourLayout->addWidget(m_restoreWindows);
    behaviourLayout->addWidget(m_tileCompanions);
    behaviourLayout->addWidget(m_followDesktops);
    auto *desktopWidget = new QWidget(behaviourBox);
    m_desktopLayout = new QFormLayout(desktopWidget);
    m_desktopLayout->setContentsMargins(0, 0, 0, 0);
    m_noDesktops = new QLabel(i18nc("@info", "Virtual desktops are listed here once the script is running."), behaviourBox);
    m_noDesktops->setWordWrap(true);
    behaviourLayout->addWidget(desktopWidget);
    behaviourLayout->addWidget(m_noDesktops);
    layout->addWidget(behaviourBox);

    const KWinScript::DropDown dropDown = kwin->dropDown();
    auto *dropDownBox = new QGroupBox(i18nc("@title:group", "Drop-Down Window"), this);
    auto *dropDownLayout = new QFormLayout(dropDownBox);
    m_dropDownEnabled = new QCheckBox(i18nc("@option:check", "Slide the window in from a screen edge and hide it again"), dropDownBox);
    m_dropDownEnabled->setObjectName(QStringLiteral("dropDownEnabled"));
    m_dropDownEnabled->setChecked(dropDown.enabled);
    dropDownLayout->addRow(m_dropDownEnabled);
    m_edge = new QComboBox(dropDownBox);
    m_edge->setObjectName(QStringLiteral("dropDownEdge"));
    m_edge->addItem(i18nc("@item:inlistbox screen edge", "Top"), QVariant::fromValue(int(KWinScript::Edge::Top)));
    m_edge->addItem(i18nc("@item:inlistbox screen edge", "Bottom"), QVariant::fromValue(int(KWinScript::Edge::Bottom)));
    m_edge->addItem(i18nc("@item:inlistbox screen edge", "Left"), QVariant::fromValue(int(KWinScript::Edge::Left)));
    m_edge->addItem(i18nc("@item:inlistbox screen edge", "Right"), QVariant::fromValue(int(KWinScript::Edge::Right)));
    m_edge->setCurrentIndex(m_edge->findData(int(dropDown.edge)));
    dropDownLayout->addRow(i18nc("@label:listbox", "Screen edge:"), m_edge);
    m_width = new QSpinBox(dropDownBox);
    m_width->setObjectName(QStringLiteral("dropDownWidth"));
    m_width->setRange(10, 100);
    m_width->setSuffix(i18nc("@item:valuesuffix percentage", "%"));
    m_width->setValue(dropDown.width);
    dropDownLayout->addRow(i18nc("@label:spinbox", "Width:"), m_width);
    m_height = new QSpinBox(dropDownBox);
    m_height->setObjectName(QStringLiteral("dropDownHeight"));
    m_height->setRange(10, 100);
    m_height->setSuffix(i18nc("@item:valuesuffix percentage", "%"));
    m_height->setValue(dropDown.height);
    dropDownLayout->addRow(i18nc("@label:spinbox", "Height:"), m_height);
    m_screen = new QComboBox(dropDownBox);
    m_screen->setObjectName(QStringLiteral("dropDownScreen"));
    m_screen->addItem(i18nc("@item:inlistbox", "Active screen"), QString());
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        m_screen->addItem(screen->name(), screen->name());
    }
    if (!dropDown.screen.isEmpty() && m_screen->findData(dropDown.screen) < 0) {
        m_screen->addItem(dropDown.screen, dropDown.screen);
    }
    m_screen->setCurrentIndex(std::max(0, m_screen->findData(dropDown.screen)));
    dropDownLayout->addRow(i18nc("@label:listbox", "Screen:"), m_screen);
    m_slide = new QCheckBox(i18nc("@option:check", "Slide in and out"), dropDownBox);
    m_slide->setObjectName(QStringLiteral("dropDownSlide"));
    m_slide->setChecked(dropDown.slide);
    dropDownLayout->addRow(i18nc("@label", "Animation:"), m_slide);
    m_hideOnFocusLoss = new QCheckBox(i18nc("@option:check", "Hide when another application is activated"), dropDownBox);
    m_hideOnFocusLoss->setObjectName(QStringLiteral("dropDownHideOnFocusLoss"));
    m_hideOnFocusLoss->setChecked(dropDown.hideOnFocusLoss);
    dropDownLayout->addRow(QString(), m_hideOnFocusLoss);
    m_shortcut = new KKeySequenceWidget(dropDownBox);
    m_shortcut->setObjectName(QStringLiteral("dropDownShortcut"));
    m_shortcut->setCheckForConflictsAgainst(KKeySequenceWidget::GlobalShortcuts | KKeySequenceWidget::StandardShortcuts);
    if (m_dropDownAction) {
        const QList<QKeySequence> current = KGlobalAccel::self()->shortcut(m_dropDownAction);
        m_shortcut->setKeySequence(current.value(0));
    }
    dropDownLayout->addRow(i18nc("@label", "Global shortcut:"), m_shortcut);
    m_screenEdge = new QCheckBox(i18nc("@option:check", "Show or hide when the pointer pushes against the screen edge"), dropDownBox);
    m_screenEdge->setObjectName(QStringLiteral("dropDownScreenEdge"));
    m_screenEdge->setChecked(dropDown.screenEdge);
    dropDownLayout->addRow(i18nc("@label", "Triggers:"), m_screenEdge);
    m_touchEdge = new QCheckBox(i18nc("@option:check", "Show or hide when swiping from the screen edge"), dropDownBox);
    m_touchEdge->setObjectName(QStringLiteral("dropDownTouchEdge"));
    m_touchEdge->setChecked(dropDown.touchEdge);
    dropDownLayout->addRow(QString(), m_touchEdge);
    m_dropDownWorkspace = workspaceCombo(dropDown.workspaceId);
    m_dropDownWorkspace->setObjectName(QStringLiteral("dropDownWorkspace"));
    m_dropDownWorkspace->setItemText(0, i18nc("@item:inlistbox", "Keep its own layout"));
    dropDownLayout->addRow(i18nc("@label:listbox", "Workspace:"), m_dropDownWorkspace);
    layout->addWidget(dropDownBox);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &KWinIntegrationDialog::apply);
    connect(m_install, &QPushButton::clicked, this, [this] {
        m_install->setEnabled(false);
        m_kwin->install();
    });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        if (KMessageBox::questionTwoActions(this,
                                            i18nc("@info", "Remove the Plasma Opus KWin script? Other KWin scripts and settings are not changed."),
                                            i18nc("@title:window", "Remove KWin Script"),
                                            KStandardGuiItem::remove(),
                                            KStandardGuiItem::cancel())
            == KMessageBox::PrimaryAction) {
            m_remove->setEnabled(false);
            m_kwin->remove();
        }
    });
    connect(m_enabled, &QCheckBox::clicked, this, [this](bool checked) {
        m_kwin->setEnabled(checked);
    });
    connect(refresh, &QPushButton::clicked, m_kwin, &KWinIntegration::refresh);
    connect(scripts, &QPushButton::clicked, this, [this] {
        m_kwin->backend()->openSettings(QStringLiteral("kwinscripts"));
    });
    connect(rules, &QPushButton::clicked, this, [this] {
        m_kwin->backend()->openSettings(QStringLiteral("kcm_kwinrules"));
    });
    connect(m_kwin, &KWinIntegration::stateChanged, this, &KWinIntegrationDialog::updateState);
    connect(m_kwin, &KWinIntegration::desktopsChanged, this, &KWinIntegrationDialog::populateDesktops);
    connect(m_kwin, &KWinIntegration::operationFinished, this, [this](const QString &error) {
        if (!error.isEmpty()) {
            KMessageBox::error(this, error, i18nc("@title:window", "KWin Script"));
        }
    });
    connect(m_followDesktops, &QCheckBox::toggled, this, &KWinIntegrationDialog::populateDesktops);
    connect(m_dropDownEnabled, &QCheckBox::toggled, this, &KWinIntegrationDialog::updateDropDownControls);

    updateState();
    populateDesktops();
    updateDropDownControls();
    new KWindowStateSaver(this, QStringLiteral("KWinIntegrationDialog"));
}

QComboBox *KWinIntegrationDialog::workspaceCombo(const QString &selected)
{
    auto *combo = new QComboBox(this);
    combo->addItem(i18nc("@item:inlistbox no workspace", "None"), QString());
    for (const auto &[id, name] : std::as_const(m_workspaces)) {
        combo->addItem(name, id);
    }
    if (!selected.isEmpty() && combo->findData(selected) < 0) {
        combo->addItem(i18nc("@item:inlistbox", "Missing workspace"), selected);
    }
    combo->setCurrentIndex(std::max(0, combo->findData(selected)));
    return combo;
}

void KWinIntegrationDialog::updateState()
{
    const KWinScript::State state = m_kwin->state();
    const KWinScript::Status status = KWinScript::status(state);
    m_status->setText(m_kwin->statusText());
    const bool available = status != KWinScript::Status::KWinUnavailable && status != KWinScript::Status::ScriptingUnavailable;
    const bool installed = state.installedVersion >= 0;
    m_install->setVisible(!installed || state.installedVersion < state.bundledVersion);
    m_install->setText(installed ? i18nc("@action:button", "Update") : i18nc("@action:button", "Install and Enable"));
    m_install->setIcon(QIcon::fromTheme(installed ? QStringLiteral("update-none") : QStringLiteral("download")));
    m_install->setEnabled(available);
    m_enabled->setEnabled(available && installed);
    m_enabled->setChecked(state.enabled);
    m_remove->setEnabled(installed);
}

void KWinIntegrationDialog::populateDesktops()
{
    QMap<QString, QString> assignments;
    for (const auto &[desktop, combo] : std::as_const(m_desktopCombos)) {
        assignments.insert(desktop, combo->currentData().toString());
    }
    if (m_desktopCombos.isEmpty()) {
        assignments = m_kwin->desktopWorkspaces();
    }
    while (m_desktopLayout->rowCount() > 0) {
        m_desktopLayout->removeRow(0);
    }
    m_desktopCombos.clear();
    const QList<KWinScript::Desktop> desktops = m_kwin->desktops();
    for (const KWinScript::Desktop &desktop : desktops) {
        QComboBox *combo = workspaceCombo(assignments.value(desktop.id));
        combo->setObjectName(QStringLiteral("desktopWorkspace_") + desktop.id);
        combo->setEnabled(m_followDesktops->isChecked());
        m_desktopLayout->addRow(i18nc("@label:listbox %1 is a virtual desktop name", "%1:", desktop.name), combo);
        m_desktopCombos.append({desktop.id, combo});
    }
    m_noDesktops->setVisible(desktops.isEmpty());
}

void KWinIntegrationDialog::updateDropDownControls()
{
    const bool enabled = m_dropDownEnabled->isChecked();
    for (QWidget *widget : std::initializer_list<
             QWidget *>{m_edge, m_width, m_height, m_screen, m_slide, m_hideOnFocusLoss, m_shortcut, m_screenEdge, m_touchEdge, m_dropDownWorkspace}) {
        widget->setEnabled(enabled);
    }
}

void KWinIntegrationDialog::apply()
{
    m_kwin->setOptions(
        {.restoreWindows = m_restoreWindows->isChecked(), .tileCompanions = m_tileCompanions->isChecked(), .followDesktops = m_followDesktops->isChecked()});
    QMap<QString, QString> assignments = m_kwin->desktopWorkspaces();
    for (const auto &[desktop, combo] : std::as_const(m_desktopCombos)) {
        assignments.insert(desktop, combo->currentData().toString());
    }
    m_kwin->setDesktopWorkspaces(assignments);
    KWinScript::DropDown dropDown;
    dropDown.enabled = m_dropDownEnabled->isChecked();
    dropDown.edge = KWinScript::Edge(m_edge->currentData().toInt());
    dropDown.width = m_width->value();
    dropDown.height = m_height->value();
    dropDown.screen = m_screen->currentData().toString();
    dropDown.slide = m_slide->isChecked();
    dropDown.hideOnFocusLoss = m_hideOnFocusLoss->isChecked();
    dropDown.screenEdge = m_screenEdge->isChecked();
    dropDown.touchEdge = m_touchEdge->isChecked();
    dropDown.workspaceId = m_dropDownWorkspace->currentData().toString();
    m_kwin->setDropDown(dropDown);
    if (m_dropDownAction) {
        const QKeySequence sequence = m_shortcut->keySequence();
        KGlobalAccel::self()->setShortcut(m_dropDownAction,
                                          sequence.isEmpty() ? QList<QKeySequence>() : QList<QKeySequence>{sequence},
                                          KGlobalAccel::NoAutoloading);
    }
}
