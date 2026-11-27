// SPDX-License-Identifier: GPL-3.0-or-later

#include "activityassociationseditor.h"
#include "activitycontroller.h"

#include <KFile>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KUrlRequester>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{
constexpr int InheritBehaviors = -1;
}

ActivityAssociationsEditor::ActivityAssociationsEditor(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("activityAssociationsEditor"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins({});

    m_message = new KMessageWidget(this);
    m_message->setObjectName(QStringLiteral("activityServiceUnavailable"));
    m_message->setMessageType(KMessageWidget::Warning);
    m_message->setWordWrap(true);
    m_message->setCloseButtonVisible(false);
    m_message->setText(i18nc("@info",
                             "The Plasma Activities service is not running. Saved associations are kept and can be edited, and apply again once "
                             "the service is back."));
    layout->addWidget(m_message);
    m_error = new KMessageWidget(this);
    m_error->setObjectName(QStringLiteral("activityAssociationError"));
    m_error->setMessageType(KMessageWidget::Error);
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);

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
    m_behaviorMode->setToolTip(i18nc("@info:tooltip",
                                     "The workspace behaviors to use when switching to the workspace for this Activity: those the workspace "
                                     "itself uses, or all, selected or none of them"));
    form->addRow(i18nc("@label:listbox", "Behaviors:"), m_behaviorMode);
    editor->addLayout(form);

    auto *behaviorGroup = new QGroupBox(i18nc("@title:group", "Selected Behaviors"), this);
    auto *behaviorLayout = new QVBoxLayout(behaviorGroup);
    const auto addBehavior = [this, behaviorGroup, behaviorLayout](const QString &name, const QString &text) {
        auto *check = new QCheckBox(text, behaviorGroup);
        check->setObjectName(name);
        behaviorLayout->addWidget(check);
        connect(check, &QCheckBox::toggled, this, &ActivityAssociationsEditor::edited);
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
        connect(requester, &KUrlRequester::textChanged, this, &ActivityAssociationsEditor::edited);
        return requester;
    };
    m_primaryFolder = addFolder(QStringLiteral("activityPrimaryFolder"), i18nc("@label:chooser", "First pane folder:"));
    m_secondaryFolder = addFolder(QStringLiteral("activitySecondaryFolder"), i18nc("@label:chooser", "Second pane folder:"));
    editor->addLayout(folders);
    auto *foldersNote = new QLabel(i18nc("@info", "Default folders open in new tabs, so the restored tabs stay open."), this);
    foldersNote->setWordWrap(true);
    editor->addWidget(foldersNote);

    m_clear = new QPushButton(i18nc("@action:button", "Clear Association"), this);
    m_clear->setObjectName(QStringLiteral("activityClearAssociation"));
    connect(m_clear, &QPushButton::clicked, this, [this] {
        if (m_formId.isEmpty()) {
            return;
        }
        ActivityAssociations::remove(m_associations, m_formId);
        const QString id = m_formId;
        m_formId.clear();
        populate();
        select(id);
        Q_EMIT changed();
    });
    editor->addWidget(m_clear);
    editor->addStretch();
    body->addLayout(editor, 2);
    layout->addLayout(body);

    connect(m_activities, &QListWidget::currentRowChanged, this, [this] {
        storeForm();
        loadForm(selectedId());
    });
    connect(m_behaviorMode, &QComboBox::currentIndexChanged, this, &ActivityAssociationsEditor::edited);
    connect(m_workspace, &QComboBox::currentIndexChanged, this, &ActivityAssociationsEditor::edited);

    populate();
    select({});
}

void ActivityAssociationsEditor::setSource(ActivitySource *source)
{
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
    }
    m_source = source;
    if (m_source) {
        connect(m_source, &ActivitySource::availabilityChanged, this, &ActivityAssociationsEditor::refresh);
        connect(m_source, &ActivitySource::activitiesChanged, this, &ActivityAssociationsEditor::refresh);
    }
    refresh();
}

void ActivityAssociationsEditor::setCatalog(const WorkspaceCatalog::Catalog &catalog)
{
    storeForm();
    m_catalog = catalog;
    loadForm(m_formId);
}

void ActivityAssociationsEditor::setAssociations(const QList<ActivityAssociations::Association> &associations)
{
    m_associations = associations;
    m_formId.clear();
    m_error->hide();
    populate();
    select({});
}

void ActivityAssociationsEditor::setEditable(bool editable)
{
    m_editable = editable;
    updateEnabled();
}

void ActivityAssociationsEditor::refresh()
{
    storeForm();
    const QString id = m_formId;
    m_formId.clear();
    populate();
    select(id);
}

void ActivityAssociationsEditor::populate()
{
    const QSignalBlocker blocker(m_activities);
    const bool available = m_source && m_source->isAvailable();
    m_message->setVisible(!available);
    m_names = available ? m_source->activities() : QMap<QString, QString>{};
    const QString current = available ? m_source->currentActivity() : QString();
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

void ActivityAssociationsEditor::select(const QString &activityId)
{
    const QString current = m_source && m_source->isAvailable() ? m_source->currentActivity() : QString();
    int requested = -1;
    int currentRow = -1;
    for (int row = 0; row < m_activities->count(); ++row) {
        const QString id = m_activities->item(row)->data(Qt::UserRole).toString();
        if (!activityId.isEmpty() && id == activityId) {
            requested = row;
        }
        if (!current.isEmpty() && id == current) {
            currentRow = row;
        }
    }
    if (requested < 0) {
        requested = currentRow >= 0 ? currentRow : (m_activities->count() > 0 ? 0 : -1);
    }
    {
        const QSignalBlocker blocker(m_activities);
        m_activities->setCurrentRow(requested);
    }
    loadForm(selectedId());
}

QString ActivityAssociationsEditor::selectedId() const
{
    const QListWidgetItem *item = m_activities->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ActivityAssociationsEditor::loadForm(const QString &activityId)
{
    const QScopedValueRollback loading(m_loading, true);
    m_formId = activityId;
    const auto *found = ActivityAssociations::find(m_associations, activityId);
    const ActivityAssociations::Association association = found ? *found : ActivityAssociations::Association{};
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

void ActivityAssociationsEditor::storeForm()
{
    if (m_formId.isEmpty() || m_loading) {
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
    if (ActivityAssociations::set(m_associations, association, error)) {
        m_error->hide();
    } else {
        m_error->setText(error);
        m_error->show();
    }
}

QList<ActivityAssociations::Association> ActivityAssociationsEditor::associations()
{
    storeForm();
    return m_associations;
}

void ActivityAssociationsEditor::edited()
{
    if (m_loading) {
        return;
    }
    updateEnabled();
    Q_EMIT changed();
}

void ActivityAssociationsEditor::updateEnabled()
{
    const bool selected = m_editable && !m_formId.isEmpty();
    m_activities->setEnabled(m_editable);
    for (QWidget *widget : std::initializer_list<QWidget *>{m_workspace, m_behaviorMode, m_primaryFolder, m_secondaryFolder}) {
        widget->setEnabled(selected);
    }
    const bool behaviors =
        selected && !m_workspace->currentData().toString().isEmpty() && m_behaviorMode->currentData().toInt() == int(WorkspaceCatalog::BehaviorMode::Selected);
    for (QCheckBox *check : {m_locationPresentation, m_searchScope, m_layoutBehavior, m_paneTabRestoration, m_appearanceBehavior, m_yaziBehavior}) {
        check->setEnabled(behaviors);
    }
    m_clear->setEnabled(selected);
}
