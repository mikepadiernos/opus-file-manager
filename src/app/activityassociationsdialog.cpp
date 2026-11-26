// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityassociationsdialog.h"
#include "activitycontroller.h"
#include "settings.h"

#include <KFile>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KUrlRequester>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{
constexpr int InheritBehaviors = -1;
}

ActivityAssociationsDialog::ActivityAssociationsDialog(ActivityController *controller, const WorkspaceCatalog::Catalog &catalog, QWidget *parent)
    : QDialog(parent)
    , m_controller(controller)
    , m_catalog(catalog)
    , m_associations(controller->associations())
{
    setObjectName(QStringLiteral("activityAssociationsDialog"));
    setWindowTitle(i18nc("@title:window", "Activity Associations"));
    auto *layout = new QVBoxLayout(this);

    m_message = new KMessageWidget(this);
    m_message->setObjectName(QStringLiteral("activityServiceUnavailable"));
    m_message->setMessageType(KMessageWidget::Warning);
    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(false);
    m_message->setText(i18nc("@info",
                             "The Plasma Activities service is not running. Saved associations are kept and can be edited, and apply again once "
                             "the service is back."));
    layout->addWidget(m_message);

    m_enabled = new QCheckBox(i18nc("@option:check", "Associate workspaces and default folders with Plasma Activities"), this);
    m_enabled->setObjectName(QStringLiteral("activityAssociationsEnabled"));
    m_enabled->setChecked(Settings::activityAssociations());
    layout->addWidget(m_enabled);
    m_offer = new QCheckBox(i18nc("@option:check", "Offer to apply an Activity's association when the current Activity changes"), this);
    m_offer->setObjectName(QStringLiteral("activityAssociationsOffer"));
    m_offer->setChecked(Settings::offerActivityAssociation());
    layout->addWidget(m_offer);

    auto *body = new QHBoxLayout;
    m_activities = new QListWidget(this);
    m_activities->setObjectName(QStringLiteral("activityList"));
    m_activities->setAccessibleName(i18nc("@info:accessibility", "Activities"));
    body->addWidget(m_activities, 1);

    auto *editor = new QVBoxLayout;
    auto *form = new QFormLayout;
    m_workspace = new QComboBox(this);
    m_workspace->setObjectName(QStringLiteral("activityWorkspace"));
    form->addRow(i18nc("@label:listbox", "Workspace:"), m_workspace);
    m_behaviorMode = new QComboBox(this);
    m_behaviorMode->setObjectName(QStringLiteral("activityBehaviorMode"));
    m_behaviorMode->addItem(i18nc("@item:inlistbox workspace behaviors", "The workspace's own"), InheritBehaviors);
    m_behaviorMode->addItem(i18nc("@item:inlistbox workspace behaviors", "All"), int(WorkspaceCatalog::BehaviorMode::All));
    m_behaviorMode->addItem(i18nc("@item:inlistbox workspace behaviors", "Selected"), int(WorkspaceCatalog::BehaviorMode::Selected));
    m_behaviorMode->addItem(i18nc("@item:inlistbox workspace behaviors", "None"), int(WorkspaceCatalog::BehaviorMode::None));
    form->addRow(i18nc("@label:listbox", "Behaviors:"), m_behaviorMode);
    editor->addLayout(form);

    auto *behaviorGroup = new QGroupBox(i18nc("@title:group", "Selected Behaviors"), this);
    auto *behaviorLayout = new QVBoxLayout(behaviorGroup);
    const auto addBehavior = [behaviorGroup, behaviorLayout](const QString &name, const QString &text) {
        auto *check = new QCheckBox(text, behaviorGroup);
        check->setObjectName(name);
        behaviorLayout->addWidget(check);
        return check;
    };
    m_locationPresentation = addBehavior(QStringLiteral("activityLocationPresentation"), i18nc("@option:check", "Location presentation"));
    m_searchScope = addBehavior(QStringLiteral("activitySearchScope"), i18nc("@option:check", "Search scope"));
    m_layoutBehavior = addBehavior(QStringLiteral("activityLayoutBehavior"), i18nc("@option:check", "Window and panel layout"));
    m_paneTabRestoration = addBehavior(QStringLiteral("activityPaneTabRestoration"), i18nc("@option:check", "Pane and tab restoration"));
    m_appearanceBehavior = addBehavior(QStringLiteral("activityAppearanceBehavior"), i18nc("@option:check", "Theme profile and appearance"));
    m_yaziBehavior = addBehavior(QStringLiteral("activityYaziModeBehavior"), i18nc("@option:check", "Yazi mode"));
    editor->addWidget(behaviorGroup);

    auto *folders = new QFormLayout;
    const auto addFolder = [this, folders](const QString &name, const QString &label) {
        auto *requester = new KUrlRequester(this);
        requester->setObjectName(name);
        requester->setMode(KFile::Directory | KFile::ExistingOnly);
        requester->setPlaceholderText(i18nc("@info:placeholder", "None"));
        folders->addRow(label, requester);
        return requester;
    };
    m_primaryFolder = addFolder(QStringLiteral("activityPrimaryFolder"), i18nc("@label:chooser", "First pane folder:"));
    m_secondaryFolder = addFolder(QStringLiteral("activitySecondaryFolder"), i18nc("@label:chooser", "Second pane folder:"));
    editor->addLayout(folders);
    auto *foldersNote = new QLabel(i18nc("@info", "Default folders open in new tabs, so the restored tabs stay open."), this);
    foldersNote->setWordWrap(true);
    editor->addWidget(foldersNote);

    auto *clear = new QPushButton(i18nc("@action:button", "Clear Association"), this);
    clear->setObjectName(QStringLiteral("activityClearAssociation"));
    connect(clear, &QPushButton::clicked, this, [this] {
        if (m_formId.isEmpty()) {
            return;
        }
        ActivityAssociations::remove(m_associations, m_formId);
        const QString id = m_formId;
        m_formId.clear();
        populate();
        int selected = m_activities->count() > 0 ? 0 : -1;
        for (int row = 0; row < m_activities->count(); ++row) {
            if (m_activities->item(row)->data(Qt::UserRole).toString() == id) {
                selected = row;
            }
        }
        m_activities->setCurrentRow(selected);
        loadForm(selectedId());
    });
    editor->addWidget(clear);
    editor->addStretch();
    body->addLayout(editor, 2);
    layout->addLayout(body);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_activities, &QListWidget::currentRowChanged, this, [this] {
        storeForm();
        loadForm(selectedId());
    });
    connect(m_enabled, &QCheckBox::toggled, this, &ActivityAssociationsDialog::updateEnabled);
    connect(m_behaviorMode, &QComboBox::currentIndexChanged, this, &ActivityAssociationsDialog::updateEnabled);
    connect(m_workspace, &QComboBox::currentIndexChanged, this, &ActivityAssociationsDialog::updateEnabled);

    populate();
    const QString current = m_controller->source() ? m_controller->source()->currentActivity() : QString();
    for (int row = 0; row < m_activities->count(); ++row) {
        if (m_activities->item(row)->data(Qt::UserRole).toString() == current) {
            m_activities->setCurrentRow(row);
        }
    }
    if (m_activities->currentRow() < 0 && m_activities->count() > 0) {
        m_activities->setCurrentRow(0);
    }
    loadForm(selectedId());
    updateEnabled();
    resize(sizeHint().expandedTo(QSize(720, 480)));
}

void ActivityAssociationsDialog::populate()
{
    const QSignalBlocker blocker(m_activities);
    ActivitySource *source = m_controller->source();
    const bool available = source && source->isAvailable();
    m_message->setVisible(!available);
    m_names = available ? source->activities() : QMap<QString, QString>{};
    const QString current = available ? source->currentActivity() : QString();
    m_activities->clear();
    QStringList ids = m_names.keys();
    for (const auto &association : std::as_const(m_associations)) {
        if (!ids.contains(association.activityId)) {
            ids.append(association.activityId);
        }
    }
    for (const QString &id : std::as_const(ids)) {
        const auto *association = ActivityAssociations::find(m_associations, id);
        QString name = m_names.value(id);
        if (name.isEmpty() && association) {
            name = association->activityName;
        }
        if (name.isEmpty()) {
            name = id;
        }
        QString text = name;
        if (id == current) {
            text = i18nc("@item:inlistbox %1 is an Activity name", "%1 (current)", name);
        } else if (available && !m_names.contains(id)) {
            text = i18nc("@item:inlistbox %1 is an Activity name", "%1 (removed)", name);
        }
        auto *item = new QListWidgetItem(text, m_activities);
        item->setData(Qt::UserRole, id);
        item->setToolTip(association && !association->isEmpty() ? i18nc("@info:tooltip", "Associated") : i18nc("@info:tooltip", "Not associated"));
    }
}

QString ActivityAssociationsDialog::selectedId() const
{
    const QListWidgetItem *item = m_activities->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ActivityAssociationsDialog::loadForm(const QString &activityId)
{
    m_formId = activityId;
    const auto *found = ActivityAssociations::find(m_associations, activityId);
    const ActivityAssociations::Association association = found ? *found : ActivityAssociations::Association{};
    const QSignalBlocker workspaceBlocker(m_workspace);
    const QSignalBlocker modeBlocker(m_behaviorMode);
    m_workspace->clear();
    m_workspace->addItem(i18nc("@item:inlistbox", "No workspace"), QString());
    for (const auto &workspace : std::as_const(m_catalog.workspaces)) {
        m_workspace->addItem(workspace.name, workspace.id);
    }
    if (!association.workspaceId.isEmpty() && m_workspace->findData(association.workspaceId) < 0) {
        // Keep a deleted workspace's reference until it is changed here.
        m_workspace->addItem(i18nc("@item:inlistbox", "Missing workspace"), association.workspaceId);
    }
    m_workspace->setCurrentIndex(qMax(0, m_workspace->findData(association.workspaceId)));
    m_behaviorMode->setCurrentIndex(m_behaviorMode->findData(association.inheritBehaviors ? InheritBehaviors : int(association.behaviors.mode)));
    m_locationPresentation->setChecked(association.behaviors.locationPresentation);
    m_searchScope->setChecked(association.behaviors.searchScope);
    m_layoutBehavior->setChecked(association.behaviors.layout);
    m_paneTabRestoration->setChecked(association.behaviors.paneTabRestoration);
    m_appearanceBehavior->setChecked(association.behaviors.appearance);
    m_yaziBehavior->setChecked(association.behaviors.yaziMode.value_or(association.behaviors.layout));
    m_primaryFolder->setUrl(association.primaryFolder);
    m_secondaryFolder->setUrl(association.secondaryFolder);
    updateEnabled();
}

void ActivityAssociationsDialog::storeForm()
{
    if (m_formId.isEmpty()) {
        return;
    }
    const auto *found = ActivityAssociations::find(m_associations, m_formId);
    ActivityAssociations::Association association = found ? *found : ActivityAssociations::Association{};
    association.activityId = m_formId;
    if (m_names.contains(m_formId)) {
        association.activityName = m_names.value(m_formId);
        association.removed = false;
    }
    association.workspaceId = m_workspace->currentData().toString();
    const int mode = m_behaviorMode->currentData().toInt();
    association.inheritBehaviors = mode == InheritBehaviors;
    association.behaviors = {mode == InheritBehaviors ? WorkspaceCatalog::BehaviorMode::All : static_cast<WorkspaceCatalog::BehaviorMode>(mode),
                             m_locationPresentation->isChecked(),
                             m_searchScope->isChecked(),
                             m_layoutBehavior->isChecked(),
                             m_paneTabRestoration->isChecked(),
                             m_appearanceBehavior->isChecked(),
                             m_yaziBehavior->isChecked()};
    association.primaryFolder = m_primaryFolder->url().adjusted(QUrl::NormalizePathSegments);
    association.secondaryFolder = m_secondaryFolder->url().adjusted(QUrl::NormalizePathSegments);
    QString error;
    if (!ActivityAssociations::set(m_associations, association, error)) {
        m_message->setText(error);
        m_message->setMessageType(KMessageWidget::Error);
        m_message->show();
    }
}

QList<ActivityAssociations::Association> ActivityAssociationsDialog::associations()
{
    storeForm();
    return m_associations;
}

void ActivityAssociationsDialog::updateEnabled()
{
    const bool enabled = m_enabled->isChecked();
    m_offer->setEnabled(enabled);
    const bool selected = enabled && !m_formId.isEmpty();
    m_activities->setEnabled(enabled);
    for (QWidget *widget : std::initializer_list<QWidget *>{m_workspace, m_behaviorMode, m_primaryFolder, m_secondaryFolder}) {
        widget->setEnabled(selected);
    }
    const bool behaviors =
        selected && !m_workspace->currentData().toString().isEmpty() && m_behaviorMode->currentData().toInt() == int(WorkspaceCatalog::BehaviorMode::Selected);
    for (QCheckBox *check : {m_locationPresentation, m_searchScope, m_layoutBehavior, m_paneTabRestoration, m_appearanceBehavior, m_yaziBehavior}) {
        check->setEnabled(behaviors);
    }
}

void ActivityAssociationsDialog::accept()
{
    const QList<ActivityAssociations::Association> edited = associations();
    Settings::setActivityAssociations(m_enabled->isChecked());
    Settings::setOfferActivityAssociation(m_offer->isChecked());
    Settings::self()->save();
    m_controller->setAssociations(edited);
    m_controller->applySettings();
    QDialog::accept();
}
